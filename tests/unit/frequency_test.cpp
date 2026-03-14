// Author: airprofly
#include "async_impression_logger.hpp"
#include "frequency_manager.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

using namespace flow_ad::frequency;

class FrequencyManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        manager_ = std::make_unique<FrequencyManager>();
    }

    std::unique_ptr<FrequencyManager> manager_;
};

// 基本测试: 频控拦截
TEST_F(FrequencyManagerTest, BasicCapping)
{
    FrequencyCap cap;
    cap.hourly_limit = 3;
    cap.daily_limit = 10;
    cap.weekly_limit = 100;

    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    // 前 3 次应该允许
    for (int i = 0; i < 3; i++)
    {
        EXPECT_FALSE(manager_->is_capped(user_id, ad_id, cap));
        manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
    }

    // 第 4 次应该被拦截
    EXPECT_TRUE(manager_->is_capped(user_id, ad_id, cap));
}

// 多个时间窗口独立测试
TEST_F(FrequencyManagerTest, MultipleTimeWindows)
{
    FrequencyCap cap;
    cap.hourly_limit = 2;
    cap.daily_limit = 5;
    cap.weekly_limit = 10;

    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    // 记录 3 次展示
    for (int i = 0; i < 3; i++)
    {
        manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
        manager_->record_impression(user_id, ad_id, TimeWindow::DAY);
        manager_->record_impression(user_id, ad_id, TimeWindow::WEEK);
    }

    // 小时限制应该触发 (2次上限, 但记录了3次)
    EXPECT_TRUE(manager_->is_capped(user_id, ad_id, cap));

    // 验证各个时间窗口的独立计数
    EXPECT_EQ(manager_->get_count(user_id, ad_id, TimeWindow::HOUR), 3);
    EXPECT_EQ(manager_->get_count(user_id, ad_id, TimeWindow::DAY), 3);
    EXPECT_EQ(manager_->get_count(user_id, ad_id, TimeWindow::WEEK), 3);

    // 测试不同的时间窗口限制
    FrequencyCap cap2;
    cap2.hourly_limit = 10; // 不限制小时

    // 小时不限制，应该不被拦截
    EXPECT_FALSE(manager_->is_capped(user_id, ad_id, cap2));
}

// 并发测试: 多线程竞态
TEST_F(FrequencyManagerTest, ConcurrencyTest)
{
    FrequencyCap cap;
    cap.hourly_limit = 10;

    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    constexpr int num_threads = 10;
    constexpr int requests_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++)
    {
        threads.emplace_back([this, user_id, ad_id]()
                             {
            for (int i = 0; i < requests_per_thread; i++) {
                manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
            } });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    // 并发累加应准确无丢失
    EXPECT_EQ(
        manager_->get_count(user_id, ad_id, TimeWindow::HOUR),
        static_cast<uint32_t>(num_threads * requests_per_thread));

    // 达到上限后应被拦截
    EXPECT_TRUE(manager_->is_capped(user_id, ad_id, cap));
}

// 不同用户独立计数测试
TEST_F(FrequencyManagerTest, DifferentUsersIndependent)
{
    FrequencyCap cap;
    cap.hourly_limit = 3;

    uint64_t ad_id = 67890;

    // 用户1: 记录 3 次
    for (int i = 0; i < 3; i++)
    {
        manager_->record_impression(11111, ad_id, TimeWindow::HOUR);
    }

    // 用户2: 记录 1 次
    manager_->record_impression(22222, ad_id, TimeWindow::HOUR);

    // 用户1 应该被拦截
    EXPECT_TRUE(manager_->is_capped(11111, ad_id, cap));

    // 用户2 应该不被拦截
    EXPECT_FALSE(manager_->is_capped(22222, ad_id, cap));
}

// 不同广告独立计数测试
TEST_F(FrequencyManagerTest, DifferentAdsIndependent)
{
    FrequencyCap cap;
    cap.hourly_limit = 3;

    uint64_t user_id = 12345;

    // 广告1: 记录 3 次
    for (int i = 0; i < 3; i++)
    {
        manager_->record_impression(user_id, 11111, TimeWindow::HOUR);
    }

    // 广告2: 记录 1 次
    manager_->record_impression(user_id, 22222, TimeWindow::HOUR);

    // 广告1 应该被拦截
    EXPECT_TRUE(manager_->is_capped(user_id, 11111, cap));

    // 广告2 应该不被拦截
    EXPECT_FALSE(manager_->is_capped(user_id, 22222, cap));
}

