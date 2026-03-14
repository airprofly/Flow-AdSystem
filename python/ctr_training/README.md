# DeepFM CTR 预估模型训练

基于 DeepFM (Factorization Machine + Deep Neural Network) 的 CTR (Click-Through Rate) 预估模型实现,专为**广告系统 CTR 预估**场景设计。

## 🎯 数据集说明

### 使用模拟数据训练

本训练脚本使用**模拟 CTR 数据**进行训练,特征维度完全匹配真实广告系统:

- **数据规模**: 可配置 (默认 10 万样本)
- **特征维度**: 8 个连续特征 + 13 个稀疏特征
- **模拟 CTR**: ~25% (正样本比例)
- **特征类型**:
  - **连续特征 (8 个)**: 广告历史 CTR、用户年龄、历史展示次数等
  - **稀疏特征 (13 个)**: 广告 ID、用户性别、城市、设备类型、上下文信息等

### 特征设计 (与 C++ 广告系统完全一致)

**连续特征 (8 个)**:
1. **广告特征 (3 个)**: `hist_ctr`, `hist_cvr`, `position`
2. **用户特征 (3 个)**: `age_norm`, `history_ctr`, `history_impressions_norm`
3. **上下文特征 (2 个)**: `hour_norm`, `time_slot`

**稀疏特征 (13 个)**:
1. **广告特征 (5 个)**: `ad_id`, `campaign_id`, `industry_id`, `creative_type`, `ad_size`
2. **用户特征 (3 个)**: `user_gender`, `user_city`, `user_device_type`
3. **上下文特征 (5 个)**: `day_of_week`, `is_weekend`, `page_category`, `network_type`, `carrier`

---

## 🚀 快速开始

### 前置要求

1. **安装依赖**:
```bash
pip install torch torchvision scikit-learn numpy tqdm
```

### 一键训练 (推荐)

```bash
# 激活环境
conda activate pytorch

# 使用模拟数据训练 DeepFM 模型
python3 train_with_synthetic_data.py \
    --num-samples 100000 \
    --epochs 10 \
    --batch-size 2048 \
    --embed-dim 32
```

脚本会自动:
1. 生成模拟 CTR 数据 (完全匹配广告系统特征)
2. 训练 DeepFM 模型
3. 保存最佳模型到 `checkpoints/best_model.pt`

---

## 🏗️ 模型架构

**DeepFM** 结合了两个组件:

1. **FM (Factorization Machine)**:
   - 一阶线性部分: 捕捉单个特征的重要性
   - 二阶交叉部分: 捕捉特征间的二阶交互

2. **Deep Neural Network**:
   - 多层感知机: 捕捉高阶非线性特征交互

```
输入特征 (8 dense + 13 sparse)
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
| `--num-samples` | 100000 | 生成样本数量 (可增加到 100 万) |
| `--batch-size` | 2048 | 批次大小 (GPU 内存允许时可增大到 8192) |
| `--epochs` | 10 | 训练轮数 (建议 20-50) |
| `--embed-dim` | 32 | 嵌入维度 (可增加到 64/128) |
| `--mlp-dims` | [256, 128, 64] | MLP 隐藏层维度 |
| `--dropout` | 0.2 | Dropout 比例 |
| `--learning-rate` | 1e-3 | 学习率 |

### 调优建议

**提高 AUC**:
- 增大 `--num-samples` 到 100 万或更多
- 增大 `--embed-dim` 到 64 或 128
- 增大 `--mlp-dims` 到 [512, 256, 128]
- 增加 `--epochs` 到 30-50

**加速训练**:
- 增大 `--batch-size` 到 8192 或更大
- 减小 `--epochs` (快速验证)
- 使用 GPU 训练

---

## 📊 数据格式

本脚本使用**模拟数据生成器**,在内存中直接生成训练数据:

```python
{
    'sparse_features': torch.Tensor,  # 稀疏特征索引 (N, 13)
    'dense_features': torch.Tensor,   # 连续特征 (N, 8)
    'labels': torch.Tensor            # 点击标签 (N,) 0/1
}
```

### 特征说明

**连续特征 (8 个)**:
- 数值型特征 (如: 用户年龄、广告历史表现等)
- 经过 Z-score 标准化
- 模拟真实分布 (正态分布、均匀分布、指数分布)

**稀疏特征 (13 个)**:
- 类别型特征 (如: 用户ID、广告ID、设备类型等)
- 类别编码: Integer ID
- 不同基数: 从 4 (性别) 到 1,000,000 (广告 ID)

---

## 📈 预期性能指标

在模拟数据上的预期表现:

| 指标 | 目标值 | 说明 |
|------|--------|------|
| **AUC** | 0.70 - 0.80 | 主要评估指标 |
| **LogLoss** | 0.40 - 0.50 | 对数损失 |
| **训练时间** | 5-10 分钟 | 10万样本, 10 epochs, GPU |

### 性能对比

- **Logistic Regression**: AUC ~0.65-0.70
- **FM**: AUC ~0.70-0.75
- **DeepFM**: AUC ~0.75-0.80 ⭐

### 注意事项

> ⚠️ **重要**: 这是使用模拟数据的训练,主要用于:
> 1. 验证训练流程是否正常工作
> 2. 测试模型架构和超参数
> 3. 快速迭代和调试
>
> 实际部署时,建议使用真实广告数据集进行训练,以获得更好的性能。

---

## 💾 输出文件

训练完成后会生成:

```
checkpoints/
└── best_model.pt          # 最佳模型 (基于验证 AUC)
```

### 模型检查点内容

```python
checkpoint = {
    'model_state_dict': model.state_dict(),
    'val_auc': 0.76,              # 最佳验证 AUC
    'val_loss': 0.45,             # 最佳验证 Loss
    'field_dims': [...],          # 特征维度 (13 个稀疏特征的基数)
    'num_dense_features': 8,      # 连续特征数量
    'embed_dim': 32,              # 嵌入维度
    'mlp_dims': [256, 128, 64],   # MLP 隐藏层维度
    'dropout': 0.2,               # Dropout 比例
    'epoch': 8,                   # 最佳 epoch
}
```

---

## 🔧 依赖

- Python >= 3.8
- PyTorch >= 2.0
- scikit-learn
- numpy
- tqdm

---

## 📚 评估指标

- **AUC**: ROC 曲线下面积 (主要指标)
  - 衡量模型排序能力
  - 0.5 = 随机猜测, 1.0 = 完美预测
  - 目标值: 0.75+

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
    --output models/deep_fm_ads.onnx
```

### 2. C++ 在线推理

在 C++ 广告系统中加载 ONNX 模型:

```cpp
#include "ctr/ctr_manager.h"

auto ctr_manager = std::make_unique<ctr::CTRManager>();
ctr_manager->load_model("models/deep_fm_ads.onnx");

// 实时预估 CTR
double ctr = ctr_manager->predict_ctr(request, ad_id);
```

### 3. 集成到广告系统

模型训练完成后,可以:
1. 导出为 ONNX 格式
2. 在 C++ 广告系统中加载
3. 用于实时 CTR 预估
4. 结合 ECPM 排序模块使用

---

## 📖 参考资料

- [DeepFM 论文](https://arxiv.org/abs/1703.04247)
- [PyTorch 官方文档](https://pytorch.org/docs/stable/)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/c-api/)
- [广告系统架构文档](../../README.md)

---

**作者**: airprofly
**创建日期**: 2026-03-12
**更新日期**: 2026-03-14
**状态**: ✅ 生产就绪 (使用模拟数据)
