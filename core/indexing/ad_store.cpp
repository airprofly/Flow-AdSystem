// Author: airprofly

#include "ad_store.hpp"

namespace flow_ad::indexing
{

void AdStore::add_ad(uint64_t ad_id, const Ad& ad)
{
    ads_[ad_id] = ad;
}

void AdStore::add_ads(const std::vector<std::pair<uint64_t, Ad>>& ads)
{
    for (const auto& [ad_id, ad] : ads)
    {
        ads_[ad_id] = ad;
    }
}

bool AdStore::remove_ad(uint64_t ad_id)
{
    auto it = ads_.find(ad_id);
    if (it != ads_.end())
    {
        ads_.erase(it);
        return true;
    }
    return false;
}

bool AdStore::update_ad(uint64_t ad_id, const Ad& ad)
{
    auto it = ads_.find(ad_id);
    if (it != ads_.end())
    {
        it->second = ad;
        return true;
    }
    return false;
}

const Ad* AdStore::get_ad(uint64_t ad_id) const
{
    auto it = ads_.find(ad_id);
    if (it != ads_.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool AdStore::has_ad(uint64_t ad_id) const
{
    return ads_.find(ad_id) != ads_.end();
}

std::vector<uint64_t> AdStore::get_all_ad_ids() const
{
    std::vector<uint64_t> ids;
    ids.reserve(ads_.size());
    for (const auto& [ad_id, _] : ads_)
    {
        ids.push_back(ad_id);
    }
    return ids;
}

void AdStore::clear()
{
    ads_.clear();
}

size_t AdStore::memory_usage_bytes() const
{
    size_t total = sizeof(ads_);

    for (const auto& [ad_id, ad] : ads_)
    {
        // 估算每个 Ad 对象的内存占用
        total += sizeof(uint64_t) + sizeof(Ad);

        // 加上 vector 的开销
        total += ad.categories.capacity() * sizeof(AdCategory);
        total += ad.targeting_geo.capacity() * sizeof(std::string);
        total += ad.targeting_devices.capacity() * sizeof(DeviceType);
        total += ad.targeting_interests.capacity() * sizeof(std::string);

        // 加上字符串内容
        for (const auto& geo : ad.targeting_geo)
        {
            total += geo.capacity();
        }
        for (const auto& interest : ad.targeting_interests)
        {
            total += interest.capacity();
        }
    }

    return total;
}

} // namespace flow_ad::indexing
