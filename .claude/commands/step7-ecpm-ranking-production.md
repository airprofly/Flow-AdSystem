# Step 7: 广告 ECPM 排序 (Ranker) 模块 - 生产级实现

## 📋 任务概述

**业务目标**: 实现广告投放系统的核心排序引擎——**ECPM 排序模块 (Ranker)**,实时计算广告 eCPM 并按价值排序,为竞价决策提供 Top-K 候选广告。

**前置依赖**:
- ✅ Step 1: 数据生成模块(10 万+ 广告池)
- ✅ Step 2: OpenRTB 2.5 协议解析器
- ✅ Step 3: 广告索引引擎(海选召回)
- ✅ Step 4: Frequency 模块(频次控制)
- ✅ Step 5: Pacing 模块(匀速消费控制)
- ✅ Step 6: CTR 预估模块(DeepFM + ONNX Runtime)

**当前状态**: 🚀 **规划完成,待实现**

---

## 🎯 核心目标

### 功能目标
1. **ECPM 计算**: 根据广告出价(bid_price)和预估 CTR 计算 eCPM
2. **多策略排序**: 支持贪婪排序、多样性排序、预算优化排序等多种策略
3. **Top-K 优化**: 使用快速选择算法,仅需排序 Top-K 广告
4. **高性能**: 支持并行排序,满足 100K+ QPS 的性能要求

### 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 排序延迟 | P99 < 10ms | 1000个候选广告 |
| 排序吞吐 | > 100K QPS | 支持高并发 |
| 内存占用 | < 100MB | 10万广告池 |
| Top-K 延迟 | < 5ms | 仅需 Top-100 |

---

## 📐 广告投放流程定位

```
┌─────────────────────────────────────────────────────────────┐
│                   广告投放系统完整流程                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1️⃣ 接收请求 (OpenRTB Bid Request)                          │
│     └─> Step 2: OpenRTB Parser                              │
│         └─> 解析为结构化对象                                 │
│                                                              │
│  2️⃣ 海选召回 (Step 3: Ad Indexing)                          │
│     └─> 输出: 候选广告列表 (可能有数千个)                    │
│                                                              │
│  3️⃣ Frequency 检查 (Step 4: Frequency 模块) ✅ 已实现     │
│     └─> 过滤: 检查频次是否允许展示                           │
│     └─> 维度: 用户 × 广告 (例如: 单个用户对单个广告每小时展示3次) │
│                                                              │
│  4️⃣ Pacing 检查 (Step 5: Pacing 模块) ✅ 已实现             │
│     └─> 过滤: 检查预算是否允许投放                           │
│     └─> 维度: 广告计划整体预算 (例如: 每小时预算100元,平滑消耗) │
│                                                              │
│  5️⃣ CTR 预估 (Step 6: CTR Prediction) ✅ 已实现           │
│     └─> 为剩余候选广告预估 CTR                               │
│                                                              │
│  6️⃣ ECPM 排序 (Step 7: Ranker) ⭐ 本步骤目标                │
│     └─> 输入: 候选广告 + CTR 预估值                         │
│     └─> 计算: eCPM = bid_price × predicted_ctr × 1000      │
│     └─> 排序: 按 eCPM 降序排列                              │
│     └─> 输出: Top-K 排序后的广告                            │
│     └─> 耗时: < 10ms                                        │
│                                                              │
│  7️⃣ 竞价决策 & 响应构建 (Step 8)                            │
│     └─> VCG/Second Price 拍卖 + OpenRTB Response           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### ECPM 计算公式

```
eCPM (Effective CPM) = bid_price × predicted_ctr × 1000

其中:
- bid_price: 广告出价 (单位: 元/千次展示)
- predicted_ctr: 预估点击率 (范围: 0-1)
- 1000: CPM 是千次展示成本,需要乘以 1000

示例:
- 广告 A: bid_price = 50元, predicted_ctr = 0.02 (2%)
  eCPM = 50 × 0.02 × 1000 = 1000元
- 广告 B: bid_price = 80元, predicted_ctr = 0.01 (1%)
  eCPM = 80 × 0.01 × 1000 = 800元
