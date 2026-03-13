"""
CTR 预估模型训练脚本 - DeepFM
支持 Criteo 数据集的端到端训练
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
import pickle
import argparse
from pathlib import Path
from tqdm import tqdm
from sklearn.metrics import roc_auc_score, log_loss
import warnings
warnings.filterwarnings('ignore')


# ==================== 模型定义 ====================

class FeaturesEmbedding(nn.Module):
    """稀疏特征嵌入层"""
    def __init__(self, field_dims, embed_dim):
        super().__init__()
        self.embedding = nn.ModuleList([
            nn.Embedding(dim, embed_dim) for dim in field_dims
        ])
        for embedding in self.embedding:
            nn.init.xavier_uniform_(embedding.weight.data)

    def forward(self, x):
        """
        Args:
            x: (batch_size, num_fields) 稀疏特征索引
        Returns:
            (batch_size, num_fields, embed_dim) 嵌入向量
        """
        return torch.stack([self.embedding[i](x[:, i])
                           for i in range(x.shape[1])], dim=1)


class FeaturesLinear(nn.Module):
    """FM 一阶线性部分"""
    def __init__(self, field_dims):
        super().__init__()
        self.fc = nn.Embedding(sum(field_dims), 1)
        self.bias = nn.Parameter(torch.zeros((1,)))
        self.register_buffer('offsets', torch.tensor(self._get_offsets(field_dims)))

    def _get_offsets(self, field_dims):
        offsets = [0]
        for dim in field_dims[:-1]:
            offsets.append(offsets[-1] + dim)
        return offsets

    def forward(self, x):
        x = x + self.offsets.unsqueeze(0)
        return torch.sum(self.fc(x), dim=1) + self.bias


class FactorizationMachine(nn.Module):
    """FM 二阶交叉部分"""
    def forward(self, x):
        """
        Args:
            x: (batch_size, num_fields, embed_dim)
        Returns:
            (batch_size, 1)
        """
        square_of_sum = torch.sum(x, dim=1) ** 2
        sum_of_square = torch.sum(x ** 2, dim=1)
        ix = square_of_sum - sum_of_square
        return torch.sum(ix, dim=1, keepdim=True) * 0.5


class MultiLayerPerceptron(nn.Module):
    """深度神经网络"""
    def __init__(self, input_dim, embed_dims, dropout=0.2):
        super().__init__()
        layers = []
        for embed_dim in embed_dims:
            layers.append(nn.Linear(input_dim, embed_dim))
            layers.append(nn.BatchNorm1d(embed_dim))
            layers.append(nn.ReLU())
            layers.append(nn.Dropout(dropout))
            input_dim = embed_dim
        layers.append(nn.Linear(embed_dims[-1], 1))
        self.mlp = nn.Sequential(*layers)

    def forward(self, x):
        return self.mlp(x)


class DeepFM(nn.Module):
    """DeepFM: FM + Deep Neural Network"""
    def __init__(self, field_dims, embed_dim=32, mlp_dims=[256, 128, 64],
                 dropout=0.2, num_dense_features=0):
        super().__init__()

        # 稀疏特征处理
        self.embedding = FeaturesEmbedding(field_dims, embed_dim)
        self.linear = FeaturesLinear(field_dims)
        self.fm = FactorizationMachine()

        # 连续特征
        self.num_dense_features = num_dense_features

        # Deep 部分
        deep_input_dim = len(field_dims) * embed_dim + num_dense_features
        self.mlp = MultiLayerPerceptron(deep_input_dim, mlp_dims, dropout)

    def forward(self, sparse_x, dense_x=None):
        """
        Args:
            sparse_x: (batch_size, num_fields) 稀疏特征
            dense_x: (batch_size, num_dense) 连续特征
        Returns:
            (batch_size,) CTR 预估值
        """
        # 特征嵌入
        embed_x = self.embedding(sparse_x)

        # FM 部分
        linear_y = self.linear(sparse_x)
        fm_y = self.fm(embed_x)

        # Deep 部分
        deep_input = embed_x.view(embed_x.size(0), -1)
        if dense_x is not None and self.num_dense_features > 0:
            deep_input = torch.cat([deep_input, dense_x], dim=1)
        deep_y = self.mlp(deep_input)

        # 组合输出
        y = linear_y + fm_y + deep_y
        return torch.sigmoid(y.squeeze(1))


# ==================== 数据集 ====================

class CTRDataset(Dataset):
    """CTR 预估数据集"""
    def __init__(self, data_path):
        data = np.load(data_path)
        self.sparse_features = torch.from_numpy(data['sparse_features']).long()
        self.dense_features = torch.from_numpy(data['dense_features']).float()
        self.labels = torch.from_numpy(data['labels']).float()

        # 检查是否有连续特征
        self.has_dense = self.dense_features.shape[1] > 0

    def __len__(self):
        return len(self.labels)

    def __getitem__(self, idx):
        if self.has_dense:
            return self.sparse_features[idx], self.dense_features[idx], self.labels[idx]
        return self.sparse_features[idx], torch.zeros(0), self.labels[idx]


def collate_fn(batch):
    """自定义批处理函数,处理可变长度的 dense_features"""
    sparse_x, dense_x, labels = zip(*batch)
    sparse_x = torch.stack(sparse_x, dim=0)

    # 检查是否有 dense_features
    if dense_x[0].numel() > 0:
        dense_x = torch.stack(dense_x, dim=0)
    else:
        dense_x = None

    labels = torch.stack(labels, dim=0)
    return sparse_x, dense_x, labels


# ==================== 训练与评估 ====================

def train_epoch(model, dataloader, criterion, optimizer, device, epoch):
    """训练一个 epoch"""
    model.train()
    total_loss = 0
    all_preds = []
    all_labels = []

    pbar = tqdm(dataloader, desc=f'Epoch {epoch}')
    for sparse_x, dense_x, labels in pbar:
        sparse_x = sparse_x.to(device)
        if dense_x is not None:
            dense_x = dense_x.to(device)
        labels = labels.to(device)

        optimizer.zero_grad()
        predictions = model(sparse_x, dense_x)
        loss = criterion(predictions, labels)
        loss.backward()

        # 梯度裁剪
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
        optimizer.step()

        total_loss += loss.item()
        all_preds.extend(predictions.detach().cpu().numpy())
        all_labels.extend(labels.cpu().numpy())

        pbar.set_postfix({'loss': f'{loss.item():.4f}'})

    avg_loss = total_loss / len(dataloader)
    auc = roc_auc_score(all_labels, all_preds)
    return avg_loss, auc


def evaluate(model, dataloader, criterion, device):
    """评估模型"""
    model.eval()
    total_loss = 0
    all_preds = []
    all_labels = []

    with torch.no_grad():
        for sparse_x, dense_x, labels in dataloader:
            sparse_x = sparse_x.to(device)
            if dense_x is not None:
                dense_x = dense_x.to(device)
            labels = labels.to(device)

            predictions = model(sparse_x, dense_x)
            loss = criterion(predictions, labels)

            total_loss += loss.item()
            all_preds.extend(predictions.cpu().numpy())
            all_labels.extend(labels.cpu().numpy())

    avg_loss = total_loss / len(dataloader)
    auc = roc_auc_score(all_labels, all_preds)
    logloss = log_loss(all_labels, all_preds)

    return avg_loss, auc, logloss


def main():
    parser = argparse.ArgumentParser(description='DeepFM CTR 预估模型训练')

    # 数据参数
    parser.add_argument('--train-data', type=str,
                       default='data/train_data.npz',
                       help='训练数据路径')
    parser.add_argument('--val-data', type=str,
                       default='data/val_data.npz',
                       help='验证数据路径')
    parser.add_argument('--encoder', type=str,
                       default='data/feature_encoder.pkl',
                       help='特征编码器路径')

    # 模型参数
    parser.add_argument('--embed-dim', type=int, default=32,
                       help='嵌入维度')
    parser.add_argument('--mlp-dims', type=int, nargs='+', default=[256, 128, 64],
                       help='MLP 隐藏层维度')
    parser.add_argument('--dropout', type=float, default=0.2,
                       help='Dropout 比例')

    # 训练参数
    parser.add_argument('--batch-size', type=int, default=2048,
                       help='批次大小')
    parser.add_argument('--epochs', type=int, default=10,
                       help='训练轮数')
    parser.add_argument('--learning-rate', type=float, default=1e-3,
                       help='学习率')
    parser.add_argument('--weight-decay', type=float, default=1e-5,
                       help='权重衰减')
    parser.add_argument('--num-workers', type=int, default=4,
                       help='数据加载线程数')

    # 输出参数
    parser.add_argument('--output-dir', type=str,
                       default='checkpoints',
                       help='模型输出目录')

    args = parser.parse_args()

    print("=" * 70)
    print("🚀 DeepFM CTR 预估模型训练")
    print("=" * 70)

    # 设备配置
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"\n📱 设备: {device}")
    if torch.cuda.is_available():
        print(f"   GPU: {torch.cuda.get_device_name(0)}")
        print(f"   显存: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.2f} GB")

    # 加载特征编码器
    print(f"\n📂 加载特征编码器: {args.encoder}")
    with open(args.encoder, 'rb') as f:
        encoder_data = pickle.load(f)

    sparse_stats = encoder_data['sparse_stats']
    field_dims = list(sparse_stats.values())

    # 检测连续特征数量
    num_dense_features = len(encoder_data.get('numerical_features', []))

    print(f"   稀疏特征维度: {field_dims}")
    print(f"   连续特征数量: {num_dense_features}")
    print(f"   总特征域: {len(field_dims)}")

    # 创建模型
    print("\n🔨 构建 DeepFM 模型...")
    model = DeepFM(
        field_dims=field_dims,
        embed_dim=args.embed_dim,
        mlp_dims=args.mlp_dims,
        dropout=args.dropout,
        num_dense_features=num_dense_features
    ).to(device)

    # 计算参数量
    total_params = sum(p.numel() for p in model.parameters())
    trainable_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print(f"   总参数量: {total_params:,}")
    print(f"   可训练参数: {trainable_params:,}")

    # 损失函数和优化器
    criterion = nn.BCELoss()
    optimizer = optim.Adam(
        model.parameters(),
        lr=args.learning_rate,
        weight_decay=args.weight_decay
    )

    # 学习率调度器
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(
        optimizer,
        mode='min',
        factor=0.5,
        patience=2
    )

    # 数据加载
    print("\n📊 加载数据...")
    train_dataset = CTRDataset(args.train_data)
    val_dataset = CTRDataset(args.val_data)

    train_loader = DataLoader(
        train_dataset,
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=True,
        collate_fn=collate_fn
    )

    val_loader = DataLoader(
        val_dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=True,
        collate_fn=collate_fn
    )

    print(f"   训练样本: {len(train_dataset):,}")
    print(f"   验证样本: {len(val_dataset):,}")
    print(f"   批次大小: {args.batch_size}")
    print(f"   训练批次数: {len(train_loader)}")

    # 创建输出目录
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # 训练循环
    print("\n" + "=" * 70)
    print("🏋️  开始训练")
    print("=" * 70)

    best_auc = 0.0
    best_loss = float('inf')
    patience_counter = 0
    patience = 5

    for epoch in range(1, args.epochs + 1):
        print(f"\n{'='*70}")
        print(f"Epoch {epoch}/{args.epochs}")
        print(f"{'='*70}")

        # 训练
        train_loss, train_auc = train_epoch(
            model, train_loader, criterion, optimizer, device, epoch
        )

        # 验证
        val_loss, val_auc, val_logloss = evaluate(
            model, val_loader, criterion, device
        )

        # 学习率调度
        scheduler.step(val_loss)

        # 打印指标
        print(f"\n📊 训练 - Loss: {train_loss:.4f}, AUC: {train_auc:.4f}")
        print(f"📊 验证 - Loss: {val_loss:.4f}, AUC: {val_auc:.4f}, LogLoss: {val_logloss:.4f}")
        print(f"📈 当前学习率: {optimizer.param_groups[0]['lr']:.2e}")

        # 保存最佳模型
        if val_auc > best_auc:
            best_auc = val_auc
            best_loss = val_loss
            patience_counter = 0

            checkpoint = {
                'epoch': epoch,
                'model_state_dict': model.state_dict(),
                'optimizer_state_dict': optimizer.state_dict(),
                'val_auc': val_auc,
                'val_loss': val_loss,
                'field_dims': field_dims,
                'num_dense_features': num_dense_features,
                'embed_dim': args.embed_dim,
                'mlp_dims': args.mlp_dims,
                'dropout': args.dropout
            }

            checkpoint_path = output_dir / 'best_model.pt'
            torch.save(checkpoint, checkpoint_path)
            print(f"💾 保存最佳模型: {checkpoint_path}")
            print(f"   🏆 最佳验证 AUC: {best_auc:.4f}")
        else:
            patience_counter += 1

        # Early stopping
        if patience_counter >= patience:
            print(f"\n⏸️  早停触发: 验证性能 {patience} 轮未提升")
            break

    # 训练完成
    print("\n" + "=" * 70)
    print("✅ 训练完成!")
    print("=" * 70)
    print(f"\n🏆 最终最佳验证 AUC: {best_auc:.4f}")
    print(f"📉 最终最佳验证 Loss: {best_loss:.4f}")
    print(f"💾 模型保存位置: {output_dir / 'best_model.pt'}")


if __name__ == "__main__":
    main()
