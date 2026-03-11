# CLAUDE.md

本文件提供 Claude Code (claude.ai/code) 在此仓库中工作的指导。

## 项目概述

**Flow-AdSystem** 是一个高性能广告投放系统,使用 C++20 实现,支持 OpenRTB 2.5/2.6 协议规范的广告池管理(10万+广告)。

### 业务目标

构建一个完整的广告技术服务栈,包括:
- **广告池模拟数据生成**: 符合 OpenRTB 协议,10万+ 广告
- **ECPM 排序服务**: 实时计算广告 ECPM 并排序
- **频次控制**: 跟踪用户广告展示次数,支持时间窗口
- **匀速消费 (Pacing)**: 控制广告投放速度,平滑预算消耗
- **广告索引服务**: 快速检索符合条件的广告
- **CTR 预估**: 离线模型训练 + 在线实时预估

---

## 技术选型

### 核心技术栈

| 类别 | 技术选型 | 版本 | 说明 |
|------|---------|------|------|
| **编程语言** | C++ | 20/23 | 现代C++特性,零开销抽象 |
| **构建系统** | CMake | 3.20+ | 跨平台构建,支持模块化 |
| **编译器** | GCC/Clang | 11+ / 13+ | 完整C++20支持 |
| **协议** | OpenRTB | 2.5/2.6 | IAB标准广告竞价协议 |
| **序列化** | Protocol Buffers | 3.21+ | 高效二进制序列化 |
| **RPC框架** | gRPC | 1.50+ | 高性能RPC,支持流式 |
| **HTTP服务** | Pistache/oat++ | - | 高性能HTTP框架 |
| **测试框架** | Google Test | 1.12+ | 单元测试和基准测试 |
| **日志库** | spdlog | 1.11+ | 高性能结构化日志 |
| **指标监控** | Prometheus | - | 指标采集和暴露 |
| **分布式追踪** | OpenTelemetry | - | 请求链路追踪 |
| **ML推理** | ONNX Runtime | 1.16+ | 跨平台模型推理 |
| **ML训练** | XGBoost/TensorFlow | - | 离线模型训练 |
| **缓存** | Redis | 7.0+ | 分布式缓存和频次控制 |
| **配置中心** | Consul | 1.15+ | 服务发现和配置管理 |
| **负载均衡** | Envoy | - | 服务网格和代理 |

### 为什么选择这些技术

#### C++20/23
- **零开销抽象**: 现代C++特性(concepts, ranges, coroutines)提供高性能
- **内存管理**: RAII和智能指针确保内存安全
- **并发支持**: std::jthread, atomic, latch/barrier等并发原语
- **编译时计算**: constexpr和模板元编程提升性能

#### Protocol Buffers + gRPC
- **高效序列化**: 二进制格式,比JSON快5-10倍
- **强类型**: 编译时类型检查,减少运行时错误
- **代码生成**: 自动生成客户端和服务端代码
- **流式支持**: 支持双向流,适合实时竞价场景

#### ONNX Runtime
- **跨平台**: 统一的推理引擎,支持多种训练框架
- **高性能**: 优化过的推理引擎,支持GPU加速
- **模型互换性**: 可在不同框架间迁移模型

---

## 微服务架构设计

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        API Gateway / Load Balancer               │
│                           (Envoy / Nginx)                        │
└───────────────────────────────┬─────────────────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  Ad Service   │     │ Bid Service   │     │Index Service  │
│  (广告投放)    │     │  (竞价服务)    │     │  (索引服务)    │
└───────┬───────┘     └───────┬───────┘     └───────┬───────┘
        │                     │                     │
        │                     │                     │
        ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Core Algorithm Layer                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │ Ranker   │ │Frequency │ │ Pacing   │ │  Index   │           │
│  │ (排序)    │ │ (频控)    │ │ (匀速消费) │ │  (索引)   │           │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘           │
└───────┼────────────┼────────────┼────────────┼──────────────────┘
        │            │            │            │
        ▼            ▼            ▼            ▼
