// Author: airprofly
// 集成测试: 索引 + 频控 + Pacing 模块协同工作
// 测试广告索引、频次控制和预算Pacing三个核心模块的集成

#include <gtest/gtest.h>
#include "openrtb_parser.h"
#include "ad_index.hpp"
#include "frequency_manager.hpp"
#include "async_impression_logger.hpp"
#include "pacing_manager.hpp"
#include "async_pacing_updater.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>

using namespace flow_ad;
using namespace flow_ad::openrtb;
using namespace flow_ad::indexing;
using namespace flow_ad::frequency;
using namespace flow_ad::pacing;

/**
 * 核心模块集成测试
 *
 * 测试索引、频控、Pacing三个模块的协同工作:
 * 1. 广告索引管理
 * 2. 频次控制
 * 3. 预算Pacing
 * 4. 异步日志记录
 */
class CoreModulesIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 1. 创建广告索引
        index_ = std::make_unique<AdIndex>();

        // 2. 创建频控管理器
        frequency_manager_ = std::make_shared<FrequencyManager>();
        frequency_logger_ = std::make_unique<AsyncImpressionLogger>(
            frequency_manager_, 10000, 100, 100);

        // 3. 创建 Pacing 管理器
        pacing_manager_ = std::make_shared<PacingManager>();
        pacing_updater_ = std::make_unique<AsyncPacingUpdater>(
            pacing_manager_, 10000, 100, 100);

        // 4. 添加测试数据和配置
        add_test_ads();
        add_pacing_configs();
    }

    void TearDown() override {
        // 等待异步操作完成
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void add_test_ads() {
        // 添加 10 个测试广告
        for (int i = 1; i <= 10; i++) {
            Ad ad;
            ad.id = 1000 + i;
            ad.campaign_id = 200 + i;
            ad.creative_id = 300 + i;
            ad.bid_price = 0.5 + (i % 10) * 0.5;

            // 添加类别
            ad.categories.push_back(static_cast<AdCategory>(i % 10));

            // 添加地域定向
            ad.targeting_geo.push_back("CN");
            ad.targeting_geo.push_back("US");

            // 添加设备定向
            ad.targeting_devices.push_back(flow_ad::DeviceType::MOBILE);
            ad.targeting_devices.push_back(flow_ad::DeviceType::DESKTOP);

            // 添加兴趣定向
            ad.targeting_interests.push_back("sports");
            ad.targeting_interests.push_back("tech");

            // 添加到索引
            index_->add_ad(ad.id, ad);
        }

        std::cout << "添加了 10 个测试广告到索引" << std::endl;
    }

    void add_pacing_configs() {
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        // 为每个广告计划配置预算
        for (int i = 1; i <= 10; i++) {
            BudgetConfig config;
            config.campaign_id = 200 + i;
            config.granularity = TimeGranularity::DAILY;
            config.total_budget = 100.0;
            config.daily_budget = 10.0;
            config.hourly_budget = 1.0;

            config.start_time_ms = now;
            config.end_time_ms = now + 24 * 3600 * 1000;

            // 使用不同的 Pacing 算法
            if (i % 3 == 0) {
                config.algorithm = PacingAlgorithm::TOKEN_BUCKET;
                config.target_rate = 0.1;
            } else if (i % 3 == 1) {
                config.algorithm = PacingAlgorithm::ADAPTIVE;
                config.target_rate = 0.1;
                config.pacing_burstiness = 0.5;
            } else {
                config.algorithm = PacingAlgorithm::PID_CONTROL;
                config.pid_kp = 1.0;
                config.pid_ki = 0.1;
                config.pid_kd = 0.01;
            }

            config.max_impression_rate = 1.0;
            config.min_impression_rate = 0.01;

            pacing_manager_->update_campaign_config(config.campaign_id, config);
        }

        std::cout << "配置了 10 个广告计划的预算" << std::endl;
    }

    std::unique_ptr<AdIndex> index_;
    std::shared_ptr<FrequencyManager> frequency_manager_;
    std::unique_ptr<AsyncImpressionLogger> frequency_logger_;
    std::shared_ptr<PacingManager> pacing_manager_;
    std::unique_ptr<AsyncPacingUpdater> pacing_updater_;
};

// 测试模块初始化
TEST_F(CoreModulesIntegrationTest, ModuleInitialization) {
    // 验证索引
    EXPECT_EQ(index_->size(), 10);
    EXPECT_FALSE(index_->empty());

    // 验证 Pacing 配置
    auto pacing_stats = pacing_manager_->get_stats();
    EXPECT_EQ(pacing_stats.total_campaigns, 10);

    std::cout << "所有模块初始化成功" << std::endl;
}

// 测试广告检索
TEST_F(CoreModulesIntegrationTest, AdRetrieval) {
    // 获取所有广告
    auto all_ad_ids = index_->get_all_ad_ids();
    EXPECT_EQ(all_ad_ids.size(), 10);

    // 获取单个广告
    const Ad* ad = index_->get_ad(1001);
    ASSERT_NE(ad, nullptr);
    EXPECT_EQ(ad->id, 1001);
    EXPECT_EQ(ad->campaign_id, 201);
    EXPECT_GT(ad->bid_price, 0.0);

    std::cout << "广告检索功能正常" << std::endl;
}

