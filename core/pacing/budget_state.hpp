// Author: airprofly
#pragma once
#include <atomic>
#include <chrono>

namespace flow_ad {
namespace pacing {

// 预算状态(线程安全)
struct BudgetState {
    // 原子变量: 已消费预算
    std::atomic<double> spent_budget{0.0};

    // 原子变量: 已展示次数
    std::atomic<uint64_t> impression_count{0};

    // 原子变量: 上次更新时间
    std::atomic<uint64_t> last_update_ms{0};

    // 令牌桶状态
    struct TokenBucketState {
        std::atomic<double> tokens{0.0};      // 当前令牌数
        std::atomic<double> rate{0.0};        // 当前速率
        uint64_t last_refill_ms{0};           // 上次填充时间(非原子,仅在有锁保护时访问)
    } token_bucket;

    // 自适应 Pacing 状态
    struct AdaptiveState {
        std::atomic<double> current_rate{0.0};     // 当前投放速率
        std::atomic<double> predicted_traffic{0.0}; // 预测流量
        std::atomic<double> last_impression_count{0.0}; // 上次展示次数(用于计算速率)
    } adaptive;

    // PID 控制状态
    struct PIDState {
        double integral_error{0.0};      // 积分误差
        double last_error{0.0};         // 上次误差
        double kp{0.0};                 // 比例系数
        double ki{0.0};                 // 积分系数
        double kd{0.0};                 // 微分系数
    } pid;
};

} // namespace pacing
} // namespace flow_ad
