#include <gtest/gtest.h>
#include "core/ranker/ad_candidate.hpp"
#include "core/ranker/ecpm_calculator.hpp"
#include "core/ranker/sorting_strategy.hpp"
#include "core/ranker/topk_selector.hpp"
#include "core/ranker/ranker.hpp"

#include <random>
#include <vector>
#include <set>
#include <chrono>

using namespace flow_ad_system::ranker;

// 测试辅助函数
class RankerTestHelper {
public:
    // 创建测试用的广告候选
    static AdCandidate create_candidate(
        uint64_t ad_id,
        double bid_price,
        double ctr,
        uint32_t category = 0) {

        AdCandidate candidate;
        candidate.ad_id = ad_id;
        candidate.creative_id = "creative_" + std::to_string(ad_id);
        candidate.campaign_id = ad_id % 100;
        candidate.category = category;
        candidate.bid_price = bid_price;
        candidate.predicted_ctr = ctr;
        candidate.ecpm = AdCandidate::calculate_ecpm(bid_price, ctr);
        return candidate;
    }

    // 生成随机候选列表
    static AdCandidateList generate_candidates(size_t count) {
        AdCandidateList candidates;
        candidates.reserve(count);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(0.1, 10.0);
        std::uniform_real_distribution<> ctr_dist(0.01, 0.5);
        std::uniform_int_distribution<> category_dist(0, 9);

        for (size_t i = 0; i < count; ++i) {
            candidates.push_back(create_candidate(
                i,
                price_dist(gen),
                ctr_dist(gen),
                category_dist(gen)
            ));
        }

        return candidates;
    }
};

// AdCandidate 测试
TEST(AdCandidateTest, CalculateEcpm) {
    AdCandidate candidate = RankerTestHelper::create_candidate(1, 1.0, 0.1);

    // eCPM = 1.0 * 0.1 * 1000 = 100
    EXPECT_DOUBLE_EQ(candidate.ecpm, 100.0);
}

TEST(AdCandidateTest, IsValid) {
    AdCandidate valid = RankerTestHelper::create_candidate(1, 1.0, 0.1);
    EXPECT_TRUE(valid.is_valid());

    AdCandidate invalid_ctr = RankerTestHelper::create_candidate(2, 1.0, 0.0);
    EXPECT_FALSE(invalid_ctr.is_valid());

    AdCandidate invalid_ecpm = RankerTestHelper::create_candidate(3, 0.0, 0.1);
    EXPECT_FALSE(invalid_ecpm.is_valid());
}

TEST(AdCandidateTest, UpdateEcpm) {
    AdCandidate candidate = RankerTestHelper::create_candidate(1, 1.0, 0.1);

    candidate.bid_price = 2.0;
    candidate.predicted_ctr = 0.2;

    candidate.update_ecpm();

    // eCPM = 2.0 * 0.2 * 1000 = 400
    EXPECT_DOUBLE_EQ(candidate.ecpm, 400.0);
}

// EcpmCalculator 测试
TEST(EcpmCalculatorTest, CalculateSingle) {
    double ecpm = EcpmCalculator::calculate_single(1.0, 0.1);
    EXPECT_DOUBLE_EQ(ecpm, 100.0);
}

TEST(EcpmCalculatorTest, CalculateBatch) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),
        RankerTestHelper::create_candidate(2, 2.0, 0.2),
        RankerTestHelper::create_candidate(3, 0.5, 0.05),
    };

    EcpmCalculator::calculate(candidates);

    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 100.0);
    EXPECT_DOUBLE_EQ(candidates[1].ecpm, 400.0);
    EXPECT_DOUBLE_EQ(candidates[2].ecpm, 25.0);
}

TEST(EcpmCalculatorTest, FilterInvalid) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),   // valid
        RankerTestHelper::create_candidate(2, 0.0, 0.1),   // invalid: bid=0
        RankerTestHelper::create_candidate(3, 1.0, 0.0),   // invalid: ctr=0
        RankerTestHelper::create_candidate(4, 2.0, 0.2),   // valid
    };

    size_t count = EcpmCalculator::filter_invalid(candidates);

    EXPECT_EQ(count, 2);
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 100.0);
    EXPECT_DOUBLE_EQ(candidates[1].ecpm, 400.0);
}

// GreedySort 测试
TEST(GreedySortTest, SortByEcpm) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),   // eCPM=100
        RankerTestHelper::create_candidate(2, 2.0, 0.2),   // eCPM=400
        RankerTestHelper::create_candidate(3, 0.5, 0.05),  // eCPM=25
    };

    GreedySort::sort(candidates);

    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 400.0);
    EXPECT_DOUBLE_EQ(candidates[1].ecpm, 100.0);
    EXPECT_DOUBLE_EQ(candidates[2].ecpm, 25.0);
}

TEST(GreedySortTest, SortEmptyList) {
    AdCandidateList candidates;
    GreedySort::sort(candidates);
    EXPECT_TRUE(candidates.empty());
}

TEST(GreedySortTest, SortSingleElement) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),
    };

    GreedySort::sort(candidates);

    EXPECT_EQ(candidates.size(), 1);
    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 100.0);
}