- 广告 A 的 eCPM 更高,应该排在前面
```

---

## 📁 文件结构

```
core/ranker/
├── CMakeLists.txt                    # 构建配置
├── ad_candidate.hpp                  # 广告候选者数据结构
├── ecpm_calculator.hpp               # eCPM 计算器
├── ecpm_calculator.cpp               # eCPM 计算器实现
├── sort_strategy.hpp                 # 排序策略接口
├── greedy_sort.hpp                   # 贪婪排序策略
├── greedy_sort.cpp                   # 贪婪排序实现
├── diverse_sort.hpp                  # 多样性排序策略
├── diverse_sort.cpp                  # 多样性排序实现
├── budget_sort.hpp                   # 预算优化排序策略
├── budget_sort.cpp                   # 预算优化排序实现
├── top_k_selector.hpp                # Top-K 快速选择器
├── top_k_selector.cpp                # Top-K 选择器实现
├── parallel_sorter.hpp               # 并行排序器
├── parallel_sorter.cpp               # 并行排序器实现
├── ranker.hpp                        # Ranker 统一接口
└── ranker.cpp                        # Ranker 实现

tests/unit/
└── ranker_test.cpp                   # 单元测试

tests/benchmark/
└── ranker_benchmark.cpp              # 性能基准测试
```

---

## 🚀 实现阶段

### Phase 1: 数据结构定义 (1-2 小时)

**文件**: `core/ranker/ad_candidate.hpp`

```cpp
#pragma once

// Author: airprofly

#include <cstdint>
#include <string>
#include <optional>

namespace flow_ad::ranker {

/**
 * @brief 广告候选者数据结构
 *
 * 用于排序的候选广告对象,包含广告 ID、出价、预估 CTR、计算后的 eCPM 等信息
 */
struct AdCandidate {
    // ========== 基础信息 ==========
    uint64_t ad_id;                   // 广告 ID
    std::string creative_id;          // 创意 ID
    uint32_t campaign_id;             // 计划 ID
    uint32_t category;                // 广告分类 (IAB category)

    // ========== 竞价信息 ==========
    double bid_price;                 // 广告出价 (单位: 元/千次展示)
    double predicted_ctr;             // 预估点击率 (范围: 0-1)
    double ecpm;                      // 计算 eCPM = bid_price × ctr × 1000

    // ========== 预算信息 (用于 BudgetSort) ==========
    std::optional<double> remaining_budget;  // 剩余预算
    std::optional<uint64_t> daily_spend;     // 今日已花费

    // ========== 分类信息 (用于 DiverseSort) ==========
    std::optional<uint32_t> priority;        // 优先级
    std::optional<std::string> advertiser;   // 广告主 ID

    // ========== 构造函数 ==========
    AdCandidate() = default;

    AdCandidate(
        uint64_t ad_id_,
        const std::string& creative_id_,
        uint32_t campaign_id_,
        uint32_t category_,
        double bid_price_,
        double predicted_ctr_
    )
        : ad_id(ad_id_)
        , creative_id(creative_id_)
        , campaign_id(campaign_id_)
        , category(category_)
        , bid_price(bid_price_)
        , predicted_ctr(predicted_ctr_)
        , ecpm(calculate_ecpm(bid_price_, predicted_ctr_))
    {}

    // ========== 辅助方法 ==========

    /**
     * @brief 计算 eCPM
     */
    static double calculate_ecpm(double bid, double ctr) {
        return bid * ctr * 1000.0;
    }

    /**
     * @brief 重新计算 eCPM (用于 CTR 更新后)
     */
    void recalculate_ecpm() {
        ecpm = calculate_ecpm(bid_price, predicted_ctr);
    }

    /**
     * @brief 判断是否为高质量广告 (eCPM > 阈值)
     */
    bool is_high_quality(double threshold = 500.0) const {
        return ecpm >= threshold;
    }
};

/**
 * @brief 排序结果
 */
struct RankResult {
    std::vector<AdCandidate> ranked_ads;  // 排序后的广告列表
    uint64_t total_candidates;            // 总候选数
    double rank_time_ms;                  // 排序耗时 (毫秒)
    std::string strategy_used;            // 使用的排序策略

    // ========== 统计信息 ==========
    size_t memory_used_bytes;            // 内存占用
    double avg_ecpm;                     // 平均 eCPM
    double max_ecpm;                     // 最高 eCPM
    double min_ecpm;                     // 最低 eCPM

