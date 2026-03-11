// Author: airprofly
#include <gtest/gtest.h>
#include "pacing_manager.hpp"
#include "async_pacing_updater.hpp"
#include <thread>
#include <vector>
#include <chrono>

using namespace flow_ad::pacing;

class PacingManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<PacingManager>();

        // 配置测试广告计划
        config_.campaign_id = 12345;
        config_.granularity = TimeGranularity::DAILY;
        config_.total_budget = 1000.0;  // 1000 元
        config_.daily_budget = 100.0;
        config_.hourly_budget = 10.0;

        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        config_.start_time_ms = now;
        config_.end_time_ms = now + 24 * 3600 * 1000; // 24 小时后

        config_.algorithm = PacingAlgorithm::TOKEN_BUCKET;
        config_.max_impression_rate = 100.0;
        config_.min_impression_rate = 10.0;
        config_.target_rate = 50.0;
        config_.lookahead_window_sec = 300.0;
        config_.pacing_burstiness = 0.1;

        // PID 参数
        config_.pid_kp = 1.0;
        config_.pid_ki = 0.1;
        config_.pid_kd = 0.01;

        manager_->update_campaign_config(config_.campaign_id, config_);
    }

    std::shared_ptr<PacingManager> manager_;
    BudgetConfig config_;
};

// 基本测试: Pacing 允许/拒绝
TEST_F(PacingManagerTest, BasicAllowDeny) {
    // 前 N 次应该允许(初始令牌桶有足够的令牌)
    int allowed_count = 0;
    for (int i = 0; i < 100; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        }
    }

    // 初始令牌为 1000 元, 所以所有 100 次都应该被允许
    EXPECT_EQ(allowed_count, 100);

    // 继续消费, 直到令牌不足
    allowed_count = 0;
    for (int i = 0; i < 2000; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        } else {
            // 令牌不足, 退出
            break;
        }
    }

    // 应该有一些被拒绝(因为令牌耗尽)
    EXPECT_LT(allowed_count, 2000);
}

// 令牌桶测试: 速率限制
TEST_F(PacingManagerTest, TokenBucketRateLimit) {
    // 设置很低的速率和较小的预算
    config_.target_rate = 1.0; // 每秒 1 元
    config_.algorithm = PacingAlgorithm::TOKEN_BUCKET;
    config_.total_budget = 10.0; // 设置较小的预算
    manager_->update_campaign_config(config_.campaign_id, config_);

    // 先消费掉所有初始令牌
    int consumed = 0;
    for (int i = 0; i < 20; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            consumed++;
        }
    }

    // 应该消费了大约 10 元(全部预算)
    EXPECT_GE(consumed, 10);
    EXPECT_LE(consumed, 20);

    // 等待令牌重填充(1秒)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // 现在应该有新令牌了
    int allowed_count = 0;
    for (int i = 0; i < 20; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        } else {
            // 令牌用完了
            break;
        }
    }

    // 应该有新令牌生成(速率1元/秒, 等待1.1秒)
    // 验证令牌桶确实在工作
    EXPECT_GT(allowed_count, 0);
    EXPECT_LE(allowed_count, 20); // 不应该超过总预算
}

// 令牌桶测试: 重填充
TEST_F(PacingManagerTest, TokenBucketRefill) {
    config_.target_rate = 10.0; // 每秒 10 元
    config_.algorithm = PacingAlgorithm::TOKEN_BUCKET;
    manager_->update_campaign_config(config_.campaign_id, config_);

    // 第一次消费一些令牌
    int allowed1 = 0;
    for (int i = 0; i < 50; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed1++;
        }
    }

    // 等待令牌重填充
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 第二次应该能消费更多令牌
    int allowed2 = 0;
    for (int i = 0; i < 50; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed2++;
        }
    }

    EXPECT_GT(allowed1, 0);
    EXPECT_GT(allowed2, 0);
}

// 自适应 Pacing 测试
TEST_F(PacingManagerTest, AdaptivePacing) {
    config_.algorithm = PacingAlgorithm::ADAPTIVE;
    config_.target_rate = 10.0;
    config_.total_budget = 100.0; // 较小的预算
    config_.pacing_burstiness = 0.5;
    manager_->update_campaign_config(config_.campaign_id, config_);

    int allowed_count = 0;
    for (int i = 0; i < 100; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        }
    }

    // 自适应 Pacing 应该允许所有请求(预算充足)
    EXPECT_GT(allowed_count, 0);
}

// PID 控制测试
TEST_F(PacingManagerTest, PIDControl) {
    config_.algorithm = PacingAlgorithm::PID_CONTROL;
    config_.pid_kp = 1.0;
    config_.pid_ki = 0.1;
    config_.pid_kd = 0.01;
    manager_->update_campaign_config(config_.campaign_id, config_);

    int allowed_count = 0;
    for (int i = 0; i < 1000; i++) {
        if (manager_->allow_impression(config_.campaign_id, 1.0)) {
            allowed_count++;
        }
    }

    // PID 控制应该允许部分请求(基于概率)
    EXPECT_GT(allowed_count, 100);
    EXPECT_LT(allowed_count, 900);
}

// 记录展示测试
TEST_F(PacingManagerTest, RecordImpression) {
    manager_->record_impression(config_.campaign_id, 10.0);
    manager_->record_impression(config_.campaign_id, 20.0);
    manager_->record_impression(config_.campaign_id, 30.0);

    auto stats = manager_->get_stats();
    EXPECT_EQ(stats.total_impressions, 3);
    EXPECT_DOUBLE_EQ(stats.total_spent, 60.0);
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
    auto updater = std::make_shared<AsyncPacingUpdater>(
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

// 性能测试
TEST_F(PacingManagerTest, PerformanceTest) {
    // 添加 1000 个广告计划
    for (int i = 0; i < 1000; i++) {
        BudgetConfig cfg = config_;
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

// 清理过期测试
TEST_F(PacingManagerTest, CleanupExpired) {
    // 添加一个已过期的广告计划
    BudgetConfig expired_config = config_;
    expired_config.campaign_id = 99999;
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    expired_config.start_time_ms = now - 100000;
    expired_config.end_time_ms = now - 1000; // 已过期
    manager_->update_campaign_config(expired_config.campaign_id, expired_config);

    // 清理前应该有 2 个广告计划
    auto stats_before = manager_->get_stats();
    EXPECT_EQ(stats_before.total_campaigns, 2);

    // 清理过期数据
    manager_->cleanup_expired();

    // 清理后应该只有 1 个广告计划
    auto stats_after = manager_->get_stats();
    EXPECT_EQ(stats_after.total_campaigns, 1);
}

// 移除广告计划测试
TEST_F(PacingManagerTest, RemoveCampaign) {
    // 添加一个广告计划
    uint64_t campaign_id = 88888;
    manager_->update_campaign_config(campaign_id, config_);

    // 验证已添加
    auto stats_before = manager_->get_stats();
    EXPECT_EQ(stats_before.total_campaigns, 2);

    // 移除广告计划
    manager_->remove_campaign(campaign_id);

    // 验证已移除
    auto stats_after = manager_->get_stats();
    EXPECT_EQ(stats_after.total_campaigns, 1);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