┌─────────────────────────────────────────────────────────────────┐
│                     Data & ML Layer                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │  Redis   │ │  Model   │ │ Feature  │ │   Data   │           │
│  │ (缓存/频控)│ │ (模型推理) │ │ (特征工程) │ │ (数据存储) │           │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
└─────────────────────────────────────────────────────────────────┘
```

### 服务拆分原则

按照 **Domain-Driven Design (DDD)** 和 **微服务单一职责原则** 拆分:

| 服务名称 | 职责 | API接口 | 性能要求 |
|---------|------|---------|---------|
| **Ad Service** | 广告投放主流程 | `/api/v1/ad/serve` | < 50ms p99 |
| **Bid Service** | 实时竞价 | `/api/v1/bid/request` | < 100ms p99 |
| **Index Service** | 广告索引查询 | `/api/v1/index/search` | < 5ms p99 |
| **Model Service** | CTR模型推理 | `/api/v1/model/predict` | < 3ms p99 |
| **Frequency Service** | 频次控制 | `/api/v1/frequency/check` | < 2ms p99 |
| **Pacing Service** | 预算 pacing | `/api/v1/pacing/allow` | < 2ms p99 |
| **Data Generator Service** | 测试数据生成 | `/api/v1/data/generate` | - |

### 服务间通信

#### 同步通信 (gRPC)
```protobuf
// 广告服务调用索引服务
service IndexService {
  rpc SearchAd(SearchRequest) returns (SearchResponse);
}

// 广告服务调用模型服务
service ModelService {
  rpc PredictCTR(PredictRequest) returns (PredictResponse);
}
```

#### 异步通信 (消息队列)
- **事件总线**: Kafka / NATS
- **使用场景**:
  - 模型更新通知
  - 广告索引变更
  - 监控指标上报
  - 日志收集

---

## 核心模块设计

### 1. 广告 ECPM 排序 (Ranker)

**位置**: `core/ranker/`

**职责**:
- 实时计算广告 ECPM (Effective CPM)
- 多维度排序(出价 × CTR)
- 支持多种排序策略(贪婪、多样性、预算优化)

**技术选型**:
```cpp
// 核心数据结构
struct AdCandidate {
    uint64_t ad_id;
    double bid_price;        // 出价
    double predicted_ctr;    // 预估CTR
    double ecpm;             // eCPM = bid × CTR
    uint32_t category;       // 广告分类
};

// 排序算法选择
- std::sort (默认,快速排序)
- std::partial_sort (Top-K场景)
- std::nth_element (第K大元素)
- 并行排序 (std::execution::par)
```

**性能目标**:
- 排序延迟: < 10ms (1000个候选)
- 吞吐量: > 100K QPS
- 内存占用: < 100MB (10万广告)

**C++实现要点**:
```cpp
// 使用 C++20 concepts 约束
template<typename T>
concept Sortable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
};

template<Sortable T>
void rank_ads(std::vector<T>& candidates, RankStrategy strategy) {
    switch (strategy) {
        case RankStrategy::Greedy:
            std::sort(candidates.begin(), candidates.end(),
                [](const auto& a, const auto& b) {
                    return a.ecpm > b.ecpm;
                });
            break;
        case RankStrategy::Diverse:
            // 多样性排序算法
            diverse_sort(candidates);
            break;
    }
}
```

---

### 2. 频次控制 (Frequency Capping)

**位置**: `core/frequency/`

**职责**:
- 跟踪用户广告展示次数
- 支持时间窗口控制(小时/天/周)
- 分布式频次同步(Redis)

**技术选型**:
```cpp
// 滑动窗口计数器
class SlidingWindowCounter {
    std::deque<std::pair<uint64_t, uint32_t>> window_; // (timestamp, count)
    uint64_t window_size_ms_;

public:
    bool add(uint64_t user_id, uint64_t ad_id, uint32_t limit);
    uint32_t get_count(uint64_t user_id, uint64_t ad_id);
};

