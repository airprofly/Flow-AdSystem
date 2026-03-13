#!/usr/bin/env python3
"""
一键生成模拟数据并训练 CTR 预估模型
简化版本,无需外部数据下载
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import numpy as np
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

class SyntheticCTRDataset(Dataset):
    """模拟 CTR 数据集"""
    def __init__(self, num_samples=100000, seed=42):
        np.random.seed(seed)
        torch.manual_seed(seed)

        self.num_samples = num_samples

        # 生成标签 - 模拟约 25% 的 CTR
        self.labels = torch.from_numpy(
            np.random.binomial(1, 0.25, num_samples).astype(np.float32)
        )

        # 生成连续特征 (8个) - 与 C++ 实现一致
        # 广告特征 (3个): hist_ctr, hist_cvr, position
        # 用户特征 (3个): age_norm, history_ctr, history_impressions_norm
        # 上下文特征 (2个): hour_norm, time_slot
        dense_features = []
        for i in range(8):
            if i < 3:
                # 广告特征: 正态分布
                feat = np.random.randn(num_samples).astype(np.float32) * 0.1 + 0.05
            elif i < 6:
                # 用户特征: 均匀分布
                feat = np.random.uniform(0, 1, num_samples).astype(np.float32)
            else:
                # 上下文特征: 指数分布
                feat = np.random.exponential(0.3, num_samples).astype(np.float32)

            # 标准化到 [0, 1]
            mean = feat.mean()
            std = feat.std()
            if std > 0:
                feat = (feat - mean) / std
            dense_features.append(feat)

        self.dense_features = torch.from_numpy(np.stack(dense_features, axis=1))

        # 生成稀疏特征 (13个) - 与 C++ 实现完全一致
        # 注意: 特征顺序必须与 C++ 代码中的顺序完全匹配!
        cardinalities = [
            # 广告特征 (5个) - 与 C++ AdFeatureExtractor 一致
            1000000,  # ad_id: Hash 编码,基数 1000000
            100000,   # campaign_id: Hash 编码,基数 100000
            1000,     # industry_id: 行业类别,基数 1000
            100,      # creative_type: 创意类型,基数 100
            50,       # ad_size: 广告尺寸,基数 50
            # 用户特征 (3个) - 与 C++ UserFeatureExtractor 一致
            4,        # user_gender: 性别 M/F/O/U,基数 4
            10000,    # user_city: 城市ID,基数 10000 (与 C++ 一致)
            10,       # user_device_type: 设备类型,基数 10
            # 上下文特征 (5个) - 与 C++ ContextFeatureExtractor 一致
            7,        # day_of_week: 星期几,基数 7
            2,        # is_weekend: 是否周末,基数 2
            1000,     # page_category: 页面类别,基数 1000
            10,       # network_type: 网络类型,基数 10
            100       # carrier: 运营商,基数 100
        ]

        sparse_features = []
        self.field_dims = []
        for cardinality in cardinalities:
            feat = np.random.randint(0, cardinality, num_samples)
            sparse_features.append(feat)
            self.field_dims.append(cardinality)

        self.sparse_features = torch.from_numpy(
            np.stack(sparse_features, axis=1)
        ).long()

    def __len__(self):
        return self.num_samples

    def __getitem__(self, idx):
        return (
            self.sparse_features[idx],
            self.dense_features[idx],
            self.labels[idx]
        )


def collate_fn(batch):
    """自定义批处理函数"""
    sparse_x, dense_x, labels = zip(*batch)
    return (
        torch.stack(sparse_x, dim=0),
        torch.stack(dense_x, dim=0),
        torch.stack(labels, dim=0)
    )


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
            dense_x = dense_x.to(device)
            labels = labels.to(device)

            predictions = model(sparse_x, dense_x)
            loss = criterion(predictions, labels)

            total_loss += loss.item()
            all_preds.extend(predictions.cpu().numpy())
            all_labels.extend(labels.cpu().numpy())

    avg_loss = total_loss / len(dataloader)
    auc = roc_auc_score(all_labels, all_preds)
    logloss_value = log_loss(all_labels, all_preds)

    return avg_loss, auc, logloss_value


# ==================== 主函数 ====================

def main():
    parser = argparse.ArgumentParser(description='使用模拟数据训练 DeepFM CTR 模型')

    # 数据参数
    parser.add_argument('--num-samples', type=int, default=100000,
                       help='生成样本数量 (默认: 10万)')
    parser.add_argument('--val-ratio', type=float, default=0.2,
                       help='验证集比例 (默认: 0.2)')

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
    parser.add_argument('--num-workers', type=int, default=0,
                       help='数据加载线程数')

    # 输出参数
    parser.add_argument('--output-dir', type=str,
                       default='checkpoints',
                       help='模型输出目录')

    args = parser.parse_args()

    print("=" * 70)
    print("🚀 DeepFM CTR 预估模型训练 (模拟数据)")
    print("=" * 70)

    # 设备配置
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"\n📱 设备: {device}")
    if torch.cuda.is_available():
        print(f"   GPU: {torch.cuda.get_device_name(0)}")
        print(f"   显存: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.2f} GB")

    # 生成模拟数据
    print(f"\n📊 生成模拟 CTR 数据...")
    print(f"   训练样本: {int(args.num_samples * (1 - args.val_ratio)):,}")
    print(f"   验证样本: {int(args.num_samples * args.val_ratio):,}")

    # 创建训练集和验证集
    train_dataset = SyntheticCTRDataset(
        num_samples=int(args.num_samples * (1 - args.val_ratio)),
        seed=42
    )

    val_dataset = SyntheticCTRDataset(
        num_samples=int(args.num_samples * args.val_ratio),
        seed=100  # 不同的种子
    )

    # 获取特征维度
    field_dims = train_dataset.field_dims
    # 从数据集获取连续特征数量
    num_dense_features = train_dataset.dense_features.shape[1]

    print(f"\n   稀疏特征维度: {len(field_dims)} 个特征域")
    print(f"   连续特征数量: {num_dense_features}")
    print(f"   类别基数范围: {min(field_dims):,} - {max(field_dims):,}")

    # 数据加载器
    train_loader = DataLoader(
        train_dataset,
        batch_size=args.batch_size,
        shuffle=True,
        num_workers=args.num_workers,
        pin_memory=True if torch.cuda.is_available() else False,
        collate_fn=collate_fn
    )

    val_loader = DataLoader(
        val_dataset,
        batch_size=args.batch_size,
        shuffle=False,
        num_workers=args.num_workers,
        pin_memory=True if torch.cuda.is_available() else False,
        collate_fn=collate_fn
    )

    print(f"   批次大小: {args.batch_size}")
    print(f"   训练批次数: {len(train_loader)}")

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

    print("\n💡 提示:")
    print("   - 这是使用模拟数据的训练,主要用于验证流程")
    print("   - 实际应用请使用真实广告数据集 (如 Criteo)")
    print("   - 可以增加 --num-samples 和 --epochs 来提升性能")


if __name__ == "__main__":
    main()