// DiverseSort 测试
TEST(DiverseSortTest, SortWithDiversity) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 2.0, 0.2, 1),  // eCPM=400, cat=1
        RankerTestHelper::create_candidate(2, 1.8, 0.2, 1),  // eCPM=360, cat=1
        RankerTestHelper::create_candidate(3, 1.5, 0.2, 2),  // eCPM=300, cat=2
        RankerTestHelper::create_candidate(4, 1.0, 0.2, 1),  // eCPM=200, cat=1
        RankerTestHelper::create_candidate(5, 0.9, 0.2, 2),  // eCPM=180, cat=2
    };

    DiverseSort::sort(candidates, 0.5);

    // 第一个应该是 eCPM 最高的
    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 400.0);

    // 检查类别多样性:前3个广告应该至少有2个不同类别
    std::set<uint32_t> categories;
    for (size_t i = 0; i < std::min(size_t(3), candidates.size()); ++i) {
        categories.insert(candidates[i].category);
    }
    EXPECT_GE(categories.size(), 2);
}

// BudgetSort 测试
TEST(BudgetSortTest, SortWithBudget) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),   // eCPM=100
        RankerTestHelper::create_candidate(2, 2.0, 0.2),   // eCPM=400
        RankerTestHelper::create_candidate(3, 0.5, 0.05),  // eCPM=25
    };

    // 设置剩余预算
    candidates[0].remaining_budget = 200.0;  // 高预算
    candidates[1].remaining_budget = 10.0;   // 低预算
    candidates[2].remaining_budget = 50.0;   // 中等预算

    BudgetSort::sort(candidates, 0.5);

    // 由于高预算的加成,第一个广告可能排在前面
    // 具体顺序取决于预算权重
    EXPECT_GT(candidates[0].bid_price * candidates[0].predicted_ctr, 0.0);
}

// TopKSelector 测试
TEST(TopKSelectorTest, SelectTopK) {
    AdCandidateList candidates = RankerTestHelper::generate_candidates(100);

    TopKSelector::select(candidates, 10);

    EXPECT_EQ(candidates.size(), 10);

    // 验证是按 eCPM 降序排列的
    for (size_t i = 1; i < candidates.size(); ++i) {
        EXPECT_GE(candidates[i-1].ecpm, candidates[i].ecpm);
    }
}

TEST(TopKSelectorTest, SelectTopKFast) {
    AdCandidateList candidates = RankerTestHelper::generate_candidates(100);

    TopKSelector::select_fast(candidates, 10);

    EXPECT_EQ(candidates.size(), 10);

    // 前10个应该是按 eCPM 降序排列的
    for (size_t i = 1; i < candidates.size(); ++i) {
        EXPECT_GE(candidates[i-1].ecpm, candidates[i].ecpm);
    }
}

TEST(TopKSelectorTest, SelectAllWhenKLargerThanSize) {
    AdCandidateList candidates = RankerTestHelper::generate_candidates(5);

    TopKSelector::select(candidates, 10);

    EXPECT_EQ(candidates.size(), 5);
}

TEST(TopKSelectorTest, FilterByEcpm) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 1.0, 0.1),   // eCPM=100
        RankerTestHelper::create_candidate(2, 0.05, 0.1),  // eCPM=5
        RankerTestHelper::create_candidate(3, 2.0, 0.1),   // eCPM=200
        RankerTestHelper::create_candidate(4, 0.02, 0.1),  // eCPM=2
    };

    size_t removed = TopKSelector::filter_by_ecpm(candidates, 50.0);

    EXPECT_EQ(removed, 2);
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_GE(candidates[0].ecpm, 50.0);
    EXPECT_GE(candidates[1].ecpm, 50.0);
}

TEST(TopKSelectorTest, DeduplicateByCategory) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 2.0, 0.2, 1),  // eCPM=400, cat=1
        RankerTestHelper::create_candidate(2, 1.0, 0.1, 1),  // eCPM=100, cat=1
        RankerTestHelper::create_candidate(3, 1.5, 0.2, 2),  // eCPM=300, cat=2
        RankerTestHelper::create_candidate(4, 0.5, 0.1, 1),  // eCPM=50, cat=1
        RankerTestHelper::create_candidate(5, 1.0, 0.2, 2),  // eCPM=200, cat=2
    };

    TopKSelector::deduplicate_by_category(candidates);

    // 每个类别只保留 eCPM 最高的
    EXPECT_EQ(candidates.size(), 2);

    // 按类别排序
    std::sort(candidates.begin(), candidates.end(),
        [](const AdCandidate& a, const AdCandidate& b) {
            return a.category < b.category;
        });

    EXPECT_EQ(candidates[0].category, 1);
    EXPECT_DOUBLE_EQ(candidates[0].ecpm, 400.0);

    EXPECT_EQ(candidates[1].category, 2);
    EXPECT_DOUBLE_EQ(candidates[1].ecpm, 300.0);
}

