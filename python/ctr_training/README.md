# DeepFM CTR 预估模型训练

基于 DeepFM (Factorization Machine + Deep Neural Network) 的 CTR (Click-Through Rate) 预估模型实现,使用 **Criteo 真实广告数据集**进行训练。

## 🎯 数据集说明

本训练脚本使用 **Criteo Display Advertising Challenge** 数据集,这是广告点击率预测的**行业标准基准数据集**:

- **数据规模**: 45M+ 样本
- **特征维度**: 13个连续特征 + 26个稀疏特征
- **真实 CTR**: ~26% (正样本比例)
- **数据来源**: [Kaggle Criteo Challenge](https://www.kaggle.com/c/criteo-display-ad-challenge)

### 为什么选择 Criteo?

✅ **真实性**: 来自真实广告系统的展示日志
✅ **行业标准**: 学术界和工业界广泛使用的基准
✅ **完美匹配**: 特征维度与广告系统场景完全对齐
✅ **可对比**: 有大量公开的基准结果(AUC 0.75-0.80)

---

## 🚀 快速开始

### 前置要求

1. **安装依赖**:
```bash
pip install kaggle pandas torch torchvision scikit-learn numpy tqdm
```

2. **配置 Kaggle API** (自动下载数据):
```bash
# 访问 https://www.kaggle.com/settings
# 下载 kaggle.json 并配置
mkdir -p ~/.kaggle
mv ~/Downloads/kaggle.json ~/.kaggle/
chmod 600 ~/.kaggle/kaggle.json
```

或者手动下载数据到 `data/` 目录:
- 数据来源: https://www.kaggle.com/c/criteo-display-ad-challenge/data
- 下载 `train.csv` 或 `dac.tar.gz`

### 一键训练

```bash
# 激活环境
conda activate pytorch

# 自动下载 Criteo 数据并训练 (100万样本)
./quickstart.sh
```

脚本会自动:
1. 从 Kaggle 下载 Criteo 数据集 (或使用本地已有数据)
2. 数据预处理 (特征编码/归一化)
3. 训练 DeepFM 模型
4. 保存最佳模型到 `checkpoints/best_model.pt`

### 手动训练

```bash
# 步骤1: 准备数据
cd data
python3 prepare_data.py
cd ..

# 步骤2: 训练模型
python3 train.py \
    --train-data data/train_data.npz \
    --val-data data/val_data.npz \
    --encoder data/feature_encoder.pkl \
    --batch-size 8192 \
    --epochs 20 \
    --embed-dim 32
```

---

## 🏗️ 模型架构

**DeepFM** 结合了两个组件:

1. **FM (Factorization Machine)**:
   - 一阶线性部分: 捕捉单个特征的重要性
   - 二阶交叉部分: 捕捉特征间的二阶交互

2. **Deep Neural Network**:
   - 多层感知机: 捕捉高阶非线性特征交互

```
输入特征 (13 dense + 26 sparse)
    ↓
嵌入层 (Embedding Layer) ←─────┐
    ↓                         │
    ├─────────┬─────────┐     │
    ↓         ↓         ↓     │
FM 一阶    FM 二阶    Deep    │
(线性)     (特征交叉) (MLP)   │
    │         │         │     │
    └─────────┴─────────┘     │
              ↓               │
         组合输出              │
              ↓               │
         Sigmoid              │
              ↓               │
         CTR 概率             │
                           共享嵌入
```

---

## ⚙️ 训练参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--batch-size` | 2048 | 批次大小 (GPU 内存允许时可增大到 8192) |
| `--epochs` | 10 | 训练轮数 (Criteo 建议 20-50) |
| `--embed-dim` | 32 | 嵌入维度 (可增加到 64/128) |
| `--mlp-dims` | [256, 128, 64] | MLP 隐藏层维度 |
| `--dropout` | 0.2 | Dropout 比例 |
| `--learning-rate` | 1e-3 | 学习率 |

### 调优建议

**提高 AUC**:
- 增大 `--embed-dim` 到 64 或 128
- 增大 `--mlp-dims` 到 [512, 256, 128]
- 增加 `--epochs` 到 30-50
- 使用更大的数据集 (修改 `prepare_data.py` 中的 `max_samples`)

**加速训练**:
- 增大 `--batch-size` 到 8192 或更大
- 减小 `--epochs` (快速验证)
- 使用 GPU 训练

---

## 📊 数据格式

训练数据使用 `.npz` 格式存储:

```python
{
    'sparse_features': np.ndarray,  # 稀疏特征索引 (N, 26)
    'dense_features': np.ndarray,   # 连续特征 (N, 13)
    'labels': np.ndarray            # 点击标签 (N,) 0/1
}
```

### 特征说明

**连续特征 (I1-I13)**:
- 数值型特征 (如: 用户年龄、广告历史表现等)
- 经过 Z-score 标准化
- 缺失值填充为均值

**稀疏特征 (C1-C26)**:
- 类别型特征 (如: 用户ID、广告ID、设备类型等)
- 经历类别编码 (String → Integer ID)
- 高基数特征 (部分特征有数万类别)

---

## 📈 预期性能指标

在 Criteo 数据集上的预期表现:

| 指标 | 目标值 | 说明 |
|------|--------|------|
| **AUC** | 0.75 - 0.80 | 主要评估指标 |
| **LogLoss** | 0.40 - 0.50 | 对数损失 |
| **训练时间** | 30-60 分钟 | 100万样本, 10 epochs, GPU |

### 性能对比

- **Logistic Regression**: AUC ~0.72
- **FM**: AUC ~0.74
- **DeepFM**: AUC ~0.76-0.78 ⭐
- **Deep & Cross**: AUC ~0.78
- **xDeepFM**: AUC ~0.79

---

## 💾 输出文件

训练完成后会生成:

```
checkpoints/
├── best_model.pt          # 最佳模型 (基于验证 AUC)
├── last_model.pt          # 最后一个 epoch 的模型
└── training_log.txt       # 训练日志

data/
├── train_data.npz         # 训练集
├── val_data.npz           # 验证集
└── feature_encoder.pkl    # 特征编码器 (用于推理)
```

### 模型检查点内容

```python
checkpoint = {
    'model_state_dict': model.state_dict(),
    'val_auc': 0.76,           # 最佳验证 AUC
    'val_loss': 0.45,          # 最佳验证 Loss
    'field_dims': [...],       # 特征维度
    'embed_dim': 32,           # 嵌入维度
    'epoch': 8,                # 最佳 epoch
}
```

---

## 🔧 依赖

- Python >= 3.8
- PyTorch >= 2.0
- scikit-learn
- numpy
- tqdm
- datasets (Hugging Face)
- pandas

---

## 📚 评估指标

- **AUC**: ROC 曲线下面积 (主要指标)
  - 衡量模型排序能力
  - 0.5 = 随机猜测, 1.0 = 完美预测
  - Criteo 基准: 0.75+

- **LogLoss**: 对数损失
  - 惩罚自信的错误预测
  - 越小越好

- **Loss**: 二元交叉熵损失

---

## 🎯 下一步

### 1. 导出 ONNX 模型

```bash
python3 export_onnx.py \
    --checkpoint checkpoints/best_model.pt \
    --output models/deep_fm_criteo.onnx
```

### 2. C++ 在线推理

在 C++ 广告系统中加载 ONNX 模型:

```cpp
#include "ctr/ctr_manager.h"

auto ctr_manager = std::make_unique<ctr::CTRManager>();
ctr_manager->load_model("models/deep_fm_criteo.onnx");

// 实时预估 CTR
double ctr = ctr_manager->predict_ctr(request, ad_id);
```

### 3. A/B 测试

对比不同模型版本:
- 版本 A: Logistic Regression (基线)
- 版本 B: DeepFM (新模型)
- 监控指标: eCPM / CTR / 收入

---

## 📖 参考资料

- [DeepFM 论文](https://arxiv.org/abs/1703.04247)
- [Criteo Kaggle Challenge](https://www.kaggle.com/c/criteo-display-ad-challenge)
- [PyTorch 官方文档](https://pytorch.org/docs/stable/)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/c-api/)

---

**作者**: airprofly
**创建日期**: 2026-03-12
**更新日期**: 2026-03-12
**状态**: ✅ 生产就绪
