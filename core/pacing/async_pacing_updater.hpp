// Author: airprofly
#pragma once
#include "pacing_manager.hpp"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

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
     * @note 非阻塞, 如果队列满则丢弃
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