// 计数测试
TEST_F(FrequencyManagerTest, GetCountTest)
{
    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    // 初始计数为 0
    EXPECT_EQ(manager_->get_count(user_id, ad_id, TimeWindow::HOUR), 0);

    // 记录 5 次
    for (int i = 0; i < 5; i++)
    {
        manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
    }

    // 计数应该是 5
    EXPECT_EQ(manager_->get_count(user_id, ad_id, TimeWindow::HOUR), 5);
}

// 无频控限制测试
TEST_F(FrequencyManagerTest, NoCapTest)
{
    FrequencyCap cap; // 所有限制都是 0

    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    // 记录 100 次
    for (int i = 0; i < 100; i++)
    {
        EXPECT_FALSE(manager_->is_capped(user_id, ad_id, cap));
        manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
    }

    // 应该永远不被拦截
    EXPECT_FALSE(manager_->is_capped(user_id, ad_id, cap));
}

// 性能测试
TEST_F(FrequencyManagerTest, PerformanceTest)
{
    FrequencyCap cap;
    cap.hourly_limit = 5;
    cap.daily_limit = 20;

    constexpr int num_iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++)
    {
        uint64_t user_id = i % 1000; // 1000 个不同用户
        uint64_t ad_id = i % 10000;  // 10000 个不同广告
        manager_->is_capped(user_id, ad_id, cap);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double qps = num_iterations * 1000000.0 / duration.count();
    std::cout << "Performance: " << qps << " QPS" << std::endl;

    // 应该 > 1M QPS
    EXPECT_GT(qps, 1000000);
}

// 统计测试
TEST_F(FrequencyManagerTest, StatsTest)
{
    auto stats = manager_->get_stats();

    EXPECT_EQ(stats.total_records, 0);
    EXPECT_EQ(stats.total_hits, 0);
    EXPECT_EQ(stats.total_misses, 0);

    // 添加一些数据
    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    manager_->record_impression(user_id, ad_id, TimeWindow::HOUR);
    manager_->get_count(user_id, ad_id, TimeWindow::HOUR);

    stats = manager_->get_stats();
    EXPECT_GT(stats.total_records, 0);
    EXPECT_GT(stats.total_hits, 0);
}

// 异步记录器测试
class AsyncImpressionLoggerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        manager_ = std::make_shared<FrequencyManager>();
        logger_ = std::make_unique<AsyncImpressionLogger>(
            manager_,
            10000, // queue_size
            100,   // batch_size
            50     // flush_interval_ms
        );
    }

    void TearDown() override
    {
        // 等待队列处理完成
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::shared_ptr<FrequencyManager> manager_;
    std::unique_ptr<AsyncImpressionLogger> logger_;
};

// 异步记录基本测试
TEST_F(AsyncImpressionLoggerTest, BasicRecordTest)
{
    uint64_t user_id = 12345;
    uint64_t ad_id = 67890;

    // 记录 5 次
    for (int i = 0; i < 5; i++)
    {
        EXPECT_TRUE(logger_->record_impression(user_id, ad_id, TimeWindow::HOUR));
    }

    // 等待异步处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 验证计数
    uint32_t count = manager_->get_count(user_id, ad_id, TimeWindow::HOUR);
    EXPECT_EQ(count, 5);
}

// 异步记录器统计测试
TEST_F(AsyncImpressionLoggerTest, StatsTest)
{
    auto stats = logger_->get_stats();

    EXPECT_EQ(stats.queued_count, 0);
    EXPECT_EQ(stats.processed_count, 0);
    EXPECT_EQ(stats.dropped_count, 0);

    // 记录一些数据
    for (int i = 0; i < 10; i++)
    {
        logger_->record_impression(i, i * 100, TimeWindow::HOUR);
    }

    stats = logger_->get_stats();
    EXPECT_GT(stats.current_queue_size, 0);
}

// 主函数
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
