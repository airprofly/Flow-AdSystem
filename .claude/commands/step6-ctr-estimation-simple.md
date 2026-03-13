# Step 6: CTR 预估模块 - 实施路线

## 🎯 核心目标

实现**离线训练 + 在线预估**的完整 CTR 预估系统:
- **离线**: PyTorch 训练深度学习模型 → 导出 ONNX
- **在线**: C++ 加载 ONNX 模型 → 5ms 内返回预估 CTR

---

## 📦 技术栈

| 模块 | 技术选型 |
|------|---------|
| **训练框架** | PyTorch 2.x + PyTorch Lightning |
| **模型架构** | **DeepFM** (因子分解机 + 深度神经网络) |
| **模型导出** | ONNX |
| **推理引擎** | ONNX Runtime C++ |
| **数据格式** | Parquet (训练) |

### 为什么选择 DeepFM?

✅ **优势:**
- **FM 二阶交叉**: 自动学习特征组合,无需人工特征工程
- **深度神经网络**: 学习高阶非线性特征交互
- **端到端训练**: FM 和 Deep 部分共享嵌入层,参数高效
- **AUC 表现**: 通常比 Wide & Deep 高 1-2 个百分点
- **适合稀疏特征**: 推荐系统的典型场景(用户ID、广告ID等)

---

## 🏗️ 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     离线训练 Pipeline                        │
│  数据采集 → 特征工程 → PyTorch训练 → 导出ONNX               │
└─────────────────────────────────────────────────────────────┘
                          │ 模型文件
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    在线预估服务 (C++)                        │
│  OpenRTB请求 → 特征提取 → ONNX推理 → 返回CTR                │
└─────────────────────────────────────────────────────────────┘
```

---

## 📝 实施步骤 (8周)

### **Week 1-2: 基础设施搭建**

#### C++ 部分
- [x] 实现 C++ 核心数据结构
  - `FeatureSpec`: 特征定义(名称/类型/维度)
  - `FeatureContext`: 特征值存储
  - `ModelConfig`: 模型配置
  - `InferenceResult`: 推理结果

- [x] 实现 C++ 接口层
  - `FeatureExtractor`: 特征提取器接口
  - `ModelInferenceEngine`: 推理引擎接口
  - `CTRManager`: CTR 管理器

- [x] 编写单元测试框架
  - 使用 Google Test
  - Mock 特征提取器 (待补充)

#### Python 部分
- [x] 搭建 PyTorch 训练环境
  - 安装 PyTorch + PyTorch Lightning
  - 创建项目结构 `python/ctr_training/`

---

### **Week 3-4: 特征工程**

#### C++ 特征提取器实现
- [x] `UserFeatureExtractor`: 用户特征(年龄/性别/地域/设备)
- [x] `AdFeatureExtractor`: 广告特征(ID/行业/创意类型)
- [x] `ContextFeatureExtractor`: 上下文特征(时间/页面/位置)

特征示例:
```cpp
// 用户特征
- user_age: float (归一化 0-1)
- user_gender: int64_t (类别编码)
- user_city: int64_t (类别编码)

// 广告特征
- ad_id: int64_t
- campaign_id: int64_t
- industry_id: int64_t

// 上下文特征
- hour_of_day: float (0-23 归一化)
- day_of_week: int64_t (0-6)
- device_type: int64_t (类别编码)
```

#### Python 特征工程
- [x] 实现数据预处理脚本
  - 稀疏特征: 类别 → ID 编码
  - 连续特征: Min-Max 归一化
  - 数据格式: Parquet

---

### **Week 5-6: 离线训练 (Python)**

#### 模型实现
- [x] 实现 `DeepFM` 模型
  ```python
  # FM 一阶项: 线性部分
  # FM 二阶项: 特征交叉 (Σ(vi · vj))
  # Deep 部分: MLP (高阶特征交互)
  # 输出: Sigmoid → CTR 概率
  ```

**DeepFM 架构:**
```
稀疏特征 (Sparse Features)
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

- [ ] 实现 `CTRDataModule` (当前为原生 PyTorch DataLoader)
  - 训练集/验证集加载
  - Data Loader 配置

- [ ] 实现 `CTRLightningModule` (当前为原生 PyTorch 训练循环)
  - 训练步骤: BCE Loss
  - 验证步骤: AUC + LogLoss
  - 优化器: Adam

#### 训练脚本
- [x] `train.py`: 训练入口
  ```bash
  python train.py \
    --train-data data/train.parquet \
    --val-data data/val.parquet \
    --epochs 10 \
    --batch-size 2048 \
    --export-onnx
  ```

- [x] `export_onnx()`: 导出 ONNX 模型
  ```python
  torch.onnx.export(
      model,
      dummy_input,
      "wide_and_deep.onnx",
      opset_version=14
  )
  ```

#### 验收标准
- [ ] 训练 AUC > 0.75 (待用固定数据集复核)
- [x] 成功导出 ONNX 模型
- [x] ONNX 模型验证通过

---

### **Week 7: 在线推理 (C++)**

#### ONNX Runtime 引擎
- [x] 实现 `ONNXRuntimeEngine`
  ```cpp
  class ONNXRuntimeEngine : public ModelInferenceEngine {
      // 1. 加载 ONNX 模型
      bool initialize(model_path, feature_spec);

      // 2. 单次推理
      bool predict(feature_context, result);

      // 3. 批量推理
      bool predict_batch(contexts, results);
  };
  ```