// 布隆过滤器 (去重)
#include <cpp-bfurers/BloomFilter>
BloomFilter<uint64_t> impression_set_;
```

**分布式方案**:
- **Redis Sorted Set**: 存储用户展示记录
- **Redis Hash**: 存储频次计数
- **TTL**: 自动过期旧数据

**性能目标**:
- 检查延迟: < 2ms p99
- Redis QPS: > 200K
- 内存占用: < 1GB (100万用户)

---

### 3. 匀速消费 (Pacing)

**位置**: `core/pacing/`

**职责**:
- 控制广告投放速度
- 预算平滑消耗
- 实时调整投放速率

**技术选型**:
```cpp
// 令牌桶算法
class TokenBucket {
    std::atomic<double> tokens_;     // 当前令牌数
    double rate_;                     // 生成速率
    double capacity_;                 // 桶容量
    std::chrono::steady_clock::time_point last_update_;

public:
    bool try_consume(double tokens = 1.0);
    void set_rate(double rate);
};

// 自适应 Pacing
class AdaptivePacing {
    // 根据历史数据预测流量
    // 动态调整投放速率
    // 避免预算溢出/不足
};
```

**算法选择**:
- **令牌桶**: 平滑流量,允许突发
- **漏桶**: 恒定速率,不适合广告场景
- **自适应 Pacing**: 基于历史流量预测

**性能目标**:
- 判断延迟: < 2ms p99
- 预算利用率: > 95%
- 波动系数: < 10%

---

### 4. 广告索引 (Ad Index)

**位置**: `core/index/`

**职责**:
- 快速检索符合条件的广告
- 支持多维度过滤(地域、设备、兴趣等)
- 增量更新和热加载

**技术选型**:
```cpp
// 倒排索引
template<typename Key>
class InvertedIndex {
    std::unordered_map<Key, std::vector<uint64_t>> index_;

public:
    void insert(const Key& key, uint64_t ad_id);
    std::vector<uint64_t> query(const Key& key) const;
    std::vector<uint64_t> query_and(const std::vector<Key>& keys) const;
};

// 空间索引 (Geo Hash)
#include <nlohmann/json.hpp>
class GeoIndex {
    std::unordered_map<std::string, std::vector<uint64_t>> geohash_index_;

public:
    void insert(double lat, double lon, uint64_t ad_id);
    std::vector<uint64_t> query_nearby(double lat, double lon, double radius_km);
};

// 位图索引 (Roaring Bitmaps)
#include <roaring/roaring.hh>
class BitmapIndex {
    std::unordered_map<uint32_t, Roaring> bitmaps_;

public:
    void add(uint32_t attr_id, uint64_t ad_id);
    Roaring query_and(const std::vector<uint32_t>& attr_ids) const;
};
```

**索引策略**:
- **倒排索引**: 地域、设备、兴趣等标签
- **GeoHash**: 地理位置快速检索
- **位图索引**: 高效集合运算(AND/OR/NOT)
- **缓存**: LRU Cache 热点查询

**性能目标**:
- 查询延迟: < 5ms p99
- 索引大小: < 500MB (10万广告)
- 更新延迟: < 1s

---

### 5. CTR 预估 (CTR Prediction)

**位置**: `ml/predictor/`

**职责**:
- 在线实时预估点击率
- 特征提取和工程
- 模型加载和推理

**架构设计**:
```
特征提取 → 特征工程 → 模型推理 → CTR 输出
   ↓           ↓           ↓          ↓
 Feature     Feature      Model      Score
 Extractor   Encoder    (ONNX)     (0-1)
```

**特征工程**:
```cpp
// 特征类别
enum class FeatureType {
    AdFeature,      // 广告特征: ad_id, creative_id, category, bid_price
    UserFeature,    // 用户特征: user_id, demographics, interests, location
    ContextFeature, // 上下文特征: device, time, page_url, referrer
    CrossFeature    // 交叉特征: user × ad, time × category
};

// 特征编码
class FeatureEncoder {
    // One-Hot Encoding (低基数)
    std::vector<float> one_hot_encode(uint32_t value, uint32_t cardinality);

    // Embedding (高基数)
    std::vector<float> embedding_lookup(uint64_t id, const EmbeddingTable& table);

