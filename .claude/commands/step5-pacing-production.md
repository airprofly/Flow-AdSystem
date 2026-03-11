# Step 5: 广告计划匀速消费（Pacing模块）- 生产级实现方案

## 🎯 核心设计原则

### 问题本质
**预算平滑消耗问题**
- 广告池: 10万+ 广告计划
- 预算规模: 日预算 100元 ~ 100万元
- 投放周期: 小时级/日级/周级
- 流量波动: 高峰期是低峰期的 3-5 倍
- 延迟要求: p99 < 2ms

### 核心挑战
1. **预算利用率**: 既要花完预算,又不能超支(允许 ±5% 误差)
2. **流量预测**: 需要预测未来流量,避免预算耗尽太早或太晚
3. **实时调整**: 根据实际投放情况动态调整投放速率
4. **多维度控制**: 支持小时/天/周不同时间粒度的预算控制
5. **高并发**: 10K+ QPS 下仍需保持低延迟

---

## 🏗️ 生产级架构设计

### 1. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    Ad Service (广告投放)                      │
│  1. 接收请求 → 2. 索引召回 → 3. Pacing 检查 ⭐ → 4. 竞价    │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                  Pacing Manager (Pacing 管理器)              │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │  Budget Tracker  │  │  Rate Calculator  │                │
│  │  (预算跟踪)       │  │  (速率计算器)     │                │
│  └────────┬─────────┘  └────────┬─────────┘                │
│           │                     │                           │
│  ┌────────▼─────────────────────▼──────────┐               │
│  │         Multi-Algorithm Pacing          │               │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐   │               │
│  │  │Token    │ │Adaptive │ │PID      │   │               │
│  │  │Bucket   │ │Pacing   │ │Control  │   │               │
│  │  └─────────┘ └─────────┘ └─────────┘   │               │
│  └─────────────────────────────────────────┘               │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │  Async Updater   │  │  Metrics Logger  │                │
│  │  (异步更新器)     │  │  (指标记录)       │                │
│  └──────────────────┘  └──────────────────┘                │
└─────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                    Storage Layer (存储层)                    │
│  ┌──────────────────┐  ┌──────────────────┐                │
│  │  L1: Local Cache │  │  L2: Redis       │                │
│  │  (内存哈希表)     │  │  (持久化)         │                │
│  │  ~0.1ms          │  │  ~5ms            │                │
│  └──────────────────┘  └──────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

---

## 💾 核心数据结构设计

### 1. 预算配置 (BudgetConfig)

```cpp
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

    // 是否启用
    bool is_enabled() const {
        return algorithm != PacingAlgorithm::NONE && total_budget > 0;
    }
};

} // namespace pacing
} // namespace flow_ad
```

---

### 2. 预算状态 (BudgetState)

```cpp
#pragma once
#include <atomic>
#include <chrono>

namespace flow_ad {
namespace pacing {

// 预算状态(线程安全)
struct BudgetState {
    // 原子变量:已消费预算
    std::atomic<double> spent_budget{0.0};

    // 原子变量:已展示次数
    std::atomic<uint64_t> impression_count{0};

    // 原子变量:上次更新时间
    std::atomic<uint64_t> last_update_ms{0};

    // 令牌桶状态
    struct TokenBucketState {
        std::atomic<double> tokens{0.0};      // 当前令牌数
        std::atomic<double> rate{0.0};        // 当前速率
        uint64_t last_refill_ms;              // 上次填充时间
    } token_bucket;

    // 自适应 Pacing 状态
    struct AdaptiveState {
        std::atomic<double> current_rate{0.0};     // 当前投放速率
        std::atomic<double> predicted_traffic{0.0}; // 预测流量
        double smoothing_factor;                   // 平滑因子
    } adaptive;

    // PID 控制状态
    struct PIDState {
        double integral_error;      // 积分误差
        double last_error;          // 上次误差
        double kp;                  // 比例系数
        double ki;                  // 积分系数
        double kd;                  // 微分系数
    } pid;
};

} // namespace pacing
} // namespace flow_ad
```

---

### 3. 分片管理器 (ShardedPacingManager)