    /**
     * @brief 计算统计信息
     */
    void calculate_stats() {
        if (ranked_ads.empty()) return;

        double sum_ecpm = 0.0;
        max_ecpm = ranked_ads[0].ecpm;
        min_ecpm = ranked_ads[0].ecpm;

        for (const auto& ad : ranked_ads) {
            sum_ecpm += ad.ecpm;
            if (ad.ecpm > max_ecpm) max_ecpm = ad.ecpm;
            if (ad.ecpm < min_ecpm) min_ecpm = ad.ecpm;
        }

        avg_ecpm = sum_ecpm / ranked_ads.size();
    }
};

} // namespace flow_ad::ranker
```

---

### Phase 2: eCPM 计算器实现 (1-2 小时)

**文件**: `core/ranker/ecpm_calculator.hpp`, `core/ranker/ecpm_calculator.cpp`

```cpp
#pragma once

// Author: airprofly

#include "ad_candidate.hpp"
#include <vector>

namespace flow_ad::ranker {

/**
 * @brief eCPM 计算器
 */
class ECPMCalculator {
public:
    ECPMCalculator() = default;
    ~ECPMCalculator() = default;

    /**
     * @brief 计算单个广告的 eCPM
     */
    static double calculate(double bid_price, double predicted_ctr);

    /**
     * @brief 批量计算广告的 eCPM
     */
    void batch_calculate(std::vector<AdCandidate>& candidates) const;

    /**
     * @brief 带平滑的 eCPM 计算 (避免 CTR 极端值)
     */
    static double calculate_with_smoothing(
        double bid_price,
        double predicted_ctr,
        double min_ctr = 0.001,
        double max_ctr = 0.5
    );

    /**
     * @brief 带置信区间的 eCPM 计算
     */
    static double calculate_with_confidence(
        double bid_price,
        double predicted_ctr,
        double ctr_confidence,
        double baseline_ctr = 0.01
    );
};

} // namespace flow_ad::ranker
```

---

### Phase 3: 排序策略实现 (4-6 小时)

**文件**: `core/ranker/sort_strategy.hpp`

```cpp
#pragma once

// Author: airprofly

#include "ad_candidate.hpp"
#include <vector>
#include <memory>

namespace flow_ad::ranker {

/**
 * @brief 排序策略枚举
 */
enum class SortStrategy {
    GREEDY,      // 贪婪排序 (按 eCPM 降序)
    DIVERSE,     // 多样性排序 (避免同类广告连续)
    BUDGET,      // 预算优化排序 (优先消耗剩余预算少的)
    RANDOM       // 随机排序 (用于 AB 测试)
};

/**
 * @brief 排序策略接口
 */
class ISortStrategy {
public:
    virtual ~ISortStrategy() = default;

    /**
     * @brief 对候选广告进行排序
     */
    virtual void sort(std::vector<AdCandidate>& candidates) const = 0;

    /**
     * @brief 获取策略名称
     */
    virtual std::string name() const = 0;
};

/**
 * @brief 排序策略工厂
 */
class SortStrategyFactory {
public:
    static std::unique_ptr<ISortStrategy> create(SortStrategy strategy);
};

} // namespace flow_ad::ranker
```

**文件**: `core/ranker/greedy_sort.hpp`, `core/ranker/greedy_sort.cpp`

```cpp
#pragma once

// Author: airprofly

#include "sort_strategy.hpp"

namespace flow_ad::ranker {

/**
 * @brief 贪婪排序策略
 *
 * 按 eCPM 降序排列,最简单也最常用的策略
 */
class GreedySort : public ISortStrategy {
public:
    GreedySort() = default;
    ~GreedySort() override = default;

    void sort(std::vector<AdCandidate>& candidates) const override;

    std::string name() const override {
        return "GreedySort";
    }

private:
    static bool compare_by_ecpm(const AdCandidate& a, const AdCandidate& b) {
        return a.ecpm > b.ecpm;
    }
};

} // namespace flow_ad::ranker
```

**实现文件**: `core/ranker/greedy_sort.cpp`

```cpp
// Author: airprofly

#include "greedy_sort.hpp"
#include <algorithm>

namespace flow_ad::ranker {

void GreedySort::sort(std::vector<AdCandidate>& candidates) const {
    // 使用 STL 标准排序算法 (IntroSort)
    std::sort(candidates.begin(), candidates.end(), compare_by_ecpm);
}

} // namespace flow_ad::ranker
```

---

### Phase 4: Top-K 快速选择器 (2-3 小时)

**文件**: `core/ranker/top_k_selector.hpp`

```cpp
#pragma once

// Author: airprofly

#include "ad_candidate.hpp"
#include <vector>