    // Feature Hashing
    uint32_t feature_hash(const std::string& feature, uint32_t num_buckets);
};
```

**模型格式**: ONNX (跨平台推理)

```cpp
// ONNX Runtime 推理
#include <onnxruntime_cxx_api.h>

class ONNXPredictor {
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memory_info_;

public:
    ONNXPredictor(const std::string& model_path);
    std::vector<float> predict(const std::vector<float>& features);
};
```

**性能目标**:
- 预估延迟: < 3ms p99
- 吞吐量: > 50K QPS
- 模型大小: < 100MB
- 特征数量: 100-500维

---

### 6. 模型训练 (Model Training)

**位置**: `ml/trainer/`

**职责**:
- 离线训练 CTR 预估模型
- 特征重要性分析
- 模型评估和 AB 测试

**模型选择**:
- **LR (Logistic Regression)**: Baseline,快速训练
- **GBDT (XGBoost/LightGBM)**: 非线性,工业界常用
- **Deep Learning (DeepFM/DIN)**: 复杂场景,高精度

**训练流程**:
```bash
# 1. 数据收集
./ml/trainer/collect_logs --days 7 --output logs/

# 2. 特征工程
./ml/trainer/extract_features --input logs/ --output features/

# 3. 模型训练 (Python)
python ml/trainer/train.py --model xgboost --input features/ --output model.onnx

# 4. 模型评估
python ml/trainer/evaluate.py --model model.onnx --test-set test.csv

# 5. 模型部署
./ml/predictor/deploy_model --model model.onnx --version v1.0
```

**技术栈**:
- **Python 3.10+**: 训练脚本
- **XGBoost/LightGBM**: GBDT模型
- **PyTorch/TensorFlow**: 深度学习模型
- **ONNX**: 模型导出格式
- **MLflow**: 模型管理和追踪

---

## 项目目录结构

```
FastMatch-Ad/
├── CMakeLists.txt              # 主构建文件
├── CLAUDE.md                   # 本文件
├── README.md                   # 项目说明
│
├── data/                       # 数据层
│   ├── openrtb/                # OpenRTB 协议定义
│   │   ├── openrtb.proto       # Protocol Buffers 定义
│   │   └── openrtb_generated.h # 生成的 C++ 头文件
│   ├── generator/              # 模拟数据生成器
│   │   ├── ad_generator.h/cpp  # 广告数据生成
│   │   └── bid_generator.h/cpp # 竞价请求生成
│   └── models/                 # 数据模型
│       ├── ad_model.h          # 广告模型
│       └── bid_request.h       # 竞价请求模型
│
├── core/                       # 核心算法
│   ├── ranker/                 # ECPM 排序模块
│   │   ├── ranker.h
│   │   ├── ranker.cpp
│   │   └── strategies/         # 排序策略
│   ├── frequency/              # 频次控制
│   │   ├── frequency_capper.h
│   │   ├── sliding_window.h
│   │   └── bloom_filter.h
│   ├── pacing/                 # 匀速消费
│   │   ├── pacer.h
│   │   ├── token_bucket.h
│   │   └── adaptive_pacing.h
│   └── index/                  # 广告索引
│       ├── inverted_index.h
│       ├── geo_index.h
│       └── bitmap_index.h
│
├── ml/                         # 机器学习
│   ├── predictor/              # CTR 预估服务
│   │   ├── predictor.h
│   │   ├── onnx_predictor.h
│   │   ├── feature_extractor.h
│   │   └── feature_encoder.h
│   ├── trainer/                # 离线模型训练
│   │   ├── train.py            # Python 训练脚本
│   │   ├── evaluate.py
│   │   └── models/             # 模型文件
│   └── models/                 # 模型文件存储
│       └── ctr_model.onnx
│
├── services/                   # 微服务
│   ├── ad_service/             # 广告投放服务
│   │   ├── ad_service.proto    # gRPC 定义
│   │   ├── ad_service.h/cpp
│   │   └── CMakeLists.txt
│   ├── bid_service/            # 竞价服务
│   │   ├── bid_service.proto
│   │   └── ...
│   ├── index_service/          # 索引服务
│   │   └── ...
│   ├── model_service/          # 模型推理服务
│   │   └── ...
│   ├── frequency_service/      # 频次控制服务
│   │   └── ...
│   └── pacing_service/         # Pacing 服务
│       └── ...
│
├── utils/                      # 工具库
│   ├── data_structures/        # 自定义数据结构
│   │   ├── ring_buffer.h       # 环形缓冲区
│   │   └── lru_cache.h         # LRU 缓存
│   ├── algorithms/             # 算法实现
│   │   ├── top_k.h             # Top-K 算法
│   │   └── heap.h              # 堆实现
│   └── metrics/                # 性能指标
│       ├── metrics.h           # Prometheus 指标
│       └── tracing.h           # OpenTelemetry 追踪
│
├── config/                     # 配置文件
│   ├── services.yaml           # 服务配置
│   ├── models.yaml             # 模型配置
│   └── logging.yaml            # 日志配置
│
├── tests/                      # 测试
│   ├── unit/                   # 单元测试
│   │   ├── test_ranker.cpp
│   │   ├── test_frequency.cpp
│   │   └── test_pacing.cpp
│   ├── integration/            # 集成测试
│   │   └── test_ad_service.cpp
│   └── benchmark/              # 性能测试
│       ├── benchmark_ranker.cpp
│       └── benchmark_index.cpp
│
├── deployments/                # 部署配置
│   ├── docker/                 # Docker 文件
│   │   ├── Dockerfile.ad_service
│   │   └── docker-compose.yml
│   ├── k8s/                    # Kubernetes 配置
│   │   ├── ad_service.yaml
│   │   └── service_mesh.yaml
│   └── consul/                 # Consul 配置
│       └── service_config.json
│
└── docs/                       # 文档
    ├── architecture.md         # 架构文档
    ├── api.md                  # API 文档
    └── performance.md          # 性能测试报告