```cpp
#pragma once
#include "budget_config.h"
#include "budget_state.h"
#include <unordered_map>
#include <shared_mutex>
#include <array>
#include <memory>

namespace flow_ad {
namespace pacing {

// 分片数量(设置为 CPU 核心数的 2-4 倍)
constexpr uint32_t kNumPacingShards = 64;

// 单个分片
struct PacingShard {
    mutable std::shared_mutex mutex;

    // campaign_id -> (config, state)
    std::unordered_map<uint64_t, std::pair<BudgetConfig, BudgetState>> campaigns;

    // 统计信息
    std::atomic<uint64_t> check_count{0};
    std::atomic<uint64_t> allow_count{0};
    std::atomic<uint64_t> deny_count{0};
};

// Pacing 管理器
class PacingManager {
public:
    PacingManager();
    ~PacingManager() = default;

    // 禁止拷贝和移动
    PacingManager(const PacingManager&) = delete;
    PacingManager& operator=(const PacingManager&) = delete;

    /**
     * 检查是否允许投放广告
     * @param campaign_id 广告计划 ID
     * @param bid_price 出价(元)
     * @return true = 允许投放;false = 拒绝投放
     */
    bool allow_impression(uint64_t campaign_id, double bid_price);

    /**
     * 记录一次广告展示(竞价成功后调用)
     * @param campaign_id 广告计划 ID
     * @param win_price 竞价获胜价格(元)
     */
    void record_impression(uint64_t campaign_id, double win_price);

    /**
     * 添加/更新广告计划配置
     */
    void update_campaign_config(uint64_t campaign_id, const BudgetConfig& config);

    /**
     * 移除广告计划
     */
    void remove_campaign(uint64_t campaign_id);

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_campaigns;
        uint64_t total_checks;
        uint64_t total_allows;
        uint64_t total_denies;
        double allow_rate;
        uint64_t total_impressions;
        double total_spent;
    };
    Stats get_stats() const;

    /**
     * 清理过期数据(后台线程调用)
     */
    void cleanup_expired();

private:
    // 分片数组
    std::array<PacingShard, kNumPacingShards> shards_;

    // 当前时间戳(毫秒)
    uint64_t current_time_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // 计算分片索引
    uint32_t get_shard_index(uint64_t campaign_id) const {
        return campaign_id % kNumPacingShards;
    }

    // 获取分片
    PacingShard& get_shard(uint64_t campaign_id) {
        return shards_[get_shard_index(campaign_id)];
    }

    // ========== Pacing 算法实现 ==========

    // 令牌桶算法
    bool token_bucket_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void token_bucket_refill(BudgetConfig& config, BudgetState& state);

    // 自适应 Pacing
    bool adaptive_pacing_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void adaptive_pacing_update(BudgetConfig& config, BudgetState& state);

    // PID 控制
    bool pid_control_check(BudgetConfig& config, BudgetState& state, double bid_price);
    void pid_control_update(BudgetConfig& config, BudgetState& state);
};

} // namespace pacing
} // namespace flow_ad
```

---

## 🔧 核心算法实现

### 1. 令牌桶算法 (Token Bucket)

