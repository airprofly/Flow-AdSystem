#pragma once

// Author: airprofly

#include "bitmap.hpp"
#include "ad_model.h"
#include "openrtb_models.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace flow_ad::indexing
{

class InvertedIndex
{
public:
    InvertedIndex() = default;
    ~InvertedIndex() = default;

    // 禁止拷贝，允许移动
    InvertedIndex(const InvertedIndex&) = delete;
    InvertedIndex& operator=(const InvertedIndex&) = delete;
    InvertedIndex(InvertedIndex&&) = default;
    InvertedIndex& operator=(InvertedIndex&&) = default;

    // ========== 索引管理 ==========

    // 添加广告到索引
    void add_ad(uint64_t ad_id, const Ad& ad);

    // 从索引中移除广告
    void remove_ad(uint64_t ad_id);

    // ========== 查询接口 ==========

    // 根据 OpenRTB 请求查询符合条件的广告（返回 BitMap）
    BitMap query(const openrtb::OpenRTBRequest& request) const;

    // ========== 统计信息 ==========

    // 获取索引中的广告总数
    size_t size() const
    {
        return ad_count_;
    }

    // 获取最大广告 ID
    size_t max_ad_id() const
    {
        return max_ad_id_;
    }

    // 获取内存占用（估算）
    size_t memory_usage_bytes() const;

    // 清空索引
    void clear();

private:
    // 倒排索引结构
    std::unordered_map<std::string, BitMap> geo_index_;       // 地域索引
    std::unordered_map<int, BitMap> device_index_;            // 设备索引
    std::unordered_map<int, BitMap> os_index_;                // 操作系统索引
    std::unordered_map<std::string, BitMap> interest_index_;  // 兴趣索引

    // 用于跟踪每个广告 ID 属于哪些索引键
    std::unordered_map<uint64_t, std::vector<std::string>> ad_geo_keys_;
    std::unordered_map<uint64_t, std::vector<int>> ad_device_keys_;
    std::unordered_map<uint64_t, std::vector<int>> ad_os_keys_;
    std::unordered_map<uint64_t, std::vector<std::string>> ad_interest_keys_;

    size_t ad_count_ = 0;   // 广告总数
    size_t max_ad_id_ = 0;  // 最大广告 ID（用于 BitMap 大小）
};

} // namespace flow_ad::indexing