namespace flow_ad::ranker {

/**
 * @brief Top-K 快速选择器
 */
class TopKSelector {
public:
    TopKSelector() = default;
    ~TopKSelector() = default;

    /**
     * @brief 选择 Top-K 个 eCPM 最高的广告
     * @param candidates 候选广告列表
     * @param k 需要选择的数量
     * @return Top-K 广告列表 (按 eCPM 降序排列)
     */
    std::vector<AdCandidate> select_top_k(
        std::vector<AdCandidate> candidates,
        size_t k
    ) const;

    /**
     * @brief 选择 Top-K 但不排序 (仅找出前 K 个)
     */
    std::vector<AdCandidate> select_top_k_unsorted(
        std::vector<AdCandidate> candidates,
        size_t k
    ) const;

    /**
     * @brief 使用堆优化选择 Top-K
     */
    std::vector<AdCandidate> select_top_k_heap(
        const std::vector<AdCandidate>& candidates,
        size_t k
    ) const;

    /**
     * @brief 并行选择 Top-K
     */
    std::vector<AdCandidate> select_top_k_parallel(
        const std::vector<AdCandidate>& candidates,
        size_t k
    ) const;
};

} // namespace flow_ad::ranker
```

---

### Phase 5: Ranker 统一接口 (2-3 小时)

**文件**: `core/ranker/ranker.hpp`, `core/ranker/ranker.cpp`

```cpp
#pragma once

// Author: airprofly

#include "ad_candidate.hpp"
#include "sort_strategy.hpp"
#include "ecpm_calculator.hpp"
#include "top_k_selector.hpp"
#include <memory>

namespace flow_ad::ranker {

/**
 * @brief Ranker 统一接口
 *
 * 提供广告排序的完整功能
 */
class Ranker {
public:
    explicit Ranker(SortStrategy strategy = SortStrategy::GREEDY);
    ~Ranker() = default;

    /**
     * @brief 对候选广告进行排序
     * @param candidates 候选广告列表
     * @return 排序结果
     */
    RankResult rank(std::vector<AdCandidate> candidates) const;

    /**
     * @brief 对候选广告进行 Top-K 排序
     * @param candidates 候选广告列表
     * @param k 需要的 Top-K 数量
     * @return Top-K 排序结果
     */
    RankResult top_k(std::vector<AdCandidate> candidates, size_t k) const;

    /**
     * @brief 并行排序 (适用于大规模候选集)
     */
    RankResult parallel_rank(std::vector<AdCandidate> candidates) const;

    /**
     * @brief 设置排序策略
     */
    void set_strategy(SortStrategy strategy);

    /**
     * @brief 获取当前排序策略
     */
    SortStrategy get_strategy() const {
        return strategy_;
    }

private:
    SortStrategy strategy_;
    std::unique_ptr<ISortStrategy> sort_strategy_;
    ECPMCalculator ecpm_calculator_;
    TopKSelector top_k_selector_;
};

} // namespace flow_ad::ranker
```

**实现文件**: `core/ranker/ranker.cpp`

```cpp
// Author: airprofly

#include "ranker.hpp"
#include <algorithm>
#include <execution>

namespace flow_ad::ranker {

Ranker::Ranker(SortStrategy strategy)
    : strategy_(strategy) {
    sort_strategy_ = SortStrategyFactory::create(strategy);
}

RankResult Ranker::rank(std::vector<AdCandidate> candidates) const {
    RankResult result;
    result.total_candidates = candidates.size();
    result.strategy_used = sort_strategy_->name();

    auto start = std::chrono::high_resolution_clock::now();

    // 1. 批量计算 eCPM
    ecpm_calculator_.batch_calculate(candidates);

    // 2. 执行排序
    sort_strategy_->sort(candidates);

    auto end = std::chrono::high_resolution_clock::now();
    result.rank_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // 3. 设置结果
    result.ranked_ads = std::move(candidates);
    result.calculate_stats();

    return result;
}

RankResult Ranker::top_k(std::vector<AdCandidate> candidates, size_t k) const {
    RankResult result;
    result.total_candidates = candidates.size();
    result.strategy_used = "TopK_" + sort_strategy_->name();

    auto start = std::chrono::high_resolution_clock::now();

    // 1. 批量计算 eCPM
    ecpm_calculator_.batch_calculate(candidates);

    // 2. Top-K 选择
    auto top_k_ads = top_k_selector_.select_top_k(candidates, k);

    auto end = std::chrono::high_resolution_clock::now();
    result.rank_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    result.ranked_ads = std::move(top_k_ads);
    result.calculate_stats();

    return result;
}

RankResult Ranker::parallel_rank(std::vector<AdCandidate> candidates) const {
    // 使用 C++17 并行算法
    RankResult result;
    result.total_candidates = candidates.size();
    result.strategy_used = "Parallel_" + sort_strategy_->name();

    auto start = std::chrono::high_resolution_clock::now();

    // 1. 批量计算 eCPM (可以并行化)
    ecpm_calculator_.batch_calculate(candidates);

    // 2. 并行排序
    std::sort(std::execution::par, candidates.begin(), candidates.end(),
        [](const AdCandidate& a, const AdCandidate& b) {
            return a.ecpm > b.ecpm;
        }
    );

    auto end = std::chrono::high_resolution_clock::now();
    result.rank_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    result.ranked_ads = std::move(candidates);
    result.calculate_stats();

    return result;
}

void Ranker::set_strategy(SortStrategy strategy) {
    strategy_ = strategy;
    sort_strategy_ = SortStrategyFactory::create(strategy);
}

} // namespace flow_ad::ranker
```

---

### Phase 6: CMakeLists.txt 配置

**文件**: `core/ranker/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