```cpp
#include "pacing_manager.h"
#include <algorithm>

namespace flow_ad {
namespace pacing {

bool PacingManager::allow_impression(uint64_t campaign_id, double bid_price) {
    auto& shard = get_shard(campaign_id);

    std::shared_lock<std::shared_mutex> lock(shard.mutex);

    auto it = shard.campaigns.find(campaign_id);
    if (it == shard.campaigns.end()) {
        return true; // 没有配置 Pacing,默认允许
    }

    auto& [config, state] = it->second;
    if (!config.is_enabled()) {
        return true; // 未启用 Pacing
    }

    shard.check_count.fetch_add(1, std::memory_order_relaxed);

    // 根据算法类型调用对应的检查逻辑
    bool allowed = false;
    switch (config.algorithm) {
        case PacingAlgorithm::TOKEN_BUCKET:
            allowed = token_bucket_check(config, state, bid_price);
            break;
        case PacingAlgorithm::ADAPTIVE:
            allowed = adaptive_pacing_check(config, state, bid_price);
            break;
        case PacingAlgorithm::PID_CONTROL:
            allowed = pid_control_check(config, state, bid_price);
            break;
        default:
            allowed = true;
            break;
    }

    if (allowed) {
        shard.allow_count.fetch_add(1, std::memory_order_relaxed);
    } else {
        shard.deny_count.fetch_add(1, std::memory_order_relaxed);
    }

    return allowed;
}

// ========== 令牌桶算法实现 ==========

bool PacingManager::token_bucket_check(BudgetConfig& config, BudgetState& state,
                                       double bid_price) {
    uint64_t now = current_time_ms();

    // 1. 先填充令牌
    token_bucket_refill(config, state);

    // 2. 检查令牌是否足够
    double current_tokens = state.token_bucket.tokens.load(std::memory_order_acquire);

    if (current_tokens >= bid_price) {
        // 3. 尝试消费令牌(CAS 操作)
        double expected = current_tokens;
        double desired = current_tokens - bid_price;

        if (state.token_bucket.tokens.compare_exchange_strong(
                expected, desired,
                std::memory_order_release,
                std::memory_order_acquire)) {
            // 消费成功
            state.last_update_ms.store(now, std::memory_order_release);
            return true;
        }
    }

    return false;
}

void PacingManager::token_bucket_refill(BudgetConfig& config, BudgetState& state) {
    uint64_t now = current_time_ms();
    uint64_t last_refill = state.token_bucket.last_refill_ms;

    if (last_refill == 0) {
        // 首次初始化
        state.token_bucket.last_refill_ms = now;
        state.token_bucket.tokens.store(config.total_budget, std::memory_order_release);
        state.token_bucket.rate.store(config.target_rate, std::memory_order_release);
        return;
    }

    // 计算时间差(秒)
    double elapsed_sec = (now - last_refill) / 1000.0;
    if (elapsed_sec < 0.001) {
        return; // 时间间隔太短,不填充
    }

    // 计算应该填充的令牌数
    double rate = state.token_bucket.rate.load(std::memory_order_acquire);
    double refill_tokens = rate * elapsed_sec;

    // 更新令牌数(不超过总预算)
    double current_tokens = state.token_bucket.tokens.load(std::memory_order_acquire);
    double new_tokens = std::min(current_tokens + refill_tokens, config.total_budget);

    state.token_bucket.tokens.store(new_tokens, std::memory_order_release);
    state.token_bucket.last_refill_ms = now;
}

} // namespace pacing
} // namespace flow_ad
```

---

### 2. 自适应 Pacing (Adaptive Pacing)

```cpp
namespace flow_ad {
namespace pacing {

bool PacingManager::adaptive_pacing_check(BudgetConfig& config, BudgetState& state,
                                          double bid_price) {
    uint64_t now = current_time_ms();

    // 1. 获取当前投放速率
    double current_rate = state.adaptive.current_rate.load(std::memory_order_acquire);

    // 2. 获取已消费预算
    double spent = state.spent_budget.load(std::memory_order_acquire);

    // 3. 计算剩余预算
    double remaining_budget = config.total_budget - spent;

    // 4. 计算剩余时间(秒)
    uint64_t elapsed_ms = now - config.start_time_ms;
    uint64_t total_duration_ms = config.end_time_ms - config.start_time_ms;
    uint64_t remaining_ms = total_duration_ms - elapsed_ms;
    double remaining_sec = remaining_ms / 1000.0;

    if (remaining_sec <= 0) {
        return false; // 投放已结束
    }

    // 5. 计算理想速率(理想情况下应该以什么速度投放)
    double ideal_rate = remaining_budget / remaining_sec;

    // 6. 根据预测流量调整速率
    double predicted_traffic = state.adaptive.predicted_traffic.load(std::memory_order_acquire);
    double adjusted_rate = ideal_rate;

    if (predicted_traffic > 0) {
        // 如果预测流量高,可以稍微加速
        // 如果预测流量低,需要减速
        double traffic_ratio = predicted_traffic / current_rate;
        adjusted_rate = ideal_rate * std::clamp(traffic_ratio, 0.5, 2.0);
    }

    // 7. 平滑调整速率
    double smoothing_factor = config.pacing_burstiness; // 0-1,越小越平滑
    double new_rate = current_rate * (1 - smoothing_factor) + adjusted_rate * smoothing_factor;

    // 8. 限制速率范围
    new_rate = std::clamp(new_rate, config.min_impression_rate, config.max_impression_rate);

    // 9. 更新速率
    state.adaptive.current_rate.store(new_rate, std::memory_order_release);

    // 10. 检查是否允许投放
    // 简化逻辑:基于速率判断
    double expected_spent = current_rate * (1.0 / 60.0); // 假设每分钟检查一次

    return (spent + expected_spent + bid_price) <= (config.total_budget * 1.05); // 允许 5% 超支
}

void PacingManager::adaptive_pacing_update(BudgetConfig& config, BudgetState& state) {
    // 这个方法由后台线程定期调用,更新预测流量
    // 这里简化实现,实际应该基于历史流量数据预测

    uint64_t now = current_time_ms();
    uint64_t last_update = state.last_update_ms.load(std::memory_order_acquire);

    if (last_update == 0) {
        state.last_update_ms.store(now, std::memory_order_release);
        return;
    }

    double elapsed_sec = (now - last_update) / 1000.0;
    if (elapsed_sec < 60.0) {
        return; // 每分钟更新一次
    }

    // 计算当前实际投放速率
    uint64_t current_impressions = state.impression_count.load(std::memory_order_acquire);
    // ... (需要记录上次的 impression_count)

    // 更新预测流量(简化:使用当前速率)
    double current_rate = state.adaptive.current_rate.load(std::memory_order_acquire);
    state.adaptive.predicted_traffic.store(current_rate, std::memory_order_release);

    state.last_update_ms.store(now, std::memory_order_release);
}

} // namespace pacing
} // namespace flow_ad
```

