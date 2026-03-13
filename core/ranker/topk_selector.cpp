#include "topk_selector.hpp"

#if defined(__cpp_lib_parallel_algorithm) && __cpp_lib_parallel_algorithm >= 201603L
#include <execution>
#endif

namespace flow_ad_system::ranker {

void TopKSelector::select_parallel(AdCandidateList& candidates, size_t k) {
#if defined(__cpp_lib_parallel_algorithm) && __cpp_lib_parallel_algorithm >= 201603L
    if (k >= candidates.size()) {
        // 使用并行排序
        std::sort(std::execution::par_unseq, candidates.begin(), candidates.end(),
            [](const AdCandidate& a, const AdCandidate& b) {
                return a.ecpm > b.ecpm;
            });
        return;
    }

    // 使用并行 partial_sort
    std::partial_sort(std::execution::par_unseq,
        candidates.begin(), candidates.begin() + k, candidates.end(),
        [](const AdCandidate& a, const AdCandidate& b) {
            return a.ecpm > b.ecpm;
        });

    candidates.resize(k);
#else
    // 回退到普通版本
    select(candidates, k);
#endif
}

} // namespace flow_ad_system::ranker
