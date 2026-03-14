<div align="center">

# 🔧 Flow-Ad 广告投放系统

### High-Performance Ad Delivery System with OpenRTB Protocol

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/Flow-AdSystem)
[![Star](https://img.shields.io/github/stars/airprofly/Flow-AdSystem?style=social)](https://github.com/airprofly/Flow-AdSystem/stargazers)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![C++ 20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Python 3.8+](https://img.shields.io/badge/python-3.8+-blue.svg)](https://www.python.org/downloads/)
[![PyTorch](https://img.shields.io/badge/PyTorch-2.0+-orange.svg)](https://pytorch.org/)
[![ONNX](https://img.shields.io/badge/ONNX-Runtime-green.svg)](https://onnxruntime.ai/)

[广告投放] · [OpenRTB] · [CTR预估] · [DeepFM] · [高性能计算]

</div>

---

## 📖 项目简介

**Flow-Ad** 是一个生产级高性能广告投放系统,支持 OpenRTB 2.5/2.6 协议规范,具备 **10 万+ 广告池**实时管理能力。

### 技术亮点

- **🚀 极致性能**: C++20 + TBB 并行计算,支持 10 万+ 广告池实时检索与排序
- **📡 标准协议**: 完整实现 OpenRTB 2.5/2.6 协议规范
- **🧠 智能算法**: DeepFM 深度学习 CTR 预估 + ECPM 排序
- **⚡ 高效推理**: PyTorch 训练 + ONNX Runtime 高性能推理引擎
- **🎯 精准控制**: 广告频次控制 + 匀速消费(Pacing)模块
- **🔍 快速召回**: 基于倒排索引的高效广告海选召回引擎

### 核心特性

| 特性 | 描述 |
|------|------|
| **高性能架构** | C++20 + STL + TBB 并行计算,端到端延迟 < 50ms |
| **智能排序** | DeepFM 模型 AUC 0.70-0.80,推理延迟 < 5ms |
| **标准协议** | 完整支持 OpenRTB 2.5/2.6 Bid Request/Response |
| **快速召回** | 倒排索引 + 多级过滤,10 万广告池检索 < 10ms |
| **精准控制** | Sliding Window 频次控制 + Token Bucket Pacing |

---

## 📌 系统架构

![系统架构图](docs/diagrams/dist/ad-system-architecture.svg)

### 技术栈

| 层级 | 技术选型 |
|------|----------|
| **协议层** | OpenRTB 2.5/2.6, RapidJSON |
| **算法层** | DeepFM (PyTorch), ONNX Runtime |
| **核心层** | C++20, STL, TBB 并行计算 |
| **索引层** | 倒排索引, 多级过滤 |
| **测试层** | Google Test (GTest) |

---

## 📦 核心模块

<details>
<summary><b>🔢 Step 1: 数据生成模块</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 生成 10 万+ 广告池测试数据
- 支持多种广告类型和行业分类
- 模拟真实广告投放场景

#### 技术实现
- C++ 高性能数据生成器
- JSON 格式输出,兼容 OpenRTB 规范
- 可配置广告数量和分布

**📖 详细文档**: [data/README.md](data/README.md)

</details>

<details>
<summary><b>🔢 Step 2: OpenRTB 协议解析器</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 解析 OpenRTB 2.5/2.6 Bid Request
- 支持完整字段解析和验证
- 高性能 JSON 反序列化

#### 技术实现
- 基于 RapidJSON 的高性能解析
- 完整的数据模型定义
- 错误处理和验证机制

**📖 详细文档**: [core/openrtb/README.md](core/openrtb/README.md)

</details>

<details>
<summary><b>🔢 Step 3: 广告索引引擎</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 海选召回,快速检索符合条件的广告
- 支持多维度过滤(行业、地域、设备等)
- 倒排索引优化检索性能

#### 技术实现
- 倒排索引数据结构
- 多级过滤策略
- 并行检索优化(TBB)

**📖 详细文档**: [core/indexing/README.md](core/indexing/README.md)

</details>

<details>
<summary><b>🔢 Step 4: 广告展示频次控制</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 跟踪用户广告展示次数
- 支持时间窗口配置
- 防止用户过度曝光

#### 技术实现
- 基于 Sliding Window 的频次统计
- 高效的内存管理
- 可配置的频次策略

**📖 详细文档**: [core/frequency/README.md](core/frequency/README.md)

</details>

<details>
<summary><b>🔢 Step 5: 广告计划匀速消费 (Pacing)</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 控制广告投放速度
- 平滑预算消耗
- 避免预算过早耗尽

#### 技术实现
- 令牌桶算法(Token Bucket)
- 实时 pacing 调整
- 预算消耗预测

**📖 详细文档**: [core/pacing/README.md](core/pacing/README.md)

</details>

<details>
<summary><b>🔢 Step 6: CTR 预估模块 (DeepFM)</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 基于 DeepFM 的 CTR 预估
- PyTorch 训练 + ONNX Runtime 推理
- 支持 8 个连续特征 + 13 个稀疏特征

#### 技术实现
- **训练框架**: PyTorch 2.0+
- **模型架构**: DeepFM (FM + DNN)
- **推理引擎**: ONNX Runtime 1.19.2
- **特征工程**: 自动特征提取和归一化

#### 模型性能
- **AUC**: 0.70 - 0.80 (模拟数据)
- **推理延迟**: < 5ms (ONNX Runtime)
- **吞吐量**: 10000+ QPS

**📖 详细文档**: [core/ctr/README.md](core/ctr/README.md)

</details>

<details>
<summary><b>🔢 Step 7: ECPM 排序模块</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 实时计算广告 eCPM
- 基于 CTR 和出价排序
- 支持多种排序策略

#### 技术实现
- eCPM = CTR × 出价 × 1000
- 高效排序算法
- Top-K 优化

</details>

<details>
<summary><b>🔢 Step 8: OpenRTB Response 响应包装</b> <code>✅ 已完成</code></summary>

#### 功能描述
- 竞价决策逻辑
- Bid Response 构建
- 符合 OpenRTB 2.5/2.6 规范

#### 技术实现
- 二价拍卖策略(Second-Price Auction)
- 高性能 JSON 序列化
- 完整的错误处理

**📖 详细文档**: [core/openrtb/README.md](core/openrtb/README.md)

</details>

---

## 📁 项目结构

<details>
<summary><b>🔧 查看完整目录结构</b></summary>

```text
Flow-AdSystem/
├── core/                      # 🔧 核心算法模块
│   ├── openrtb/              # 📡 OpenRTB 协议解析
│   ├── indexing/             # 🔍 广告索引引擎
│   ├── frequency/            # 🎯 频次控制
│   ├── pacing/               # ⏱️ 匀速消费
│   ├── ctr/                  # 🧠 CTR 预估 (ONNX Runtime)
│   └── ranker/               # 📊 ECPM 排序
├── data/                      # 💾 数据层
│   ├── data/                 # 测试数据
│   ├── generator/            # 数据生成器
│   └── models/               # 数据模型定义
├── python/                    # 🐍 Python 训练模块
│   └── ctr_training/         # CTR 预估离线训练
│       ├── models/           # DeepFM 模型定义
│       ├── data/             # 数据预处理
│       ├── checkpoints/      # 模型检查点 (.gitignore)
│       ├── train_with_synthetic_data.py  # 训练脚本
│       └── export_onnx.py    # ONNX 导出脚本
├── models/                    # 🎯 ONNX 模型文件 (.gitignore)
├── tests/                     # 🧪 单元测试
│   ├── unit/                 # 单元测试
│   └── integration/          # 集成测试
├── scripts/                   # 📜 辅助脚本
├── docs/                      # 📝 文档
│   └── diagrams/             # 📊 架构图
├── .github/                   # 🔧 CI/CD 配置
│   └── workflows/            # GitHub Actions
├── CMakeLists.txt            # 🔧 CMake 构建配置
└── README.md                 # 📖 项目文档
```

</details>

---

## 🔧 环境配置

### 前置要求

| 依赖 | 版本要求 | 安装方式 |
|------|----------|----------|
| **C++ 编译器** | GCC 11+ / Clang 13+ | 系统包管理器 |
| **CMake** | 3.20+ | [cmake.org](https://cmake.org/download/) |
| **Python** | 3.8+ | [python.org](https://www.python.org/) |
| **Conda** | 最新版 | [Miniconda](https://docs.conda.io/en/latest/miniconda.html) |

### 系统依赖

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    cmake \
    g++ \
    libssl-dev \
    python3 \
    python3-pip
```

### Python 环境

> ⚠️ **重要提示**: 请勿使用系统自带的 Python 运行本项目,建议使用 Conda 虚拟环境以避免依赖冲突。

```bash
# 创建 Conda 虚拟环境
conda create -n pytorch python=3.10 -y
conda activate pytorch

# 安装 PyTorch 和相关依赖
pip install torch torchvision scikit-learn numpy tqdm pandas onnx onnxruntime
```

---

## 🚀 快速开始

### 🌟 一键启动（推荐）

> ⭐ **最简单的方式**: 使用我们提供的一键启动脚本，自动完成所有配置！

```bash
# 克隆项目
git clone https://github.com/airprofly/Flow-AdSystem.git
cd Flow-AdSystem

# 一键启动（自动安装依赖、训练模型、生成数据、编译运行）
./start.sh
```

**启动脚本会自动完成**:
- ✅ 检查并安装系统依赖（CMake, g++, Python等）
- ✅ 安装Python依赖（PyTorch, ONNX等）
- ✅ 训练DeepFM模型并导出ONNX
- ✅ 生成测试数据（100条广告）
- ✅ 编译C++项目
- ✅ 运行所有测试

**高级选项**:
```bash
./start.sh --help                    # 查看所有选项
./start.sh --num-ads 10000           # 生成10000条广告数据
./start.sh --skip-deps --skip-train  # 跳过依赖和训练步骤
./start.sh --debug                   # 使用Debug模式编译
```

---

### 📝 手动安装步骤

如果您想手动控制每个步骤，请按照以下流程：

#### 1️⃣ 克隆项目

```bash
git clone https://github.com/airprofly/Flow-AdSystem.git
cd Flow-AdSystem
```

#### 2️⃣ 训练 DeepFM 模型

```bash
# 激活 Python 环境
conda activate pytorch

# 使用模拟数据训练 (10 万样本)
python3 python/ctr_training/train_with_synthetic_data.py \
    --num-samples 100000 \
    --epochs 10 \
    --batch-size 2048 \
    --embed-dim 32

# 输出: python/ctr_training/checkpoints/best_model.pt
```

### 3️⃣ 导出 ONNX 模型

```bash
# 导出为 ONNX 格式
python3 python/ctr_training/export_onnx.py \
    --checkpoint python/ctr_training/checkpoints/best_model.pt \
    --output models/deep_fm.onnx \
    --opset 14

# 输出: models/deep_fm.onnx
```

### 4️⃣ 生成测试数据

```bash
# 创建构建目录
cmake -B .build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 构建数据生成器
cmake --build .build/Debug --target ad_generator

# 生成 10 万广告数据
mkdir -p data/data
./.build/Debug/bin/ad_generator -n 100000 -o data/data/ads_data.json
```

### 5️⃣ 编译项目

```bash
# 配置 CMake (Debug 模式)
cmake -B .build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译项目 (并行)
cmake --build .build/Debug -j$(nproc)
```

### 6️⃣ 运行测试

```bash
# 运行所有测试
cd .build/Debug
ctest --output-on-failure --parallel $(nproc)

# 运行 CTR 相关测试
ctest -R "CTR" --output-on-failure

# 运行特定测试
ctest -R "CTRManagerTest.PredictCTRWithOnnxModel" --output-on-failure
```

---


## 📊 实验结果

### CTR 预估性能

| 指标 | 数值 | 说明 |
|------|------|------|
| **AUC** | 0.70 - 0.80 | 模拟数据 (10 万样本) |
| **LogLoss** | 0.40 - 0.50 | 对数损失 |
| **推理延迟** | < 5ms | ONNX Runtime,单线程 |
| **吞吐量** | 10000+ QPS | ONNX Runtime,多线程 |

### 系统性能

| 模块 | 性能指标 |
|------|----------|
| **广告召回** | < 10ms (10 万广告池) |
| **CTR 预估** | < 5ms (ONNX Runtime) |
| **ECPM 排序** | < 1ms (1000 候选) |
| **端到端延迟** | < 50ms (完整流程) |

---

## 📖 文档

- **DeepFM 模型**: [python/ctr_training/README.md](python/ctr_training/README.md)
- **系统架构**: [docs/diagrams/](docs/diagrams/)
- **测试指南**: [tests/README.md](tests/README.md)

---

## 🤝 贡献指南

欢迎贡献代码! 请遵循以下步骤:

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- **C++ 代码**: 遵循 Google C++ Style Guide
- **Python 代码**: 遵循 PEP 8 规范
- **提交信息**: 使用清晰的 Commit Message

---

## 📄 许可证

本项目采用 [MIT 许可证](https://opensource.org/licenses/MIT)。

---

## 🙏 致谢

- **OpenRTB 规范**: [IAB Tech Lab](https://www.iab.com/guidelines/openrtb/)
- **DeepFM 论文**: [DeepFM: A Factorization-Machine based Neural Network for CTR Prediction](https://arxiv.org/abs/1703.04247)
- **ONNX Runtime**: [Microsoft ONNX Runtime](https://onnxruntime.ai/)
- **RapidJSON**: [Tencent RapidJSON](https://rapidjson.org/)

---

## 📮 联系方式

- **作者**: airprofly
- **GitHub**: [@airprofly](https://github.com/airprofly)
- **Issues**: [GitHub Issues](https://github.com/airprofly/Flow-AdSystem/issues)

---

<div align="center">

**如果这个项目对你有帮助,请给一个 ⭐️ Star!**

Made with ❤️ by airprofly

</div>
