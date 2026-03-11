// Author: airprofly
#pragma once
#include <cstdint>
#include <atomic>
#include <chrono>

namespace flow_ad {
namespace frequency {

// 频控条目 (紧凑存储,仅 12 字节)
struct FreqRecord {
    uint32_t count;              // 已展示次数 (非原子,由外部锁保护)
    uint64_t expire_at_ms;       // 过期时间戳 (毫秒)

    FreqRecord() : count(0), expire_at_ms(0) {}

    FreqRecord(uint32_t c, uint64_t expire)
        : count(c), expire_at_ms(expire) {}
};

// 频控规则 (从广告配置加载)
struct FrequencyCap {
    uint32_t hourly_limit{0};   // 小时级别限制 (0 = 不限制)
    uint32_t daily_limit{0};    // 天级别限制
    uint32_t weekly_limit{0};   // 周级别限制

    // 是否启用
    bool is_enabled() const {
        return hourly_limit > 0 || daily_limit > 0 || weekly_limit > 0;
    }
};

// 时间窗口枚举
enum class TimeWindow : uint64_t {
    HOUR = 3600000,        // 1 小时
    DAY = 86400000,        // 24 小时
    WEEK = 604800000       // 7 天
};

} // namespace frequency
} // namespace flow_ad
