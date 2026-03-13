# FastMatch-Ad 广告投放系统

## 🎯 业务目标

高性能广告投放系统，支持 OpenRTB 2.5/2.6 协议规范，10万+ 广告池管理。

### 核心模块

✅ **C++ 广告 ECPM 排序 (Ranker)** - 实时计算广告 eCPM 并排序 <br>
✅ **C++ 广告展示频次控制** - 跟踪用户广告展示次数，支持时间窗口 <br>
✅ **C++ 广告计划匀速消费 (Pacing)** - 控制广告投放速度，平滑预算消耗<br>
✅ **C++ 广告投放索引** - 快速检索符合条件的广告<br>
✅ **CTR 预估离线训练 (Python)** - PyTorch DeepFM 模型训练，支持 ONNX 导出<br>

## 📁 项目结构

```

## 🧪 CTR 预测快速验证

1) 使用 `pytorch` conda 环境导出 ONNX 模型:

```bash
conda run -n pytorch python python/ctr_training/export_onnx.py \
	--checkpoint python/ctr_training/checkpoints/best_model.pt \
	--output models/deep_fm.onnx \
	--opset 14 --no-verbose
```

2) 构建项目:

```bash
cmake -S . -B .build/Debug
cmake --build .build/Debug -j
```

3) 运行 CTR 相关 gtest:

```bash
cd .build/Debug
ctest -R "CTRManagerTest.PredictCTRWithOnnxModel|CTRE2ETest.PredictEndToEnd" --output-on-failure
```
FastMatch-Ad/
├── data/           # 数据层 (OpenRTB协议、数据模型)
├── core/           # 核心算法 (排序、频控、Pacing、索引)
├── python/         # Python 训练模块
│   └── ctr_training/    # CTR 预估离线训练
│       ├── models/      # DeepFM 模型
│       ├── data/        # 数据预处理
│       └── train.py     # 训练脚本
├── services/       # 微服务 (广告投放服务)
├── utils/          # 工具库 (日志、指标)
├── config/         # 配置管理
└── tests/          # 单元测试
```

 