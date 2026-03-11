#!/bin/bash
# Author: airprofly
# 广告数据生成脚本

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
DATA_DIR="${PROJECT_ROOT}/data/data"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   广告数据生成脚本${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# 创建数据目录
mkdir -p "${DATA_DIR}"
echo -e "${YELLOW}✓ 数据目录: ${DATA_DIR}${NC}"

# 检查构建目录是否存在
if [ ! -d "${BUILD_DIR}" ]; then
    echo -e "${YELLOW}构建目录不存在,正在创建...${NC}"
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    echo -e "${YELLOW}正在配置 CMake...${NC}"
    cmake ..
    echo -e "${GREEN}✓ CMake 配置完成${NC}"
else
    echo -e "${GREEN}✓ 构建目录已存在${NC}"
fi

# 编译项目
echo ""
echo -e "${YELLOW}正在编译项目...${NC}"
cd "${BUILD_DIR}"
make -j$(nproc)
echo -e "${GREEN}✓ 编译完成${NC}"

# 解析命令行参数
NUM_ADS=100
NUM_CAMPAIGNS=1000
NUM_CREATIVES=5000
NUM_ADVERTISERS=100
OUTPUT_FILE="${DATA_DIR}/ads_data.json"
SEED=42

while [[ $# -gt 0 ]]; do
    case $1 in
        -n|--num-ads)
            NUM_ADS="$2"
            shift 2
            ;;
        -c|--num-campaigns)
            NUM_CAMPAIGNS="$2"
            shift 2
            ;;
        -r|--num-creatives)
            NUM_CREATIVES="$2"
            shift 2
            ;;
        -a|--num-advertisers)
            NUM_ADVERTISERS="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        -s|--seed)
            SEED="$2"
            shift 2
            ;;
        -h|--help)
            echo "用法: $0 [选项]"
            echo ""
            echo "选项:"
            echo "  -n, --num-ads <N>              广告数量 (默认: 100)"
            echo "  -c, --num-campaigns <N>        广告计划数量 (默认: 1000)"
            echo "  -r, --num-creatives <N>        创意数量 (默认: 5000)"
            echo "  -a, --num-advertisers <N>      广告主数量 (默认: 100)"
            echo "  -o, --output <file>            输出文件路径 (默认: ./data/data/ads_data.json)"
            echo "  -s, --seed <N>                 随机种子 (默认: 42)"
            echo "  -h, --help                     显示此帮助信息"
            echo ""
            echo "示例:"
            echo "  $0                                    # 使用默认配置"
            echo "  $0 -n 10000 -o test.json             # 生成1万条广告"
            echo "  $0 -n 1000 -c 100 -r 500 -a 50       # 完整自定义配置"
            exit 0
            ;;
        *)
            echo -e "${RED}错误: 未知参数 $1${NC}"
            echo "使用 -h 或 --help 查看帮助"
            exit 1
            ;;
    esac
done

# 生成数据
echo ""
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}   开始生成数据${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "广告数量:     ${NUM_ADS}"
echo -e "广告计划数量: ${NUM_CAMPAIGNS}"
echo -e "创意数量:     ${NUM_CREATIVES}"
echo -e "广告主数量:   ${NUM_ADVERTISERS}"
echo -e "输出文件:     ${OUTPUT_FILE}"
echo -e "随机种子:     ${SEED}"
echo -e "${YELLOW}========================================${NC}"
echo ""

# 执行生成器
"${BUILD_DIR}/bin/ad_generator" \
    -n "${NUM_ADS}" \
    -c "${NUM_CAMPAIGNS}" \
    -r "${NUM_CREATIVES}" \
    -a "${NUM_ADVERTISERS}" \
    -o "${OUTPUT_FILE}" \
    -s "${SEED}"

# 检查结果
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   ✓ 数据生成完成!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "文件位置: ${OUTPUT_FILE}"
    echo -e "文件大小: $(du -h "${OUTPUT_FILE}" | cut -f1)"
    echo -e "${GREEN}========================================${NC}"
else
    echo ""
    echo -e "${RED}✗ 数据生成失败${NC}"
    exit 1
fi