---

### 3. PID 控制算法

```cpp
namespace flow_ad {
namespace pacing {

bool PacingManager::pid_control_check(BudgetConfig& config, BudgetState& state,
                                      double bid_price) {
    uint64_t now = current_time_ms();

    // 1. 计算当前进度
    uint64_t elapsed_ms = now - config.start_time_ms;
    uint64_t total_duration_ms = config.end_time_ms - config.start_time_ms;
    double time_progress = static_cast<double>(elapsed_ms) / total_duration_ms;

    // 2. 计算预算进度
    double spent = state.spent_budget.load(std::memory_order_acquire);
    double budget_progress = spent / config.total_budget;

    // 3. 计算误差(预算进度 - 时间进度)
    double error = budget_progress - time_progress;

    // 4. PID 计算
    auto& pid = state.pid;

    // 比例项
    double p_term = pid.kp * error;

    // 积分项
    pid.integral_error += error;
    double i_term = pid.ki * pid.integral_error;

    // 微分项
    double derivative = error - pid.last_error;
    pid.last_error = error;
    double d_term = pid.kd * derivative;

    // PID 输出
    double pid_output = p_term + i_term + d_term;

    // 5. 根据 PID 输出调整投放概率
    // pid_output > 0: 投放过快,降低概率
    // pid_output < 0: 投放过慢,提高概率
    double base_probability = 0.5;
    double adjusted_probability = base_probability - pid_output;
    adjusted_probability = std::clamp(adjusted_probability, 0.0, 1.0);

    // 6. 概率性投放
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    return dis(gen) < adjusted_probability;
}

void PacingManager::pid_control_update(BudgetConfig& config, BudgetState& state) {
    // PID 参数初始化(如果需要)
    auto& pid = state.pid;

    if (pid.kp == 0.0 && pid.ki == 0.0 && pid.kd == 0.0) {
        // 初始化 PID 参数
        pid.kp = 1.0;  // 比例系数
        pid.ki = 0.1;  // 积分系数
        pid.kd = 0.01; // 微分系数
        pid.integral_error = 0.0;
        pid.last_error = 0.0;
    }
}

} // namespace pacing
} // namespace flow_ad
```

---

## 🔄 异步更新机制

### 问题: 展示记录的"异步性"

Pacing 检查必须在竞价**之前**,但实际消费金额只有在竞价**成功**后才确定。

### 解决方案: 异步消息队列

