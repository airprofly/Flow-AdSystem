#!/bin/bash

# ===========================================
# 使用模拟数据快速训练 DeepFM CTR 模型
# ===========================================

set -e

echo "=========================================="
echo "🚀 DeepFM CTR 模型快速训练 (模拟数据)"
echo "=========================================="
echo ""

# 配置
NUM_SAMPLES=100000  # 10万样本
EPOCHS=10
BATCH_SIZE=2048

echo "📊 训练配置:"
echo "   样本数: $NUM_SAMPLES"
echo "   训练轮数: $EPOCHS"
echo "   批次大小: $BATCH_SIZE"
echo ""

# 检查 Python
if ! command -v python3 &> /dev/null; then
    echo "❌ 错误: 未找到 python3"
    exit 1
fi

echo "✅ Python3 版本: $(python3 --version)"
echo ""

# 检查 PyTorch
if ! python3 -c "import torch" &> /dev/null; then
    echo "⚠️  未安装 PyTorch,正在安装..."
    pip install torch torchvision --index-url https://download.pytorch.org/whl/cpu
fi

# 检查其他依赖
echo "📦 检查依赖..."
pip install numpy scikit-learn tqdm -q

echo ""
echo "🏋️  开始训练..."
echo ""

# 运行训练
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$SCRIPT_DIR/train_with_synthetic_data.py" \
    --num-samples $NUM_SAMPLES \
    --epochs $EPOCHS \
    --batch-size $BATCH_SIZE \
    --embed-dim 32 \
    --mlp-dims 256 128 64 \
    --learning-rate 1e-3 \
    --output-dir "$SCRIPT_DIR/checkpoints"

echo ""
echo "=========================================="
echo "✅ 训练完成!"
echo "=========================================="
echo ""
echo "📊 模型文件:"
echo "   - 最佳模型: $SCRIPT_DIR/checkpoints/best_model.pt"
echo ""
echo "🎯 模拟数据性能 (仅供参考):"
echo "   - AUC 通常在 0.60-0.75 之间"
echo "   - 真实数据性能会更好 (0.75-0.80)"
echo ""
echo "🚀 下一步:"
echo "   1. 使用真实数据训练获得更好的性能"
echo "   2. 导出 ONNX 模型用于 C++ 推理"
echo "   3. 在线系统中进行实时 CTR 预估"
echo ""