- [x] 核心功能
  - 模型加载: `Ort::Session`
  - 张量准备: 特征 → ONNX Tensor
  - 推理执行: `session->Run()`
  - 结果提取: Tensor → CTR

#### 性能优化
- [x] 启用图优化: `ORT_ENABLE_ALL`
- [x] 多线程推理: `SetIntraOpNumThreads`
- [ ] SIMD 加速(可选)

#### 验收标准
- [ ] 单次预测 p99 < 5ms
- [ ] 吞吐量 > 100K QPS

---

### **Week 8: 集成与 A/B 测试**

#### CTR 管理器完善
- [x] 实现多模型支持
  ```cpp
  // 支持同时加载多个模型(不同版本)
  // 流量分配: Model A: 20%, Model B: 80%
  ```

- [x] 实现模型热更新
  ```cpp
  bool reload_model(model_name, new_model_path);
  ```

- [ ] 集成到广告投放服务
  ```cpp
  // 在排序阶段调用 CTR 预估
  double ctr = ctr_manager->predict_ctr(request, imp, ad_id);
  double ecpm = ctr * bid_price;
  ```

#### A/B 测试
- [x] 流量分流机制
- [ ] 监控指标: AUC / LogLoss / 延迟
- [ ] 对比实验: 新模型 vs 基线

---

## 🗂️ 文件结构

```
Flow-AdSystem/
├── src/ctr/                          # C++ 在线预估
│   ├── core/
│   │   ├── feature_spec.h            # 特征定义
│   │   ├── feature_context.h         # 特征上下文
│   │   └── model_config.h            # 模型配置
│   ├── extractor/
│   │   ├── feature_extractor.h       # 特征提取器接口
│   │   ├── user_feature_extractor.h
│   │   ├── ad_feature_extractor.h
│   │   └── context_feature_extractor.h
│   ├── engine/
│   │   ├── model_inference_engine.h  # 推理引擎接口
│   │   └── onnx_runtime_engine.h     # ONNX Runtime 实现
│   └── ctr_manager.h                 # CTR 管理器
│
└── python/ctr_training/              # Python 离线训练
    ├── models/
    │   ├── wide_and_deep.py          # Wide & Deep 模型
    │   └── deep_fm.py                # DeepFM 模型(可选)
    ├── data/
    │   └── prepare_data.py           # 数据预处理
    ├── train.py                      # 训练脚本
    ├── evaluate.py                   # 评估脚本
    └── export_onnx.py                # ONNX 导出
```

---

## 📊 核心数据流

### 离线训练流程
```
原始日志 (CSV/Parquet)
    ↓
特征工程 (prepare_data.py)
    ↓
训练数据 (Parquet: 稀疏特征/连续特征/标签)
    ↓
PyTorch 训练 (train.py)
    ↓
模型检查点 (.pth)
    ↓
ONNX 导出 (export_onnx.py)
    ↓
ONNX 模型 (.onnx)
```

### 在线预估流程
```
OpenRTB 请求
    ↓
特征提取 (FeatureExtractor)
    ↓
FeatureContext (特征值)
    ↓
ONNX 推理 (ONNXRuntimeEngine)
    ↓
CTR 预估值 (0-1)
    ↓
eCPM 计算 = CTR × 出价
```

---

## ✅ 验收标准

### 功能验收
- [x] 支持离线训练 DeepFM 模型
- [x] 支持 ONNX 模型导出和加载
- [ ] 支持在线实时预估 (< 5ms)
- [x] 支持模型热更新
- [ ] 支持 A/B 测试(多模型流量分配)

### 性能验收
- [ ] 训练 AUC > 0.75
- [ ] 在线预估 p99 < 5ms
- [ ] 吞吐量 > 100K QPS
- [ ] 内存占用 < 2GB (单模型)

### 质量验收
- [ ] 单元测试覆盖率 > 80%
- [ ] 在线预估误差 < 5% (vs 离线训练)

---

## 🚀 快速开始

### 1. 离线训练
```bash
cd python/ctr_training

# 步骤1: 准备数据
python prepare_data.py \
  --input data/raw_logs.parquet \
  --output data/train_data.parquet

# 步骤2: 训练模型
python train.py \
  --train-data data/train_data.parquet \
  --val-data data/val_data.parquet \
  --export-onnx

# 输出: checkpoints/deep_fm.onnx
```

### 2. 在线预估 (C++)
```cpp
// 初始化
auto ctr_manager = std::make_unique<ctr::CTRManager>();
ctr_manager->initialize();

// 添加模型
ctr::ModelConfig config;
config.model_name = "deep_fm_v1";
config.model_path = "models/deep_fm.onnx";
config.traffic_fraction = 1.0;
ctr_manager->add_model(config);

// 预估 CTR
double ctr = ctr_manager->predict_ctr(request, imp, ad_id);
```

---

## 📚 参考资料

- [DeepFM 论文](https://arxiv.org/abs/1703.04247)
- [PyTorch 官方文档](https://pytorch.org/docs/stable/)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/c-api/)
- [PyTorch Lightning](https://pytorch-lightning.readthedocs.io/)

---

**作者**: airprofly
**创建日期**: 2026-03-11
**状态**: 🚀 实施路线图
