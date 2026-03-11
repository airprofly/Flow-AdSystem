// Author: airprofly

#include "ad_index.hpp"
#include <iostream>
#include <iomanip>

namespace flow_ad::indexing
{

void AdIndex::add_ad(uint64_t ad_id, const Ad& ad)
{
    inverted_index_.add_ad(ad_id, ad);
    ad_store_.add_ad(ad_id, ad);
}

void AdIndex::add_ads(const std::vector<std::pair<uint64_t, Ad>>& ads)
{
    for (const auto& [ad_id, ad] : ads)
    {
        add_ad(ad_id, ad);
    }
}

bool AdIndex::remove_ad(uint64_t ad_id)
{
    if (!ad_store_.has_ad(ad_id))
    {
        return false;
    }

    inverted_index_.remove_ad(ad_id);
    ad_store_.remove_ad(ad_id);
    return true;
}

void AdIndex::clear()
{
    inverted_index_.clear();
    ad_store_.clear();
    total_queries_ = 0;
    total_recall_time_ms_ = 0.0;
}

QueryResult AdIndex::query(const openrtb::OpenRTBRequest& request) const
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // 使用倒排索引查询
    BitMap bitmap = inverted_index_.query(request);

    // 将 BitMap 转换为广告 ID 列表
    std::vector<uint64_t> candidate_ids = query_by_bitmap(bitmap);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

    // 更新性能统计
    total_queries_++;
    total_recall_time_ms_ += elapsed.count();

    // 构建结果
    QueryResult result;
    result.candidate_ad_ids = std::move(candidate_ids);
    result.total_ads = size();
    result.recall_time_ms = elapsed.count();

    return result;
}

std::vector<uint64_t> AdIndex::query_by_bitmap(const BitMap& bitmap) const
{
    std::vector<uint64_t> ad_ids;
    ad_ids.reserve(bitmap.count());

    auto set_bits = bitmap.get_set_bits();
    for (auto ad_id : set_bits)
    {
        // 只返回存在的广告 ID（避免已删除的广告）
        if (ad_store_.has_ad(ad_id))
        {
            ad_ids.push_back(ad_id);
        }
    }

    return ad_ids;
}

const Ad* AdIndex::get_ad(uint64_t ad_id) const
{
    return ad_store_.get_ad(ad_id);
}

bool AdIndex::has_ad(uint64_t ad_id) const
{
    return ad_store_.has_ad(ad_id);
}

std::vector<uint64_t> AdIndex::get_all_ad_ids() const
{
    return ad_store_.get_all_ad_ids();
}

size_t AdIndex::size() const
{
    return ad_store_.size();
}

bool AdIndex::empty() const
{
    return ad_store_.empty();
}

IndexStats AdIndex::get_stats() const
{
    IndexStats stats;
    stats.total_ads = size();
    stats.max_ad_id = inverted_index_.max_ad_id();
    stats.index_memory_bytes = inverted_index_.memory_usage_bytes();
    stats.store_memory_bytes = ad_store_.memory_usage_bytes();
    stats.total_queries = total_queries_;
    stats.avg_recall_time_ms = total_queries_ > 0 ? total_recall_time_ms_ / total_queries_ : 0.0;

    // TODO: 如果需要，可以添加更多统计信息
    // stats.geo_keys = inverted_index_.geo_index_size();
    // stats.device_keys = inverted_index_.device_index_size();
    // stats.os_keys = inverted_index_.os_index_size();
    // stats.interest_keys = inverted_index_.interest_index_size();

    return stats;
}

void AdIndex::print_stats() const
{
    auto stats = get_stats();

    std::cout << "========================================" << std::endl;
    std::cout << "        Ad Index Statistics             " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Total Ads:          " << std::setw(12) << stats.total_ads << std::endl;
    std::cout << "Max Ad ID:          " << std::setw(12) << stats.max_ad_id << std::endl;
    std::cout << "Total Queries:      " << std::setw(12) << stats.total_queries << std::endl;
    std::cout << "Avg Recall Time:    " << std::setw(11) << std::fixed << std::setprecision(3)
              << stats.avg_recall_time_ms << " ms" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Index Memory:       " << std::setw(10) << (stats.index_memory_bytes / 1024.0 / 1024.0)
              << " MB" << std::endl;
    std::cout << "Store Memory:       " << std::setw(10) << (stats.store_memory_bytes / 1024.0 / 1024.0)
              << " MB" << std::endl;
    std::cout << "Total Memory:       " << std::setw(10) << ((stats.index_memory_bytes + stats.store_memory_bytes) / 1024.0 / 1024.0)
              << " MB" << std::endl;
    std::cout << "========================================" << std::endl;
}

} // namespace flow_ad::indexing