```cpp
#pragma once
#include "pacing_manager.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace flow_ad {
namespace pacing {

// 展示事件
struct ImpressionEvent {
    uint64_t campaign_id;
    double win_price;
    uint64_t timestamp_ms;
};

// 异步更新器
class AsyncPacingUpdater {
public:
    AsyncPacingUpdater(std::shared_ptr<PacingManager> manager,
                       size_t queue_size = 100000,
                       uint32_t batch_size = 1000,
                       uint32_t flush_interval_ms = 100)
        : manager_(manager),
          queue_size_(queue_size),
          batch_size_(batch_size),
          flush_interval_ms_(flush_interval_ms),
          running_(false) {
        start();
    }

    ~AsyncPacingUpdater() {
        stop();
    }

    // 禁止拷贝和移动
    AsyncPacingUpdater(const AsyncPacingUpdater&) = delete;
    AsyncPacingUpdater& operator=(const AsyncPacingUpdater&) = delete;

    /**
     * 异步记录一次展示
     * @note 非阻塞,如果队列满则丢弃
     */
    bool record_impression(uint64_t campaign_id, double win_price) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (queue_.size() >= queue_size_) {
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        queue_.push({campaign_id, win_price, current_time_ms()});
        cv_.notify_one();
        return true;
    }

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t processed_count;
        uint64_t dropped_count;
        size_t current_queue_size;
    };
    Stats get_stats() const {
        Stats stats{};
        stats.processed_count = processed_count_.load(std::memory_order_relaxed);
        stats.dropped_count = dropped_count_.load(std::memory_order_relaxed);

        std::unique_lock<std::mutex> lock(mutex_);
        stats.current_queue_size = queue_.size();

        return stats;
    }

private:
    void start() {
        running_ = true;
        worker_thread_ = std::thread([this]() {
            while (running_) {
                process_batch();
            }
            process_remaining();
        });
    }

    void stop() {
        running_ = false;
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    void process_batch() {
        std::vector<ImpressionEvent> batch;
        batch.reserve(batch_size_);

        {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_.wait_for(lock, std::chrono::milliseconds(flush_interval_ms_),
                       [this] { return !queue_.empty() || !running_; });

            while (!queue_.empty() && batch.size() < batch_size_) {
                batch.push_back(queue_.front());
                queue_.pop();
            }
        }

        if (!batch.empty()) {
            for (const auto& event : batch) {
                manager_->record_impression(event.campaign_id, event.win_price);
                processed_count_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void process_remaining() {
        std::vector<ImpressionEvent> remaining;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            remaining.reserve(queue_.size());
            while (!queue_.empty()) {
                remaining.push_back(queue_.front());
                queue_.pop();
            }
        }

        for (const auto& event : remaining) {
            manager_->record_impression(event.campaign_id, event.win_price);
        }
    }

    static uint64_t current_time_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    std::shared_ptr<PacingManager> manager_;
    size_t queue_size_;
    uint32_t batch_size_;
    uint32_t flush_interval_ms_;

    std::queue<ImpressionEvent> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> running_;

    std::atomic<uint64_t> processed_count_{0};
    std::atomic<uint64_t> dropped_count_{0};
};

} // namespace pacing
} // namespace flow_ad
```

---

## 🧪 单元测试

