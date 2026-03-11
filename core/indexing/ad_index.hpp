#pragma once

// Author: airprofly

#include "bitmap.hpp"
#include "inverted_index.hpp"
#include "ad_store.hpp"
#include "openrtb_models.h"
#include "ad_model.h"
#include <chrono>
#include <optional>
#include <vector>
#include <cstdint>

namespace flow_ad::indexing
{

// 查询结果
struct QueryResult
{
    std::vector<uint64_t> candidate_ad_ids;  // 候选广告 ID 列表
    size_t total_ads = 0;                    // 索引中的广告总数
    double recall_time_ms = 0.0;             // 召回耗时（毫秒）

    // 获取召回率（召回的广告数 / 总广告数）
    double recall_rate() const
    {
        return total_ads > 0 ? static_cast<double>(candidate_ad_ids.size()) / total_ads : 0.0;
    }
};

// 统计信息
struct IndexStats
{
    size_t total_ads = 0;                    // 广告总数
    size_t max_ad_id = 0;                    // 最大广告 ID
    size_t geo_keys = 0;                     // 地域索引键数量
    size_t device_keys = 0;                  // 设备索引键数量
    size_t os_keys = 0;                      // 操作系统索引键数量
    size_t interest_keys = 0;                // 兴趣索引键数量
    size_t index_memory_bytes = 0;           // 索引内存占用（字节）
    size_t store_memory_bytes = 0;           // 存储内存占用（字节）
    double avg_recall_time_ms = 0.0;         // 平均召回时间（毫秒）
    size_t total_queries = 0;                // 总查询次数
};

// 广告索引引擎（统一接口）
class AdIndex
{
public:
    AdIndex() = default;
    ~AdIndex() = default;

    // 禁止拷贝，允许移动
    AdIndex(const AdIndex&) = delete;
    AdIndex& operator=(const AdIndex&) = delete;
    AdIndex(AdIndex&&) = default;
    AdIndex& operator=(AdIndex&&) = default;

    // ========== 索引管理 ==========

    // 添加广告到索引
    void add_ad(uint64_t ad_id, const Ad& ad);

    // 批量添加广告
    void add_ads(const std::vector<std::pair<uint64_t, Ad>>& ads);

    // 从索引中移除广告
    bool remove_ad(uint64_t ad_id);

    // 清空索引
    void clear();

    // ========== 查询接口 ==========

    // 根据 OpenRTB 请求查询候选广告
    QueryResult query(const openrtb::OpenRTBRequest& request) const;

    // 根据 BitMap 查询候选广告（内部使用）
    std::vector<uint64_t> query_by_bitmap(const BitMap& bitmap) const;

    // ========== 广告访问 ==========

    // 获取广告详细信息（返回指针，如果不存在返回 nullptr）
    const Ad* get_ad(uint64_t ad_id) const;

    // 检查广告是否存在
    bool has_ad(uint64_t ad_id) const;

    // 获取所有广告 ID
    std::vector<uint64_t> get_all_ad_ids() const;

    // ========== 统计信息 ==========

    // 获取广告总数
    size_t size() const;

    // 检查是否为空
    bool empty() const;

    // 获取统计信息
    IndexStats get_stats() const;

    // 打印统计信息
    void print_stats() const;

private:
    InvertedIndex inverted_index_;  // 倒排索引
    AdStore ad_store_;              // 广告存储

    // 性能统计（mutable 因为需要在 const 方法中更新）
    mutable size_t total_queries_ = 0;
    mutable double total_recall_time_ms_ = 0.0;
};

} // namespace flow_ad::indexing