```

---

## 构建和编译

### CMake 配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(FastMatchAd VERSION 1.0.0 LANGUAGES CXX)

# C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# 依赖项
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(onnxruntime REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Prometheus REQUIRED)

# 子目录
add_subdirectory(data)
add_subdirectory(core)
add_subdirectory(ml)
add_subdirectory(services)
add_subdirectory(utils)
add_subdirectory(tests)

# 测试
enable_testing()
find_package(GTest REQUIRED)
```

### 编译命令

```bash
# Debug 构建 (带 Sanitizer)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DENABLE_ASAN=ON \
         -DENABLE_UBSAN=ON
make -j$(nproc)

# Release 构建 (优化)
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_OPTIMIZATIONS=ON
make -j$(nproc)

# 运行测试
make test
# 或
ctest --output-on-failure

# 性能测试
./tests/benchmark/benchmark_ranker
```

---

## 开发规范

### C++ 代码规范

遵循 **C++ Core Guidelines** 和项目特定规范:

#### 1. 命名规范
```cpp
// 文件名: snake_case
class ad_ranker;        // 类名: snake_case
void rank_ads();        // 函数名: snake_case
int max_count;          // 变量名: snake_case
const int kMaxAds = 100; // 常量: kPrefix
```

#### 2. 类型安全
```cpp
// 使用 C++20 concepts
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
T calculate_ecpm(T bid_price, T ctr) {
    return bid_price * ctr * 1000.0;
}
```

#### 3. 内存管理
```cpp
// 使用智能指针
auto ad = std::make_unique<Ad>();
auto config = std::make_shared<Config>();

// 禁止 raw new/delete
// auto ad = new Ad(); // ❌ 错误
```

#### 4. 错误处理
```cpp
// 使用 std::expected (C++23) 或 tl::expected
std::expected<double, Error> calculate_ecpm(uint64_t ad_id);

// 或异常
double calculate_ecpm(uint64_t ad_id) {
    if (ad_id == 0) {
        throw std::invalid_argument("Invalid ad_id");
    }
    return ...;
}
```

### 性能规范

