#include "ecpm_calculator.hpp"
#include <execution>

namespace flow_ad_system::ranker {

void EcpmCalculator::calculate_parallel(AdCandidateList& candidates) noexcept {
#if defined(__cpp_lib_parallel_algorithm) && __cpp_lib_parallel_algorithm >= 201603L
    // 使用C++17并行算法优化大量计算
    std::for_each(std::execution::par_unseq,
        candidates.begin(), candidates.end(),
        [](AdCandidate& c) { c.update_ecpm(); });
#else
    // 回退到普通计算
    calculate(candidates);
#endif
}

} // namespace flow_ad_system::ranker
