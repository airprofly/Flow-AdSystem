#pragma once

// Author: airprofly

#include "ad_model.h"
#include <optional>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace flow_ad::indexing
{

class AdStore
{
public:
    AdStore() = default;
    ~AdStore() = default;

    // 禁止拷贝，允许移动
    AdStore(const AdStore&) = delete;
    AdStore& operator=(const AdStore&) = delete;
    AdStore(AdStore&&) = default;
    AdStore& operator=(AdStore&&) = default;

    // ========== 广告管理 ==========

    // 添加广告
    void add_ad(uint64_t ad_id, const Ad& ad);

    // 批量添加广告
    void add_ads(const std::vector<std::pair<uint64_t, Ad>>& ads);

    // 移除广告
    bool remove_ad(uint64_t ad_id);

    // 更新广告
    bool update_ad(uint64_t ad_id, const Ad& ad);

    // ========== 查询接口 ==========

    // 根据 ID 获取广告（返回指针，如果不存在返回 nullptr）
    const Ad* get_ad(uint64_t ad_id) const;

    // 检查广告是否存在
    bool has_ad(uint64_t ad_id) const;

    // 获取所有广告 ID
    std::vector<uint64_t> get_all_ad_ids() const;

    // ========== 统计信息 ==========

    // 获取广告总数
    size_t size() const
    {
        return ads_.size();
    }

    // 检查是否为空
    bool empty() const
    {
        return ads_.empty();
    }

    // 清空所有广告
    void clear();

    // 获取内存占用（估算）
    size_t memory_usage_bytes() const;

private:
    std::unordered_map<uint64_t, Ad> ads_;
};

} // namespace flow_ad::indexing