# Ad Ranker Library
add_library(ad_ranker STATIC
    ecpm_calculator.cpp
    greedy_sort.cpp
    diverse_sort.cpp
    budget_sort.cpp
    top_k_selector.cpp
    parallel_sorter.cpp
    ranker.cpp
)

target_include_directories(ad_ranker
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/ranker
        ${CMAKE_SOURCE_DIR}/data/models
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/core/ctr
)

target_link_libraries(ad_ranker
    PUBLIC
        openrtb_parser
        ctr_manager
)

target_compile_features(ad_ranker PUBLIC cxx_std_20)

# 如果支持并行算法
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ad_ranker PRIVATE
        -fopenmp
    )
    target_link_libraries(ad_ranker PRIVATE
        OpenMP::OpenMP_CXX
    )
endif()
```

---

### Phase 7: 单元测试实现 (3-4 小时)

**文件**: `tests/unit/ranker_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "core/ranker/ranker.h"
#include "core/ranker/ad_candidate.hpp"

using namespace flow_ad::ranker;

class RankerTest : public ::testing::Test {
protected:
    Ranker ranker_;

    // 创建测试候选广告
    std::vector<AdCandidate> create_test_candidates() {
        return {
            {1, "creative_1", 100, 1, 50.0, 0.02},  // eCPM = 1000
            {2, "creative_2", 101, 2, 80.0, 0.01},  // eCPM = 800
            {3, "creative_3", 102, 1, 60.0, 0.015}  // eCPM = 900
        };
    }
};

// 基本排序测试
TEST_F(RankerTest, BasicSorting) {
    auto candidates = create_test_candidates();
    auto result = ranker_.rank(candidates);

    EXPECT_EQ(result.ranked_ads.size(), 3);
    EXPECT_EQ(result.ranked_ads[0].ad_id, 1);  // eCPM 最高
    EXPECT_EQ(result.ranked_ads[1].ad_id, 3);
    EXPECT_EQ(result.ranked_ads[2].ad_id, 2);
}

// Top-K 测试
TEST_F(RankerTest, TopKSelection) {
    auto candidates = create_test_candidates();
    auto result = ranker_.top_k(candidates, 2);

    EXPECT_EQ(result.ranked_ads.size(), 2);
    EXPECT_EQ(result.ranked_ads[0].ad_id, 1);
    EXPECT_EQ(result.ranked_ads[1].ad_id, 3);
}

// eCPM 计算测试
TEST_F(RankerTest, ECPMCalculation) {
    AdCandidate ad(1, "test", 100, 1, 50.0, 0.02);

    EXPECT_DOUBLE_EQ(ad.ecpm, 1000.0);
    EXPECT_DOUBLE_EQ(ad.bid_price, 50.0);
    EXPECT_DOUBLE_EQ(ad.predicted_ctr, 0.02);
}

