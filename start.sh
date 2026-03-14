#!/bin/bash
# Flow-AdSystem 一键启动脚本
# 适用于 clone 后首次初始化（包含模型训练、构建、测试）
set -e

# ────────── 颜色 ──────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKPOINT_DIR="${PROJECT_ROOT}/python/ctr_training/checkpoints"
MODEL_PATH="${PROJECT_ROOT}/models/deep_fm.onnx"

# ────────── 默认参数 ──────────
NUM_ADS=100
BUILD_TYPE="Release"
SKIP_DEPS=false
SKIP_TRAIN=false

step() { echo -e "\n${CYAN}──────────────────────────────────────────${NC}"; echo -e "${YELLOW}[$1/$TOTAL_STEPS] $2${NC}"; }
ok()   { echo -e "${GREEN}✓ $1${NC}"; }
warn() { echo -e "${YELLOW}⚠ $1${NC}"; }
die()  { echo -e "${RED}✗ $1${NC}"; exit 1; }

usage() {
    echo -e "Usage: $0 [options]"
    echo -e ""
    echo -e "Options:"
    echo -e "  --num-ads <N>      生成广告数量 (默认: 100, 推荐建全量测试: 100000)"
    echo -e "  --skip-deps        跳过系统依赖安装步骤"
    echo -e "  --skip-train       跳过模型训练步骤（需要已存在 ${MODEL_PATH}）"
    echo -e "  --debug            使用 Debug 模式编译（默认 Release）"
    echo -e "  -h, --help         显示此帮助信息"
    echo -e ""
    echo -e "Examples:"
    echo -e "  $0                          # 标准一键启动"
    echo -e "  $0 --num-ads 100000         # 生成 10 万广告数据"
    echo -e "  $0 --skip-deps --skip-train # 跳过依赖安装和模型训练"
    echo -e "  $0 --debug                  # Debug 模式编译"
}

# ────────── 解析参数 ──────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --num-ads)    NUM_ADS="$2"; shift 2 ;;
        --skip-deps)  SKIP_DEPS=true; shift ;;
        --skip-train) SKIP_TRAIN=true; shift ;;
        --debug)      BUILD_TYPE="Debug"; shift ;;
        -h|--help)    usage; exit 0 ;;
        *) echo -e "${RED}未知参数: $1${NC}"; usage; exit 1 ;;
    esac
done

BUILD_DIR="${PROJECT_ROOT}/build"
TOTAL_STEPS=7
[[ "${SKIP_DEPS}" == true ]] && TOTAL_STEPS=$((TOTAL_STEPS - 1))
[[ "${SKIP_TRAIN}" == true ]] && TOTAL_STEPS=$((TOTAL_STEPS - 2))
CURRENT_STEP=0

echo -e "\n${BLUE}╔══════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║        Flow-AdSystem  一键启动脚本        ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════╝${NC}"
echo -e "  构建模式: ${BUILD_TYPE} | 广告数量: ${NUM_ADS}"

# ────────── 1. 系统依赖 ──────────
if [[ "${SKIP_DEPS}" == false ]]; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    step ${CURRENT_STEP} "检查/安装系统依赖"
    MISSING=()
    for dep in cmake ninja g++ python3 pip3; do
        command -v "$dep" &>/dev/null || MISSING+=("$dep")
    done
    if (( ${#MISSING[@]} > 0 )); then
        warn "缺少: ${MISSING[*]}，尝试自动安装..."
        sudo apt-get update -y -qq
        sudo apt-get install -y cmake ninja-build g++ python3 python3-pip libssl-dev
    fi
    ok "系统依赖就绪"
fi

# ────────── 2. Python 依赖 ──────────
if [[ "${SKIP_DEPS}" == false ]]; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    step ${CURRENT_STEP} "安装 Python 依赖"
    PIP_FLAGS="--quiet"
    if pip3 install --help 2>&1 | grep -q 'break-system-packages'; then
        PIP_FLAGS="$PIP_FLAGS --break-system-packages"
    fi
    pip3 install torch torchvision onnx onnxruntime numpy scikit-learn tqdm pandas $PIP_FLAGS
    ok "Python 依赖安装完成"
fi

# ────────── 3 & 4. 训练 + 导出 ONNX ──────────
if [[ "${SKIP_TRAIN}" == false ]]; then
    CURRENT_STEP=$((CURRENT_STEP + 1))
    step ${CURRENT_STEP} "训练 DeepFM CTR 模型"
    mkdir -p "${CHECKPOINT_DIR}" "${PROJECT_ROOT}/models"

    if [ -f "${MODEL_PATH}" ]; then
        ok "ONNX 模型已存在 (${MODEL_PATH})，跳过训练"
    else
        python3 "${PROJECT_ROOT}/python/ctr_training/train_with_synthetic_data.py" \
            --num-samples 10000 \
            --epochs 5 \
            --batch-size 1024 \
            --embed-dim 16 \
            --output-dir "${CHECKPOINT_DIR}"
        ok "训练完成"

        CURRENT_STEP=$((CURRENT_STEP + 1))
        step ${CURRENT_STEP} "导出 ONNX 模型"
        CHECKPOINT_FILE="${CHECKPOINT_DIR}/best_model.pt"
        [ -f "${CHECKPOINT_FILE}" ] || die "训练检查点不存在: ${CHECKPOINT_FILE}"
        python3 "${PROJECT_ROOT}/python/ctr_training/export_onnx.py" \
            --checkpoint "${CHECKPOINT_FILE}" \
            --output "${MODEL_PATH}" \
            --opset 14 \
            --no-verbose
        ok "ONNX 模型已导出: ${MODEL_PATH}"
    fi
elif [[ ! -f "${MODEL_PATH}" ]]; then
    die "指定了 --skip-train 但 ONNX 模型不存在: ${MODEL_PATH}"
fi

# ────────── 5. 构建 ──────────
CURRENT_STEP=$((CURRENT_STEP + 1))
step ${CURRENT_STEP} "CMake 配置 & 构建 (${BUILD_TYPE})"
cmake -B "${BUILD_DIR}" \
      -G Ninja \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_CXX_STANDARD=20
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
ok "C++ 项目构建完成"

# ────────── 6. 生成测试数据 ──────────
CURRENT_STEP=$((CURRENT_STEP + 1))
step ${CURRENT_STEP} "生成测试广告数据 (${NUM_ADS} 条)"
mkdir -p "${PROJECT_ROOT}/data/data"
"${BUILD_DIR}/bin/ad_generator" -n "${NUM_ADS}" -o "${PROJECT_ROOT}/data/data/ads_data.json"
ok "广告数据已生成: data/data/ads_data.json"

# ────────── 7. 运行测试 ──────────
CURRENT_STEP=$((CURRENT_STEP + 1))
step ${CURRENT_STEP} "运行所有 GTest 测试"
cd "${BUILD_DIR}"
ctest --output-on-failure --parallel "$(nproc)"
cd "${PROJECT_ROOT}"

echo -e "\n${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║      Flow-AdSystem 初始化完成！           ║${NC}"
echo -e "${GREEN}║  构建产物: build/bin/                     ║${NC}"
echo -e "${GREEN}║  ONNX 模型: models/deep_fm.onnx           ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