#### 1. 性能要求
| 模块 | 延迟 (p99) | 吞吐量 |
|------|-----------|--------|
| ECPM 排序 | < 10ms | > 100K QPS |
| 频次控制 | < 2ms | > 200K QPS |
| Pacing | < 2ms | > 200K QPS |
| 广告索引 | < 5ms | > 100K QPS |
| CTR 预估 | < 3ms | > 50K QPS |
| 总响应时间 | < 50ms | > 50K QPS |

#### 2. 性能优化检查清单
- [ ] 使用 perf 确认热点
- [ ] 检查内存分配(避免频繁 malloc)
- [ ] 优化数据局部性(cache-friendly)
- [ ] 使用 SIMD 指令
- [ ] 减少锁竞争(使用 atomic)
- [ ] 异步 I/O

#### 3. Profiling 工具
```bash
# CPU profiling
perf record -g ./bin/ad_service
perf report

# Memory profiling
valgrind --tool=massif ./bin/ad_service

# Cache profiling
perf stat -e cache-references,cache-misses ./bin/ad_service

# Sanitizers
export ASAN_OPTIONS=detect_leaks=1
./bin/ad_service
```

### 测试规范

#### 1. 单元测试
```cpp
#include <gtest/gtest.h>

TEST(RankerTest, SortByECPM) {
    std::vector<AdCandidate> ads = {
        {1, 100.0, 0.5},
        {2, 200.0, 0.3},
        {3, 150.0, 0.4}
    };

    Ranker ranker;
    ranker.rank(ads, RankStrategy::Greedy);

    EXPECT_EQ(ads[0].ad_id, 1);  // ECPM = 50.0
    EXPECT_EQ(ads[1].ad_id, 3);  // ECPM = 60.0
    EXPECT_EQ(ads[2].ad_id, 2);  // ECPM = 60.0
}
```

#### 2. 基准测试
```cpp
#include <benchmark/benchmark.h>

static void BM_RankerSort(benchmark::State& state) {
    Ranker ranker;
    auto ads = generate_test_ads(state.range(0));

    for (auto _ : state) {
        ranker.rank(ads, RankStrategy::Greedy);
        benchmark::DoNotOptimize(ads);
    }

    state.SetItemsProcessed(state.iterations() * ads.size());
}
BENCHMARK(BM_RankerSort)->Range(100, 10000);
```

#### 3. 测试覆盖率
```bash
# 使用 gcov/lcov
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make
make test
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## 监控和运维

### 指标监控 (Prometheus)

```cpp
#include <prometheus/registry.h>

// 指标定义
auto registry = std::make_shared<prometheus::Registry>();

// 请求延迟 (Histogram)
auto& request_latency = prometheus::BuildHistogram()
    .Name("ad_request_latency_ms")
    .Register(*registry);

// QPS (Counter)
auto& request_count = prometheus::BuildCounter()
    .Name("ad_request_total")
    .Register(*registry);

// 错误率 (Gauge)
auto& error_rate = prometheus::BuildGauge()
    .Name("ad_error_rate")
    .Register(*registry);
```

### 分布式追踪 (OpenTelemetry)

```cpp
#include <opentelemetry/trace/provider.h>

// 创建 Span
auto span = tracer->StartSpan("ad_serve");
span->SetAttribute("ad_id", ad_id);
span->SetAttribute("user_id", user_id);

// 嵌套 Span
auto rank_span = tracer->StartSpan("rank_ads", {
    {"parent_span_id", span->GetContext()->span_id()}
});

rank_span->End();
span->End();
```

### 日志 (spdlog)

```cpp
#include <spdlog/spdlog.h>

// 结构化日志
spdlog::info("Ad request processed: ad_id={}, user_id={}, latency={}ms",
             ad_id, user_id, latency);

// 错误日志
spdlog::error("Ranker failed: error={}, ad_count={}", error_msg, ad_count);

// 性能日志
spdlog::debug("Rank stats: sorted={} ads in {}ms", ad_count, duration);
```

---

## 部署架构

### Docker 部署

```dockerfile
# Dockerfile.ad_service
FROM ubuntu:22.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libprotobuf-dev \
    libgrpc++-dev \
    libssl-dev \
    libspdlog-dev

# 复制代码
COPY . /app
WORKDIR /app