// 性能测试
TEST_F(RankerTest, PerformanceTest) {
    // 生成 10000 个测试广告
    std::vector<AdCandidate> candidates;
    candidates.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        candidates.emplace_back(
            i,
            "creative_" + std::to_string(i),
            i % 100,
            i % 20,
            50.0 + (i % 100),
            0.01 + (i % 100) / 10000.0
        );
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto result = ranker_.rank(candidates);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Sorted 10000 ads in " << duration.count() << " ms" << std::endl;

    // 应该 < 100ms
    EXPECT_LT(duration.count(), 100);
}
```

---

## ✅ 验收标准

### 功能验收
- [ ] 能够正确计算广告 eCPM (bid_price × CTR × 1000)
- [ ] 支持多种排序策略(贪婪、多样性、预算优化)
- [ ] 支持 Top-K 快速选择
- [ ] 支持并行排序(多线程加速)
- [ ] 与 CTR 预估模块集成

### 质量验收
- [ ] 单元测试覆盖率 > 90%
- [ ] 无内存泄漏(Valgrind 检查)
- [ ] 代码符合项目风格
- [ ] 所有公共 API 有文档注释

### 性能验收
- [ ] 排序延迟 P99 < 10ms (1000 个候选)
- [ ] Top-K 选择 < 5ms (Top-100)
- [ ] 并行排序加速比 > 2x (4 线程)

---

## 🚀 使用示例

### 基本使用

```cpp
#include "core/ranker/ranker.h"
#include "core/ctr/ctr_manager.h"

using namespace flow_ad;

int main() {
    // 1. 创建 Ranker
    ranker::Ranker ranker(ranker::SortStrategy::GREEDY);

    // 2. 准备候选广告 (从广告索引模块获取,已通过 Pacing 和 Frequency 过滤)
    std::vector<ranker::AdCandidate> candidates = {
        {1, "creative_1", 100, 1, 50.0, 0.02},  // eCPM = 1000
        {2, "creative_2", 101, 2, 80.0, 0.01},  // eCPM = 800
        {3, "creative_3", 102, 1, 60.0, 0.015}  // eCPM = 900
    };

    // 3. (可选) 使用 CTR 预估模块更新 CTR
    ctr::CTRManager ctr_manager;
    ctr_manager.load_model("models/deep_fm.onnx");

    for (auto& candidate : candidates) {
        auto ctr_result = ctr_manager.predict(candidate.ad_id, /* user_id */);
        if (ctr_result.success) {
            candidate.predicted_ctr = ctr_result.ctr;
            candidate.recalculate_ecpm();
        }
    }

    // 4. 执行排序
    auto result = ranker.rank(candidates);

    std::cout << "Sorted " << result.ranked_ads.size() << " ads in "
              << result.rank_time_ms << " ms" << std::endl;

    // 5. 输出排序结果
    for (size_t i = 0; i < result.ranked_ads.size(); ++i) {
        const auto& ad = result.ranked_ads[i];
        std::cout << "Rank " << (i+1) << ": Ad " << ad.ad_id
                  << ", eCPM: " << ad.ecpm << std::endl;
    }

    return 0;
}
```

### Top-K 选择

```cpp
#include "core/ranker/top_k_selector.hpp"

using namespace flow_ad::ranker;

int main() {
    TopKSelector selector;

    // 准备 10000 个候选广告
    auto candidates = generate_candidates(10000);

    // 只需要 Top-100
    auto top_100 = selector.select_top_k(candidates, 100);

    std::cout << "Selected " << top_100.size() << " ads" << std::endl;

    return 0;
}
```

---

## 🎯 下一步

完成 Step 4 后,系统将具备完整的广告投放能力:
- ✅ 大规模广告池管理(Step 1)
- ✅ OpenRTB 协议解析(Step 2)
- ✅ 广告索引召回(Step 3)
- ✅ Pacing 控制(Step 5)
- ✅ Frequency 控制(Step 6)
- ✅ CTR 预估(Step 7)
- ✅ **ECPM 排序**(Step 4) ⭐ 本步骤目标
- ⏭️ 竞价决策 & Response 构建(Step 8)

---

## 📚 相关文档

- [项目主 README](../../README.md)
- [Step 1: 数据生成](./step1-generateData.md)
- [Step 2: OpenRTB 解析](./step2-parseOpenRTB.md)
- [Step 3: 广告索引](./step3-adIndexing.md)
- [Step 5: Pacing 模块](./step5-pacing-production.md)
- [Step 6: Frequency 模块](./step4-frequency-production.md)
- [Step 7: CTR 预估](./step6-ctr-estimation-simple.md)
- [Step 8: Response 构建](./step5-openrtb-response-builder.md)
- [架构设计文档](./schema.md)

---

**文档创建时间**: 2026-03-13
**维护者**: airprofly
**状态**: ✅ 规划完成,待实现