```cpp
#include <gtest/gtest.h>
#include "pacing_manager.h"
#include "async_pacing_updater.h"
#include <thread>
#include <vector>

class PacingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<pacing::PacingManager>();

        // 配置测试广告计划
        config_.campaign_id = 12345;
        config_.granularity = pacing::TimeGranularity::DAILY;
        config_.total_budget = 1000.0;  // 1000 元
        config_.daily_budget = 100.0;
        config_.hourly_budget = 10.0;

        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        config_.start_time_ms = now;
        config_.end_time_ms = now + 24 * 3600 * 1000; // 24 小时后

        config_.algorithm = pacing::PacingAlgorithm::TOKEN_BUCKET;
        config_.max_impression_rate = 100.0;
        config_.min_impression_rate = 10.0;
        config_.target_rate = 50.0;

        manager_->update_campaign_config(config_.campaign_id, config_);
    }

    std::unique_ptr<pacing::PacingManager> manager_;
    pacing::BudgetConfig config_;
};

// 基本测试: Pacing 允许/拒绝
TEST_F(PacingManagerTest, BasicAllowDeny) {
    // 前 N 次应该允许
    int allowed_count = 0;
    for (int i = 0; i < 100; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        }
    }

    // 应该有部分被允许,部分被拒绝(取决于令牌桶速率)
    EXPECT_GT(allowed_count, 0);
    EXPECT_LT(allowed_count, 100);
}

// 令牌桶测试: 速率限制
TEST_F(PacingManagerTest, TokenBucketRateLimit) {
    // 设置很低的速率
    config_.target_rate = 1.0; // 每秒 1 元
    manager_->update_campaign_config(config_.campaign_id, config_);

    int allowed_count = 0;
    for (int i = 0; i < 100; i++) {
        if (manager_->allow_impression(config_.campaign_id, 0.5)) {
            allowed_count++;
        }
    }

    // 由于速率限制,应该只有少量被允许
    EXPECT_LT(allowed_count, 10);
}

// 并发测试: 多线程竞态
TEST_F(PacingManagerTest, ConcurrencyTest) {
    constexpr int num_threads = 10;
    constexpr int requests_per_thread = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> allowed_count{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &allowed_count]() {
            for (int i = 0; i < requests_per_thread; i++) {
                if (manager_->allow_impression(config_.campaign_id, 1.0)) {
                    allowed_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 总允许次数应该在合理范围内
    EXPECT_GT(allowed_count.load(), 0);
    EXPECT_LT(allowed_count.load(), num_threads * requests_per_thread);
}

// 异步更新测试
TEST_F(PacingManagerTest, AsyncUpdaterTest) {
    auto updater = std::make_unique<pacing::AsyncPacingUpdater>(
        manager_, 10000, 100, 100);

    // 记录多次展示
    for (int i = 0; i < 1000; i++) {
        updater->record_impression(config_.campaign_id, 1.0);
    }

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto stats = updater->get_stats();
    EXPECT_GT(stats.processed_count, 0);
    EXPECT_EQ(stats.dropped_count, 0);
}

// 性能测试
TEST_F(PacingManagerTest, PerformanceTest) {
    // 添加 1000 个广告计划
    for (int i = 0; i < 1000; i++) {
        pacing::BudgetConfig cfg = config_;
        cfg.campaign_id = 20000 + i;
        manager_->update_campaign_config(cfg.campaign_id, cfg);
    }

    constexpr int num_iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        uint64_t campaign_id = 20000 + (i % 1000);
        manager_->allow_impression(campaign_id, 1.0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double qps = num_iterations * 1000000.0 / duration.count();
    std::cout << "Performance: " << qps << " QPS" << std::endl;

    // 应该 > 500K QPS
    EXPECT_GT(qps, 500000);
}

// 统计测试
TEST_F(PacingManagerTest, StatsTest) {
    manager_->allow_impression(config_.campaign_id, 1.0);
    manager_->allow_impression(config_.campaign_id, 1.0);
    manager_->allow_impression(config_.campaign_id, 1.0);

    auto stats = manager_->get_stats();

    EXPECT_EQ(stats.total_campaigns, 1);
    EXPECT_EQ(stats.total_checks, 3);
    EXPECT_GE(stats.total_allows, 0);
    EXPECT_LE(stats.total_allows, 3);
}
```

---

## 📊 性能优化建议

### 1. 使用更快的哈希表

```cpp
// 替换 std::unordered_map 为 absl::flat_hash_map
#include "absl/container/flat_hash_map.h"

struct PacingShard {
    mutable std::shared_mutex mutex;
    absl::flat_hash_map<uint64_t, std::pair<BudgetConfig, BudgetState>> campaigns;
    // ...
};
```

### 2. 无锁优化

```cpp
// 对于高频读取的配置,可以使用原子指针
class PacingManager {
    struct AtomicConfigPtr {
        std::shared_ptr<BudgetConfig> config;
        std::atomic<bool> updating{false};

        std::shared_ptr<BudgetConfig> get() const {
            while (updating.load(std::memory_order_acquire)) {
                // 等待更新完成
                std::this_thread::yield();
            }
            return config;
        }
    };

    std::unordered_map<uint64_t, AtomicConfigPtr> config_cache_;
};
```