// 测试频控和 Pacing 的协同工作
TEST_F(CoreModulesIntegrationTest, FrequencyAndPacingCoordination) {
    uint64_t user_id = 12345;
    uint64_t ad_id = 1001;

    // 模拟多次展示
    for (int i = 0; i < 25; i++) {
        // 1. 频控检查
        FrequencyCap cap;
        cap.hourly_limit = 5;
        cap.daily_limit = 20;

        if (!frequency_manager_->is_capped(user_id, ad_id, cap)) {
            // 2. Pacing 检查
            const Ad* ad = index_->get_ad(ad_id);
            ASSERT_NE(ad, nullptr);

            uint64_t campaign_id = ad->campaign_id;
            if (pacing_manager_->allow_impression(campaign_id, ad->bid_price)) {
                // 允许投放
                frequency_logger_->record_impression(user_id, ad_id, TimeWindow::HOUR);
                frequency_logger_->record_impression(user_id, ad_id, TimeWindow::DAY);
                pacing_updater_->record_impression(campaign_id, ad->bid_price);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // 等待异步处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // 验证频控统计
    auto freq_stats = frequency_manager_->get_stats();
    EXPECT_GT(freq_stats.total_records, 0);

    // 验证 Pacing 统计
    auto pacing_stats = pacing_manager_->get_stats();
    EXPECT_GT(pacing_stats.total_checks, 0);

    std::cout << "频控记录数: " << freq_stats.total_records << std::endl;
    std::cout << "Pacing 检查次数: " << pacing_stats.total_checks << std::endl;
}

// 测试并发场景
TEST_F(CoreModulesIntegrationTest, ConcurrencyStressTest) {
    constexpr int num_threads = 10;
    constexpr int requests_per_thread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> blocked_by_frequency{0};
    std::atomic<int> blocked_by_pacing{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, &blocked_by_frequency, &blocked_by_pacing, t]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> user_dist(1, 10000);
            std::uniform_int_distribution<> ad_dist(1000, 1010);

            for (int i = 0; i < requests_per_thread; i++) {
                uint64_t user_id = user_dist(gen);
                uint64_t ad_id = ad_dist(gen);

                // 频控检查 (使用宽松的限制)
                FrequencyCap cap;
                cap.hourly_limit = 100;  // 宽松限制
                cap.daily_limit = 1000;

                bool frequency_allowed = !frequency_manager_->is_capped(user_id, ad_id, cap);

                // Pacing 检查
                const Ad* ad = index_->get_ad(ad_id);
                if (ad) {
                    bool pacing_allowed = pacing_manager_->allow_impression(ad->campaign_id, ad->bid_price);

                    if (!frequency_allowed) {
                        blocked_by_frequency++;
                    } else if (!pacing_allowed) {
                        blocked_by_pacing++;
                    } else {
                        success_count++;

                        // 异步记录
                        frequency_logger_->record_impression(user_id, ad_id, TimeWindow::HOUR);
                        pacing_updater_->record_impression(ad->campaign_id, ad->bid_price);
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 等待异步处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "成功投放: " << success_count << std::endl;
    std::cout << "频控拦截: " << blocked_by_frequency << std::endl;
    std::cout << "Pacing拦截: " << blocked_by_pacing << std::endl;

    // 验证数据一致性
    auto pacing_stats = pacing_manager_->get_stats();
    EXPECT_GT(pacing_stats.total_checks, 0);

    // 验证有成功的投放
    EXPECT_GT(success_count, 0);

    std::cout << "Pacing允许率: " << (pacing_stats.allow_rate * 100) << "%" << std::endl;
}

// 测试性能
TEST_F(CoreModulesIntegrationTest, PerformanceBenchmark) {
    constexpr int num_iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    int served = 0;
    for (int i = 0; i < num_iterations; i++) {
        uint64_t user_id = (i % 1000) + 1;
        uint64_t ad_id = 1000 + (i % 10);

        // 模拟投放流程
        FrequencyCap cap;
        cap.hourly_limit = 100;
        cap.daily_limit = 1000;

        if (frequency_manager_->is_capped(user_id, ad_id, cap)) {
            continue;
        }

        const Ad* ad = index_->get_ad(ad_id);
        if (!ad) continue;

        if (!pacing_manager_->allow_impression(ad->campaign_id, ad->bid_price)) {
            continue;
        }

        served++;

        // 每100次记录一次
        if (i % 100 == 0) {
            frequency_logger_->record_impression(user_id, ad_id, TimeWindow::HOUR);
            pacing_updater_->record_impression(ad->campaign_id, ad->bid_price);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double qps = num_iterations * 1000.0 / duration.count();

    std::cout << "完整流程性能测试:" << std::endl;
    std::cout << "迭代次数: " << num_iterations << std::endl;
    std::cout << "成功投放: " << served << std::endl;
    std::cout << "耗时: " << duration.count() << " ms" << std::endl;
    std::cout << "QPS: " << static_cast<int>(qps) << std::endl;

    // 性能应该 > 5K QPS (包含所有模块的完整流程)
    EXPECT_GT(qps, 5000);
}

// 测试内存使用
TEST_F(CoreModulesIntegrationTest, MemoryUsage) {
    // 获取索引统计
    auto index_stats = index_->get_stats();

    // 获取频控统计
    auto freq_stats = frequency_manager_->get_stats();

    std::cout << "内存使用统计:" << std::endl;
    std::cout << "索引内存: " << (index_stats.index_memory_bytes / 1024) << " KB" << std::endl;
    std::cout << "存储内存: " << (index_stats.store_memory_bytes / 1024) << " KB" << std::endl;
    std::cout << "频控内存: " << (freq_stats.memory_usage_bytes / 1024) << " KB" << std::endl;

    size_t total_memory_kb = (index_stats.index_memory_bytes +
                             index_stats.store_memory_bytes +
                             freq_stats.memory_usage_bytes) / 1024;

    std::cout << "总内存: " << total_memory_kb << " KB" << std::endl;

    // 内存使用应该合理 (< 10MB for 10 ads)
    EXPECT_LT(total_memory_kb, 10240);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