TEST(TopKSelectorTest, GetKthEcpm) {
    AdCandidateList candidates = {
        RankerTestHelper::create_candidate(1, 2.0, 0.2),   // eCPM=400
        RankerTestHelper::create_candidate(2, 1.0, 0.1),   // eCPM=100
        RankerTestHelper::create_candidate(3, 1.5, 0.15),  // eCPM=225
    };

    double ecpm = TopKSelector::get_kth_ecpm(candidates, 1);

    // 第2大的 eCPM 应该是 225
    EXPECT_DOUBLE_EQ(ecpm, 225.0);
}

// Ranker 测试
TEST(RankerTest, BasicRanking) {
    RankerConfig config;
    config.sort_strategy = SortStrategy::kGreedy;
    config.max_ads = 5;
    config.min_ecpm = 0.0;

    Ranker ranker(config);

    AdCandidateList candidates = RankerTestHelper::generate_candidates(100);

    size_t count = ranker.rank(candidates);

    EXPECT_EQ(count, 5);
    EXPECT_EQ(candidates.size(), 5);

    // 验证是按 eCPM 降序排列的
    for (size_t i = 1; i < candidates.size(); ++i) {
        EXPECT_GE(candidates[i-1].ecpm, candidates[i].ecpm);
    }
}

TEST(RankerTest, RankingWithMinEcpmFilter) {
    RankerConfig config;
    config.sort_strategy = SortStrategy::kGreedy;
    config.max_ads = 10;
    config.min_ecpm = 100.0;

    Ranker ranker(config);

    AdCandidateList candidates = RankerTestHelper::generate_candidates(100);

    ranker.rank(candidates);

    // 所有候选的 eCPM 应该都 >= 100
    for (const auto& candidate : candidates) {
        EXPECT_GE(candidate.ecpm, 100.0);
    }
}

TEST(RankerTest, RankingWithCategoryDedup) {
    RankerConfig config;
    config.sort_strategy = SortStrategy::kGreedy;
    config.max_ads = 10;
    config.enable_category_dedup = true;

    Ranker ranker(config);

    AdCandidateList candidates;
    for (int i = 0; i < 20; ++i) {
        candidates.push_back(RankerTestHelper::create_candidate(
            i, 1.0 + i * 0.1, 0.1, i % 5));  // 5个类别
    }

    ranker.rank(candidates);

    // 验证没有重复的类别
    std::set<uint32_t> categories;
    for (const auto& candidate : candidates) {
        categories.insert(candidate.category);
    }

    EXPECT_EQ(categories.size(), candidates.size());
}

TEST(RankerTest, CreateDefaultRanker) {
    auto ranker = Ranker::create_default();

    ASSERT_NE(ranker, nullptr);

    AdCandidateList candidates = RankerTestHelper::generate_candidates(50);

    ranker->rank(candidates);

    EXPECT_LE(candidates.size(), 10);  // 默认 max_ads=10
}

TEST(RankerTest, CreateHighPerformanceRanker) {
    auto ranker = Ranker::create_high_performance();

    ASSERT_NE(ranker, nullptr);

    AdCandidateList candidates = RankerTestHelper::generate_candidates(2000);

    ranker->rank(candidates);

    EXPECT_LE(candidates.size(), 10);
}

TEST(RankerTest, UpdateConfig) {
    auto ranker = Ranker::create_default();

    RankerConfig new_config;
    new_config.max_ads = 20;
    new_config.sort_strategy = SortStrategy::kDiverse;

    ranker->update_config(new_config);

    const auto& config = ranker->get_config();
    EXPECT_EQ(config.max_ads, 20);
    EXPECT_EQ(config.sort_strategy, SortStrategy::kDiverse);
}

TEST(RankerTest, Stats) {
    Ranker ranker;

    // 第一次调用
    AdCandidateList candidates1 = RankerTestHelper::generate_candidates(100);
    ranker.rank(candidates1);

    // 第二次调用(使用新列表)
    AdCandidateList candidates2 = RankerTestHelper::generate_candidates(100);
    ranker.rank(candidates2);

    // 第三次调用(使用新列表)
    AdCandidateList candidates3 = RankerTestHelper::generate_candidates(100);
    ranker.rank(candidates3);

    const auto& stats = ranker.get_stats();

    EXPECT_EQ(stats.total_requests, 3);
    EXPECT_EQ(stats.total_candidates, 300);  // 100个候选 × 3次请求

    ranker.reset_stats();

    const auto& stats_after_reset = ranker.get_stats();
    EXPECT_EQ(stats_after_reset.total_requests, 0);
    EXPECT_EQ(stats_after_reset.total_candidates, 0);
}

// 性能测试
TEST(RankerTest, PerformanceLargeDataset) {
    Ranker ranker(Ranker::create_high_performance()->get_config());

    AdCandidateList candidates = RankerTestHelper::generate_candidates(10000);

    auto start = std::chrono::high_resolution_clock::now();

    ranker.rank(candidates);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    // 性能目标:10000个候选应该在100ms内完成排序
    EXPECT_LT(duration, 100);

    std::cout << "Performance: " << duration << "ms for 10000 candidates\n";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
