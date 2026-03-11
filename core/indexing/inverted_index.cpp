// Author: airprofly

#include "inverted_index.hpp"
#include <algorithm>

namespace flow_ad::indexing
{

void InvertedIndex::add_ad(uint64_t ad_id, const Ad& ad)
{
    // 更新最大 ID
    max_ad_id_ = std::max(max_ad_id_, ad_id);
    ad_count_++;

    // 1. 地域索引
    for (const auto& geo : ad.targeting_geo)
    {
        auto& bitmap = geo_index_[geo];
        if (bitmap.size() <= ad_id)
        {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
        ad_geo_keys_[ad_id].push_back(geo);
    }

    // 2. 设备索引（将 DeviceType 转换为 int）
    for (const auto& device : ad.targeting_devices)
    {
        int device_key = static_cast<int>(device);
        auto& bitmap = device_index_[device_key];
        if (bitmap.size() <= ad_id)
        {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
        ad_device_keys_[ad_id].push_back(device_key);
    }

    // 3. 兴趣索引
    for (const auto& interest : ad.targeting_interests)
    {
        auto& bitmap = interest_index_[interest];
        if (bitmap.size() <= ad_id)
        {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
        ad_interest_keys_[ad_id].push_back(interest);
    }
}

void InvertedIndex::remove_ad(uint64_t ad_id)
{
    // 从地域索引中移除
    auto geo_it = ad_geo_keys_.find(ad_id);
    if (geo_it != ad_geo_keys_.end())
    {
        for (const auto& geo : geo_it->second)
        {
            auto bitmap_it = geo_index_.find(geo);
            if (bitmap_it != geo_index_.end())
            {
                bitmap_it->second.clear(ad_id);
            }
        }
        ad_geo_keys_.erase(geo_it);
    }

    // 从设备索引中移除
    auto device_it = ad_device_keys_.find(ad_id);
    if (device_it != ad_device_keys_.end())
    {
        for (const auto& device_key : device_it->second)
        {
            auto bitmap_it = device_index_.find(device_key);
            if (bitmap_it != device_index_.end())
            {
                bitmap_it->second.clear(ad_id);
            }
        }
        ad_device_keys_.erase(device_it);
    }

    // 从操作系统索引中移除
    auto os_it = ad_os_keys_.find(ad_id);
    if (os_it != ad_os_keys_.end())
    {
        for (const auto& os_key : os_it->second)
        {
            auto bitmap_it = os_index_.find(os_key);
            if (bitmap_it != os_index_.end())
            {
                bitmap_it->second.clear(ad_id);
            }
        }
        ad_os_keys_.erase(os_it);
    }

    // 从兴趣索引中移除
    auto interest_it = ad_interest_keys_.find(ad_id);
    if (interest_it != ad_interest_keys_.end())
    {
        for (const auto& interest : interest_it->second)
        {
            auto bitmap_it = interest_index_.find(interest);
            if (bitmap_it != interest_index_.end())
            {
                bitmap_it->second.clear(ad_id);
            }
        }
        ad_interest_keys_.erase(interest_it);
    }

    ad_count_--;
}

BitMap InvertedIndex::query(const openrtb::OpenRTBRequest& request) const
{
    std::vector<const BitMap*> and_conditions;
    std::vector<const BitMap*> or_conditions;

    // 1. 地域过滤（AND 条件）
    if (request.device && request.device->geo && request.device->geo->city)
    {
        const auto& city = *request.device->geo->city;
        auto it = geo_index_.find(city);
        if (it != geo_index_.end())
        {
            and_conditions.push_back(&it->second);
        }
        else
        {
            // 没有广告定向到该城市，返回空结果
            return BitMap(max_ad_id_ + 1);
        }
    }

    // 2. 设备类型过滤（AND 条件）
    if (request.device && request.device->devicetype)
    {
        // OpenRTB DeviceType 到 Ad DeviceType 的转换
        // openrtb::DeviceType::MOBILE(1) -> flow_ad::DeviceType::MOBILE(0)
        // openrtb::DeviceType::PERSONAL_COMPUTER(2) -> flow_ad::DeviceType::DESKTOP(1)
        // openrtb::DeviceType::TABLET(5) -> flow_ad::DeviceType::TABLET(2)
        int device_key = -1;
        switch (*request.device->devicetype)
        {
            case 1: // openrtb::DeviceType::MOBILE
                device_key = static_cast<int>(flow_ad::DeviceType::MOBILE);
                break;
            case 2: // openrtb::DeviceType::PERSONAL_COMPUTER
                device_key = static_cast<int>(flow_ad::DeviceType::DESKTOP);
                break;
            case 5: // openrtb::DeviceType::TABLET
                device_key = static_cast<int>(flow_ad::DeviceType::TABLET);
                break;
            default:
                // 其他设备类型暂不处理
                break;
        }

        if (device_key >= 0)
        {
            auto it = device_index_.find(device_key);
            if (it != device_index_.end())
            {
                and_conditions.push_back(&it->second);
            }
        }
    }

    // 3. 操作系统过滤（AND 条件）
    if (request.device && request.device->os)
    {
        auto it = os_index_.find(*request.device->os);
        if (it != os_index_.end())
        {
            and_conditions.push_back(&it->second);
        }
    }

    // 4. 兴趣过滤（OR 条件：满足任意兴趣即可）
    if (request.user && request.user->keywords && !request.user->keywords->empty())
    {
        for (const auto& keyword : *request.user->keywords)
        {
            auto it = interest_index_.find(keyword);
            if (it != interest_index_.end())
            {
                or_conditions.push_back(&it->second);
            }
        }
    }

    // 执行查询
    BitMap result(max_ad_id_ + 1);

    // 如果没有 AND 条件，返回所有广告的 BitMap
    if (and_conditions.empty())
    {
        // 初始化为全 1（这里我们假设所有广告 ID 都是连续的）
        // 更好的方式是维护一个全局的 BitMap
        for (size_t i = 0; i <= max_ad_id_; ++i)
        {
            result.set(i);
        }
    }
    else
    {
        // 求所有 AND 条件的交集
        result = *and_conditions[0];
        for (size_t i = 1; i < and_conditions.size(); ++i)
        {
            result = result & *and_conditions[i];
        }
    }

    // 如果有 OR 条件，求并集后再与结果求交集
    if (!or_conditions.empty())
    {
        BitMap or_result(max_ad_id_ + 1);
        or_result = *or_conditions[0];
        for (size_t i = 1; i < or_conditions.size(); ++i)
        {
            or_result = or_result | *or_conditions[i];
        }
        result = result & or_result;
    }

    return result;
}

size_t InvertedIndex::memory_usage_bytes() const
{
    size_t total = 0;

    // 计算每个索引的内存占用
    for (const auto& [key, bitmap] : geo_index_)
    {
        total += bitmap.memory_usage_bytes() + sizeof(std::string) + key.size();
    }

    for (const auto& [key, bitmap] : device_index_)
    {
        total += bitmap.memory_usage_bytes() + sizeof(int);
    }

    for (const auto& [key, bitmap] : os_index_)
    {
        total += bitmap.memory_usage_bytes() + sizeof(int);
    }

    for (const auto& [key, bitmap] : interest_index_)
    {
        total += bitmap.memory_usage_bytes() + sizeof(std::string) + key.size();
    }

    // 加上跟踪映射的内存占用
    total += ad_geo_keys_.size() * (sizeof(uint64_t) + sizeof(std::vector<std::string>));
    total += ad_device_keys_.size() * (sizeof(uint64_t) + sizeof(std::vector<int>));
    total += ad_os_keys_.size() * (sizeof(uint64_t) + sizeof(std::vector<int>));
    total += ad_interest_keys_.size() * (sizeof(uint64_t) + sizeof(std::vector<std::string>));

    return total;
}

void InvertedIndex::clear()
{
    geo_index_.clear();
    device_index_.clear();
    os_index_.clear();
    interest_index_.clear();
    ad_geo_keys_.clear();
    ad_device_keys_.clear();
    ad_os_keys_.clear();
    ad_interest_keys_.clear();
    ad_count_ = 0;
    max_ad_id_ = 0;
}

} // namespace flow_ad::indexing
