#include "ranker.hpp"
#include <chrono>
#include <algorithm>
#include <execution>

namespace flow_ad_system::ranker {

Ranker::Ranker(const RankerConfig& config)
    : config_(config), stats_{} {}

size_t Ranker::rank(AdCandidateList& candidates) {
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t input_size = candidates.size();

    // 1. 计算 eCPM
    calculate_ecpm(candidates);

    // 2. 过滤无效候选
    filter_invalid(candidates);

    // 3. 过滤低 eCPM 候选
    filter_by_ecpm(candidates);

    // 4. 排序
    sort_candidates(candidates);

    // 5. Top-K 选择
    select_topk(candidates);

    // 6. 去重
    deduplicate(candidates);

    // 更新统计信息
    update_stats(input_size, candidates.size());

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    // 更新处理时间统计(简单移动平均)
    constexpr double alpha = 0.1;  // 平滑因子
    stats_.avg_processing_time_us =
        stats_.avg_processing_time_us * (1.0 - alpha) + duration * alpha;

    return candidates.size();
}

void Ranker::calculate_ecpm(AdCandidateList& candidates) {
    if (config_.enable_parallel && candidates.size() >= config_.parallel_threshold) {
        EcpmCalculator::calculate_parallel(candidates);
    } else {
        EcpmCalculator::calculate(candidates);
    }
}

void Ranker::filter_invalid(AdCandidateList& candidates) {
    size_t before = candidates.size();
    size_t after = EcpmCalculator::filter_invalid(candidates);
    stats_.filtered_invalid += (before - after);
}

void Ranker::filter_by_ecpm(AdCandidateList& candidates) {
    if (config_.min_ecpm > 0.0) {
        stats_.filtered_by_ecpm += TopKSelector::filter_by_ecpm(
            candidates, config_.min_ecpm);
    }
}

void Ranker::sort_candidates(AdCandidateList& candidates) {
    if (config_.enable_parallel && candidates.size() >= config_.parallel_threshold) {
        // 并行排序
        std::sort(std::execution::par_unseq, candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });
    } else {
        // 使用指定的排序策略
        switch (config_.sort_strategy) {
            case SortStrategy::kGreedy:
                Sorter::sort(candidates, SortStrategy::kGreedy);
                break;
            case SortStrategy::kDiverse:
                Sorter::sort_diverse(candidates, config_.diversity_weight);
                break;
            case SortStrategy::kBudget:
                Sorter::sort_budget(candidates, config_.budget_weight);
                break;
        }
    }
}

void Ranker::select_topk(AdCandidateList& candidates) {
    if (config_.max_ads > 0 && config_.max_ads < candidates.size()) {
        if (config_.enable_parallel && candidates.size() >= config_.parallel_threshold) {
            TopKSelector::select_parallel(candidates, config_.max_ads);
        } else {
            TopKSelector::select(candidates, config_.max_ads);
        }
    }
}

void Ranker::deduplicate(AdCandidateList& candidates) {
    if (config_.enable_category_dedup) {
        TopKSelector::deduplicate_by_category(candidates);
    }
    if (config_.enable_deal_dedup) {
        TopKSelector::deduplicate_by_deal(candidates);
    }
}

void Ranker::update_stats(size_t input_size, size_t output_size) {
    (void)output_size;  // 避免未使用参数警告
    ++stats_.total_requests;
    stats_.total_candidates += input_size;
}

std::unique_ptr<Ranker> Ranker::create_default() {
    RankerConfig config;
    config.sort_strategy = SortStrategy::kGreedy;
    config.max_ads = 10;
    config.min_ecpm = 0.0;
    config.enable_parallel = true;
    config.parallel_threshold = 1000;

    return std::make_unique<Ranker>(config);
}

std::unique_ptr<Ranker> Ranker::create_high_performance() {
    RankerConfig config;
    config.sort_strategy = SortStrategy::kGreedy;
    config.max_ads = 10;
    config.min_ecpm = 0.0;
    config.enable_parallel = true;
    config.parallel_threshold = 500;  // 更低的阈值,更积极地使用并行
    config.enable_category_dedup = true;  // 启用去重减少计算量

    return std::make_unique<Ranker>(config);
}

} // namespace flow_ad_system::ranker
