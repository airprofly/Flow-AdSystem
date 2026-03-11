#pragma once

// Author: airprofly

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace flow_ad
{

    // -----------------------------------------------------------------------
    // Enumerations
    // -----------------------------------------------------------------------

    enum class AdCategory
    {
        ECOMMERCE = 0,
        GAMING,
        FINANCE,
        EDUCATION,
        HEALTH,
        AUTOMOTIVE,
        TRAVEL,
        ENTERTAINMENT,
        TECHNOLOGY,
        FOOD
    };

    enum class DeviceType
    {
        MOBILE = 0,
        DESKTOP,
        TABLET
    };

    enum class AdStatus
    {
        PENDING = 0,
        ACTIVE,
        PAUSED,
        ENDED,
        DELETED
    };

    // -----------------------------------------------------------------------
    // Enum to String Conversions
    // -----------------------------------------------------------------------

    inline std::string ad_category_to_string(AdCategory category)
    {
        static const std::string category_names[] = {
            "ecommerce",     // ECOMMERCE = 0
            "gaming",        // GAMING = 1
            "finance",       // FINANCE = 2
            "education",     // EDUCATION = 3
            "health",        // HEALTH = 4
            "automotive",    // AUTOMOTIVE = 5
            "travel",        // TRAVEL = 6
            "entertainment", // ENTERTAINMENT = 7
            "technology",    // TECHNOLOGY = 8
            "food"           // FOOD = 9
        };

        const auto index = static_cast<size_t>(category);
        if (index < sizeof(category_names) / sizeof(category_names[0]))
        {
            return category_names[index];
        }
        return "unknown";
    }

    inline std::string device_type_to_string(DeviceType device)
    {
        static const std::string device_names[] = {
            "mobile",  // MOBILE = 0
            "desktop", // DESKTOP = 1
            "tablet"   // TABLET = 2
        };

        const auto index = static_cast<size_t>(device);
        if (index < sizeof(device_names) / sizeof(device_names[0]))
        {
            return device_names[index];
        }
        return "unknown";
    }

    inline std::string ad_status_to_string(AdStatus status)
    {
        static const std::string status_names[] = {
            "pending", // PENDING = 0
            "active",  // ACTIVE = 1
            "paused",  // PAUSED = 2
            "ended",   // ENDED = 3
            "deleted"  // DELETED = 4
        };

        const auto index = static_cast<size_t>(status);
        if (index < sizeof(status_names) / sizeof(status_names[0]))
        {
            return status_names[index];
        }
        return "unknown";
    }

    // -----------------------------------------------------------------------
    // Data models
    // -----------------------------------------------------------------------

    struct Advertiser
    {
        uint64_t id{0};
        std::string name;
        std::string category;
    };

    struct AdCampaign
    {
        uint64_t id{0};
        uint64_t advertiser_id{0};
        std::string name;
        double budget{0.0};
        double spent{0.0};
        double bid_price{0.0};
        AdStatus status{AdStatus::PENDING};
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
    };

    struct Creative
    {
        uint64_t id{0};
        std::string title;
        std::string description;
        std::string url;
        std::string type; // "banner" | "video" | "native"
        uint32_t width{0};
        uint32_t height{0};
    };

    struct Ad
    {
        uint64_t id{0};
        uint64_t campaign_id{0};
        uint64_t creative_id{0};
        double bid_price{0.0};
        std::vector<AdCategory> categories;
        std::vector<std::string> targeting_geo;
        std::vector<DeviceType> targeting_devices;
        std::vector<std::string> targeting_interests;
    };

} // namespace flow_ad
