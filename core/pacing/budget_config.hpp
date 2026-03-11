// Author: airprofly
#pragma once
#include <cstdint>
#include <chrono>

namespace flow_ad {
namespace pacing {

// 时间粒度
enum class TimeGranularity : uint32_t {
    HOURLY = 1,     // 小时级别
    DAILY = 2,      // 天级别
    WEEKLY = 3,     // 周级别
    CAMPAIGN = 4    // 整个投放周期
};

// Pacing 算法类型
enum class PacingAlgorithm : uint32_t {
    NONE = 0,           // 不启用 Pacing
    TOKEN_BUCKET = 1,   // 令牌桶算法
    ADAPTIVE = 2,       // 自适应 Pacing
    PID_CONTROL = 3     // PID 控制
};

// 预算配置
struct BudgetConfig {
    uint64_t campaign_id;        // 广告计划 ID
    TimeGranularity granularity; // 时间粒度

    double total_budget;         // 总预算(元)
    double daily_budget;         // 日预算(元)
    double hourly_budget;        // 小时预算(元)

    uint64_t start_time_ms;      // 投放开始时间(毫秒时间戳)
    uint64_t end_time_ms;        // 投放结束时间(毫秒时间戳)

    PacingAlgorithm algorithm;   // Pacing 算法

    // 速率配置
    double max_impression_rate;  // 最大展示速率(次/秒)
    double min_impression_rate;  // 最小展示速率(次/秒)
    double target_rate;          // 目标速率(次/秒)

    // 自适应 Pacing 参数
    double lookahead_window_sec; // 预测窗口(秒)
    double pacing_burstiness;    // 突发系数(0-1)

    // PID 控制参数
    double pid_kp;               // 比例系数
    double pid_ki;               // 积分系数
    double pid_kd;               // 微分系数

    // 是否启用
    bool is_enabled() const {
        return algorithm != PacingAlgorithm::NONE && total_budget > 0;
    }
};

} // namespace pacing
} // namespace flow_ad