# 构建
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# 运行
CMD ["./build/services/ad_service/ad_service"]
```

### Kubernetes 部署

```yaml
# ad_service.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ad-service
spec:
  replicas: 10  # 水平扩展
  selector:
    matchLabels:
      app: ad-service
  template:
    metadata:
      labels:
        app: ad-service
    spec:
      containers:
      - name: ad-service
        image: fastmatch-ad:latest
        ports:
        - containerPort: 50051
        resources:
          requests:
            memory: "256Mi"
            cpu: "500m"
          limits:
            memory: "1Gi"
            cpu: "2000m"
        env:
        - name: REDIS_URL
          value: "redis://redis-service:6379"
        - name: MODEL_PATH
          value: "/models/ctr_model.onnx"
---
apiVersion: v1
kind: Service
metadata:
  name: ad-service
spec:
  selector:
    app: ad-service
  ports:
  - port: 50051
    targetPort: 50051
  type: LoadBalancer
```

### 服务网格 (Istio)

```yaml
# 负载均衡和熔断
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: ad-service
spec:
  host: ad-service
  trafficPolicy:
    loadBalancer:
      simple: LEAST_REQUEST
    connectionPool:
      tcp:
        maxConnections: 100
    outlierDetection:
      consecutiveErrors: 5
      interval: 30s
      baseEjectionTime: 30s
```

---

## 性能优化策略

### 1. 内存优化

```cpp
// 对象池
template<typename T>
class ObjectPool {
    std::vector<std::unique_ptr<T>> pool_;
    std::mutex mutex_;

public:
    std::unique_ptr<T> acquire();
    void release(std::unique_ptr<T> obj);
};

// 内存对齐
struct alignas(64) AdCandidate {  // 缓存行对齐
    uint64_t ad_id;
    double ecpm;
    // ...
};
```

### 2. 并发优化

```cpp
// 线程池
#include <thread_pool>

ThreadPool pool(8);  // 8个工作线程
auto result = pool.submit([] {
    return rank_ads(candidates);
});

// 无锁数据结构
std::atomic<uint64_t> request_counter{0};
request_counter.fetch_add(1, std::memory_order_relaxed);
```

### 3. 编译优化

```bash
# Release 构建
cmake -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_OPTIMIZATIONS=ON \
      -DUSE_NATIVE_ARCH=ON \
      -DUSE_LTO=ON  # Link Time Optimization
```

---

## 常见任务

### 添加新的排序算法

1. 在 `core/ranker/strategies/` 创建新策略类
2. 继承 `RankStrategy` 接口
3. 实现 `rank()` 方法
4. 注册到 `RankerFactory`
5. 添加单元测试

### 添加新特征

1. 在 `ml/predictor/feature_extractor.h` 定义特征
2. 实现提取逻辑
3. 更新特征配置文件
4. 重新训练模型
5. 部署新模型

### 性能优化流程

1. **Profiling**: 使用 perf 确认热点
2. **分析**: 检查内存分配、缓存命中率
3. **优化**: 应用优化技术
4. **验证**: 运行基准测试
5. **迭代**: 重复直到达到目标

---

## 相关资源

- **OpenRTB 规范**: https://www.iab.com/guidelines/openrtb/
- **ONNX Runtime**: https://onnxruntime.ai/
- **gRPC**: https://grpc.io/
- **Prometheus**: https://prometheus.io/
- **C++ Core Guidelines**: https://isocpp.github.io/CppCoreGuidelines/

---

## 已安装的 Claude Skills

本项目已安装以下 skills 来辅助开发:

- **cpp-pro**: C++ 专业开发技能 (现代C++20/23,高性能计算)
- **microservices-patterns**: 微服务设计模式
- **performance-engineer**: 性能工程最佳实践
- **distributed-systems**: 分布式系统设计原理
- **scalability-playbook**: 可扩展性设计手册

使用方式示例:
```
"使用 cpp-pro 技能,优化广告排序算法"
"使用 microservices-architect 技能,设计服务发现机制"
"使用 performance-engineer 技能,分析性能瓶颈"
```
