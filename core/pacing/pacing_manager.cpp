// Author: airprofly
#include "pacing_manager.hpp"
#include <algorithm>
#include <random>
#include <cmath>
#include <mutex>

namespace flow_ad {
namespace pacing {

PacingManager::PacingManager() {
    // 初始化所有分片
}

bool PacingManager::allow_impression(uint64_t campaign_id, double bid_price) {
    auto& shard = get_shard(campaign_id);

    std::shared_lock<std::shared_mutex> lock(shard.mutex);

    auto it = shard.campaigns.find(campaign_id);
    if (it == shard.campaigns.end()) {
        return true; // 没有配置 Pacing, 默认允许
    }

    auto& [config, state] = it->second;
    if (!config.is_enabled()) {
        return true; // 未启用 Pacing
    }

    shard.check_count.fetch_add(1, std::memory_order_relaxed);

    // 检查是否在投放时间内
    uint64_t now = current_time_ms();
    if (now < config.start_time_ms || now > config.end_time_ms) {
        return false; // 不在投放时间内
    }

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

void PacingManager::record_impression(uint64_t campaign_id, double win_price) {
    auto& shard = get_shard(campaign_id);

    std::unique_lock<std::shared_mutex> lock(shard.mutex);

    auto it = shard.campaigns.find(campaign_id);
    if (it == shard.campaigns.end()) {
        return;
    }

    auto& [config, state] = it->second;

    // 更新已消费预算
    state.spent_budget.fetch_add(win_price, std::memory_order_relaxed);

    // 更新展示次数
    state.impression_count.fetch_add(1, std::memory_order_relaxed);

    // 更新时间戳
    state.last_update_ms.store(current_time_ms(), std::memory_order_relaxed);
}

void PacingManager::update_campaign_config(uint64_t campaign_id, const BudgetConfig& config) {
    auto& shard = get_shard(campaign_id);

    std::unique_lock<std::shared_mutex> lock(shard.mutex);

    auto& state = shard.campaigns[campaign_id].second;

    // 更新配置
    shard.campaigns[campaign_id].first = config;

    // 初始化状态(如果是首次添加)
    if (state.token_bucket.last_refill_ms == 0) {
        uint64_t now = current_time_ms();
        state.token_bucket.last_refill_ms = now;
        state.token_bucket.tokens.store(config.total_budget, std::memory_order_release);
        state.token_bucket.rate.store(config.target_rate, std::memory_order_release);
        state.adaptive.current_rate.store(config.target_rate, std::memory_order_release);
        state.adaptive.predicted_traffic.store(config.target_rate, std::memory_order_release);
        state.last_update_ms.store(now, std::memory_order_release);

        // 初始化 PID 参数
        if (config.algorithm == PacingAlgorithm::PID_CONTROL) {
            state.pid.kp = config.pid_kp > 0 ? config.pid_kp : 1.0;
            state.pid.ki = config.pid_ki > 0 ? config.pid_ki : 0.1;
            state.pid.kd = config.pid_kd > 0 ? config.pid_kd : 0.01;
        }
    }
}

void PacingManager::remove_campaign(uint64_t campaign_id) {
    auto& shard = get_shard(campaign_id);

    std::unique_lock<std::shared_mutex> lock(shard.mutex);

    shard.campaigns.erase(campaign_id);
}

auto PacingManager::get_stats() const -> Stats {
    Stats stats{};
    stats.total_campaigns = 0;
    stats.total_checks = 0;
    stats.total_allows = 0;
    stats.total_denies = 0;
    stats.total_impressions = 0;
    stats.total_spent = 0.0;

    for (const auto& shard : shards_) {
        std::shared_lock<std::shared_mutex> lock(shard.mutex);

        stats.total_campaigns += shard.campaigns.size();
        stats.total_checks += shard.check_count.load(std::memory_order_relaxed);
        stats.total_allows += shard.allow_count.load(std::memory_order_relaxed);
        stats.total_denies += shard.deny_count.load(std::memory_order_relaxed);

        for (const auto& [_, config_state] : shard.campaigns) {
            const auto& state = config_state.second;
            stats.total_impressions += state.impression_count.load(std::memory_order_relaxed);
            stats.total_spent += state.spent_budget.load(std::memory_order_relaxed);
        }
    }

    if (stats.total_checks > 0) {
        stats.allow_rate = static_cast<double>(stats.total_allows) / stats.total_checks;
    } else {
        stats.allow_rate = 0.0;
    }

    return stats;
}

void PacingManager::cleanup_expired() {
    uint64_t now = current_time_ms();

    for (auto& shard : shards_) {
        std::unique_lock<std::shared_mutex> lock(shard.mutex);

        auto it = shard.campaigns.begin();
        while (it != shard.campaigns.end()) {
            const auto& config = it->second.first;
            // 移除已过期的广告计划
            if (now > config.end_time_ms) {
                it = shard.campaigns.erase(it);
            } else {
                ++it;
            }
        }
    }
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
        return; // 时间间隔太短, 不填充
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

// ========== 自适应 Pacing 算法实现 ==========

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
        // 如果预测流量高, 可以稍微加速
        // 如果预测流量低, 需要减速
        double traffic_ratio = predicted_traffic / std::max(current_rate, 0.001);
        adjusted_rate = ideal_rate * std::clamp(traffic_ratio, 0.5, 2.0);
    }

    // 7. 平滑调整速率
    double smoothing_factor = config.pacing_burstiness; // 0-1, 越小越平滑
    double new_rate = current_rate * (1.0 - smoothing_factor) + adjusted_rate * smoothing_factor;

    // 8. 限制速率范围
    new_rate = std::clamp(new_rate, config.min_impression_rate, config.max_impression_rate);

    // 9. 更新速率
    state.adaptive.current_rate.store(new_rate, std::memory_order_release);

    // 10. 检查是否允许投放
    // 基于速率判断: 检查当前消费是否超过预期
    double expected_spent = current_rate * (1.0 / 60.0); // 假设每分钟检查一次

    return (spent + expected_spent + bid_price) <= (config.total_budget * 1.05); // 允许 5% 超支
}

void PacingManager::adaptive_pacing_update(BudgetConfig& config, BudgetState& state) {
    // 这个方法由后台线程定期调用, 更新预测流量
    // 这里简化实现, 实际应该基于历史流量数据预测

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
    double last_impressions = state.adaptive.last_impression_count.load(std::memory_order_acquire);
    double impression_delta = current_impressions - last_impressions;
    double actual_rate = impression_delta / elapsed_sec;

    // 更新预测流量(简化: 使用当前速率)
    state.adaptive.predicted_traffic.store(actual_rate, std::memory_order_release);
    state.adaptive.last_impression_count.store(current_impressions, std::memory_order_release);

    state.last_update_ms.store(now, std::memory_order_release);
}

// ========== PID 控制算法实现 ==========

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
    // 限制积分项, 防止积分饱和
    pid.integral_error = std::clamp(pid.integral_error, -10.0, 10.0);
    double i_term = pid.ki * pid.integral_error;

    // 微分项
    double derivative = error - pid.last_error;
    pid.last_error = error;
    double d_term = pid.kd * derivative;

    // PID 输出
    double pid_output = p_term + i_term + d_term;

    // 5. 根据 PID 输出调整投放概率
    // pid_output > 0: 投放过快, 降低概率
    // pid_output < 0: 投放过慢, 提高概率
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
    // PID 参数已在 update_campaign_config 中初始化
    // 这个方法保留用于未来的 PID 参数自适应调整
}

} // namespace pacing
} // namespace flow_ad