### 3. 批量更新优化

```cpp
// 后台线程批量更新预算状态
class BatchPacingUpdater {
    void update_batch() {
        std::vector<uint64_t> campaign_ids;

        // 收集需要更新的广告计划
        {
            std::shared_lock lock(mutex_);
            for (const auto& [id, _] : campaigns_) {
                campaign_ids.push_back(id);
            }
        }

        // 批量更新(释放锁)
        for (uint64_t id : campaign_ids) {
            update_single_campaign(id);
        }
    }
};
```

---

## 🎯 与广告投放系统集成

```cpp
#include "pacing_manager.h"
#include "async_pacing_updater.h"

namespace flow_ad {
namespace service {

class AdService {
public:
    AdService() {
        pacing_manager_ = std::make_shared<pacing::PacingManager>();
        async_pacing_updater_ = std::make_unique<pacing::AsyncPacingUpdater>(
            pacing_manager_,
            100000,  // queue_size
            1000,    // batch_size
            100      // flush_interval_ms
        );

        // 启动后台更新线程
        start_background_tasks();
    }

    /**
     * 广告投放主流程
     */
    std::vector<uint64_t> serve_ads(const BidRequest& request) {
        std::vector<uint64_t> candidate_ads;

        // 1. 索引召回
        candidate_ads = index_service_->search(request);

        // 2. Pacing 过滤
        std::vector<uint64_t> paced_ads;
        for (uint64_t ad_id : candidate_ads) {
            uint64_t campaign_id = get_campaign_id(ad_id);
            double bid_price = get_bid_price(ad_id);

            if (pacing_manager_->allow_impression(campaign_id, bid_price)) {
                paced_ads.push_back(ad_id);
            }
        }

        // 3. 频控过滤
        std::vector<uint64_t> filtered_ads;
        for (uint64_t ad_id : paced_ads) {
            if (!frequency_manager_->is_capped(request.user_id, ad_id, ...)) {
                filtered_ads.push_back(ad_id);
            }
        }

        // 4. ECPM 排序
        auto ranked_ads = ranker_->rank(filtered_ads, request);

        // 5. 返回 Top-K
        return top_k(ranked_ads, 5);
    }

    /**
     * 竞价成功回调
     */
    void on_auction_success(uint64_t ad_id, double win_price) {
        uint64_t campaign_id = get_campaign_id(ad_id);

        // 异步记录展示
        async_pacing_updater_->record_impression(campaign_id, win_price);
    }

private:
    std::shared_ptr<pacing::PacingManager> pacing_manager_;
    std::unique_ptr<pacing::AsyncPacingUpdater> async_pacing_updater_;

    void start_background_tasks() {
        // 启动自适应 Pacing 更新线程
        background_thread_ = std::thread([this]() {
            while (running_) {
                // 更新所有广告计划的 Pacing 状态
                pacing_manager_->cleanup_expired();

                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
    }

    std::thread background_thread_;
    std::atomic<bool> running_{true};
};

} // namespace service
} // namespace flow_ad
```

---

## ✅ 验收标准

### 功能验收
- [ ] 支持令牌桶、自适应 Pacing、PID 控制三种算法
- [ ] 支持小时/天/周不同时间粒度的预算控制
- [ ] 预算利用率 > 95%(不浪费预算)
- [ ] 超支率 < 5%(不严重超支)
- [ ] 异步记录展示
- [ ] 自动清理过期数据

### 性能验收
- [ ] 检查延迟 p99 < 2ms
- [ ] 吞吐量 > 500K QPS
- [ ] 内存占用合理 (< 500MB,10万广告计划)
- [ ] 并发安全(多线程竞态测试通过)

### 质量验收
- [ ] 单元测试覆盖率 > 90%
- [ ] 无内存泄漏
- [ ] 无线程安全问题

---

## 📚 相关文档

- [项目主 README](../../README.md)
- [架构设计规范](schema.md)
- [Step 3: 广告索引模块](step3-adIndexing.md)
- [Step 4: 频次控制模块](step4-frequency-production.md)

---

**作者**: airprofly
**创建日期**: 2026-03-11
**状态**: ✅ 生产级实现方案
