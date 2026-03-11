#pragma once

// Author: airprofly

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flow_ad::openrtb
{

    // -------------------------------------------------------------------------
    // Enumerations (OpenRTB 2.5 Specification)
    // -------------------------------------------------------------------------

    // 设备类型 (OpenRTB 2.5 Section 3.2.18)
    enum class DeviceType : int
    {
        UNKNOWN = 0,
        MOBILE = 1,            // 手机/平板
        PERSONAL_COMPUTER = 2, // 个人电脑
        TV = 3,                // 电视
        PHONE = 4,             // 手机
        TABLET = 5,            // 平板
        CONNECTED_DEVICE = 6,  // 连接设备
        SET_TOP_BOX = 7        // 机顶盒
    };

    // 操作系统 (OpenRTB 2.5 Section 3.2.19)
    enum class OS : int
    {
        UNKNOWN = 0,
        IOS = 1,
        ANDROID = 2,
        WINDOWS = 3,
        OSX = 4,
        LINUX = 5
    };

    // 连接类型 (OpenRTB 2.5 Section 3.2.20)
    enum class ConnectionType : int
    {
        UNKNOWN = 0,
        ETHERNET = 1,
        WIFI = 2,
        DATA_2G = 3,
        DATA_3G = 4,
        DATA_4G = 5
    };

    // 广告位置 (OpenRTB 2.5 Section 3.2.7)
    enum class AdPosition : int
    {
        UNKNOWN = 0,
        ABOVE_FOLD = 1,
        DEPRECATED = 2,
        BELOW_FOLD = 3,
        HEADER = 4,
        FOOTER = 5,
        SIDEBAR = 6,
        FULL_SCREEN = 7
    };

    // 拍卖类型 (OpenRTB 2.5 Section 3.2.1)
    enum class AuctionType : int
    {
        FIRST_PRICE = 1,
        SECOND_PRICE = 2
    };

    // 性别 (OpenRTB 2.5 Section 3.2.13)
    enum class Gender : char
    {
        UNKNOWN = 'O',
        MALE = 'M',
        FEMALE = 'F'
    };

    // -------------------------------------------------------------------------
    // Core Data Structures
    // -------------------------------------------------------------------------

    // 地理位置 (OpenRTB 2.5 Section 3.2.15)
    struct Geo
    {
        std::optional<std::string> country; // ISO-3166-1 alpha-3
        std::optional<std::string> region;  // ISO-3166-2
        std::optional<std::string> city;    // 城市名称
        std::optional<double> lat;          // 纬度
        std::optional<double> lon;          // 经度
        std::optional<int> type;            // 位置精度 (1=GPS, 2=IP, etc.)
        std::optional<std::string> ext;     // 扩展字段
    };

    // Banner 对象 (OpenRTB 2.5 Section 3.2.6)
    struct Banner
    {
        std::optional<std::vector<int>> w;       // 宽度 (支持多个)
        std::optional<std::vector<int>> h;       // 高度 (支持多个)
        std::optional<int> wmax;                 // 最大宽度
        std::optional<int> hmax;                 // 最大高度
        std::optional<int> wmin;                 // 最小宽度
        std::optional<int> hmin;                 // 最小高度
        std::optional<int> pos;                  // 位置 (AdPosition)
        std::optional<std::vector<int>> btype;   // Banner 类型
        std::optional<std::vector<int>> battr;   // Banner 属性
        std::optional<std::string> ext;          // 扩展字段

        // 辅助方法: 获取主要尺寸
        std::pair<int, int> get_primary_size() const
        {
            int width = 0, height = 0;
            if (w && !w->empty())
                width = (*w)[0];
            if (h && !h->empty())
                height = (*h)[0];
            return {width, height};
        }
    };

    // Video 对象 (OpenRTB 2.5 Section 3.2.8)
    struct Video
    {
        std::optional<std::vector<int>> mimes;      // MIME 类型
        std::optional<int> w;                       // 宽度
        std::optional<int> h;                       // 高度
        std::optional<int> startdelay;              // 开始延迟
        std::optional<std::vector<int>> protocols;  // 协议
        std::optional<int> maxduration;             // 最大时长
        std::optional<std::vector<int>> api;        // API 框架
        std::optional<std::string> ext;             // 扩展字段
    };

    // Native 对象 (OpenRTB 2.5 Section 3.2.9)
    struct Native
    {
        std::optional<std::string> request; // Native 请求字符串
        std::optional<std::string> ver;     // Native 版本
        std::vector<std::unordered_map<std::string, std::string>>
            assets;                         // 资源列表
        std::optional<std::string> ext;     // 扩展字段
    };

    // 广告位对象 (OpenRTB 2.5 Section 3.2.2)
    struct Impression
    {
        std::string id;                           // 必填: 广告位 ID
        std::optional<Banner> banner;             // Banner 信息
        std::optional<Video> video;               // 视频信息
        std::optional<Native> native_;            // 原生广告信息
        std::optional<double> bidfloor;           // 底价
        std::optional<std::string> bidfloorcur;   // 底价货币 (默认 USD)
        std::optional<bool> bidfloor_priority;    // 是否强制底价
        std::optional<std::vector<int>> secure;   // 是否需要 HTTPS
        std::optional<std::vector<int>> iframebuster; // iframe 破解
        std::optional<std::string> ext;           // 扩展字段

        // 辅助方法: 获取广告位类型
        std::string get_impression_type() const
        {
            if (banner)
                return "banner";
            if (video)
                return "video";
            if (native_)
                return "native";
            return "unknown";
        }
    };

    // 设备对象 (OpenRTB 2.5 Section 3.2.18)
    struct Device
    {
        std::optional<std::string> ua;      // User-Agent
        std::optional<std::string> ip;      // IP 地址 (IPv4)
        std::optional<std::string> ipv6;    // IPv6 地址
        std::optional<Geo> geo;             // 地理位置
        std::optional<int> devicetype;      // 设备类型 (DeviceType)
        std::optional<int> os;              // 操作系统 (OS)
        std::optional<std::string> osv;     // 操作系统版本
        std::optional<int> dnt;             // Do Not Track
        std::optional<int> lmt;             // Limit Ad Tracking
        std::optional<int> connectiontype;  // 连接类型 (ConnectionType)
        std::optional<std::string> carrier; // 运营商
        std::optional<std::string> make;    // 设备制造商
        std::optional<std::string> model;   // 设备型号
        std::optional<std::string> ext;     // 扩展字段
    };

    // 用户对象 (OpenRTB 2.5 Section 3.2.13)
    struct User
    {
        std::string id;                                // 必填: 用户 ID
        std::optional<std::string> buyeruid;           // 买家用户 ID (DSP 用)
        std::optional<int> yob;                        // 出生年份
        std::optional<std::string> gender;             // 性别 (Gender: M/F/O)
        std::optional<Geo> geo;                        // 地理位置
        std::optional<std::vector<std::string>> keywords; // 关键词/兴趣
        std::optional<std::string> ext;                // 扩展字段
    };

    // 应用对象 (OpenRTB 2.5 Section 3.2.16)
    struct App
    {
        std::optional<std::string> id;                 // 应用 ID
        std::optional<std::string> name;               // 应用名称
        std::optional<std::string> bundle;             // 包名 (Android) / Bundle ID (iOS)
        std::optional<std::string> domain;             // 应用域名
        std::optional<std::string> storeurl;           // 应用商店 URL
        std::optional<std::vector<int>> cat;           // IAB 内容分类
        std::optional<std::vector<std::string>> sectioncat; // 子分类
        std::optional<std::vector<std::string>> pagecat;    // 页面分类
        std::optional<std::string> ver;                // 应用版本
        std::optional<std::string> ext;                // 扩展字段
    };

    // 站点对象 (OpenRTB 2.5 Section 3.2.17)
    struct Site
    {
        std::optional<std::string> id;                 // 站点 ID
        std::optional<std::string> name;               // 站点名称
        std::optional<std::string> domain;             // 站点域名
        std::optional<std::vector<int>> cat;           // IAB 内容分类
        std::optional<std::vector<std::string>> sectioncat; // 子分类
        std::optional<std::vector<std::string>> pagecat;    // 页面分类
        std::optional<std::string> page;               // 页面 URL
        std::optional<std::string> ref;                // 来源 URL
        std::optional<std::string> ext;                // 扩展字段
    };

    // OpenRTB Bid Request 根对象 (OpenRTB 2.5 Section 3.2.1)
    struct OpenRTBRequest
    {
        std::string id;                           // 必填: 请求唯一标识
        std::vector<Impression> imp;              // 必填: 广告位列表
        std::optional<Device> device;             // 推荐: 设备信息
        std::optional<User> user;                 // 推荐: 用户信息
        std::optional<App> app;                   // 可选: 应用信息 (与 site 互斥)
        std::optional<Site> site;                 // 可选: 站点信息 (与 app 互斥)
        std::optional<int> at;                    // 拍卖类型 (AuctionType, 默认 2)
        std::optional<std::vector<int>> bcat;     // 屏蔽的广告类别 (IAB)
        std::optional<std::vector<std::string>> badv; // 屏蔽的广告主域名
        std::optional<std::vector<std::string>> bapp;   // 屏蔽的应用 ID
        std::optional<std::string> ext;           // 扩展字段

        // 辅助方法
        bool is_app() const { return app.has_value(); }
        bool is_site() const { return site.has_value(); }
        size_t get_impression_count() const { return imp.size(); }
    };

    // -------------------------------------------------------------------------
    // Helper Functions - Enum to String
    // -------------------------------------------------------------------------

    inline std::string device_type_to_string(DeviceType type)
    {
        switch (type)
        {
        case DeviceType::MOBILE:
            return "Mobile";
        case DeviceType::PERSONAL_COMPUTER:
            return "Personal Computer";
        case DeviceType::TV:
            return "TV";
        case DeviceType::PHONE:
            return "Phone";
        case DeviceType::TABLET:
            return "Tablet";
        case DeviceType::CONNECTED_DEVICE:
            return "Connected Device";
        case DeviceType::SET_TOP_BOX:
            return "Set-Top Box";
        default:
            return "Unknown";
        }
    }

    inline std::string os_to_string(OS os)
    {
        switch (os)
        {
        case OS::IOS:
            return "iOS";
        case OS::ANDROID:
            return "Android";
        case OS::WINDOWS:
            return "Windows";
        case OS::OSX:
            return "OSX";
        case OS::LINUX:
            return "Linux";
        default:
            return "Unknown";
        }
    }

    inline std::string connection_type_to_string(ConnectionType type)
    {
        switch (type)
        {
        case ConnectionType::ETHERNET:
            return "Ethernet";
        case ConnectionType::WIFI:
            return "WiFi";
        case ConnectionType::DATA_2G:
            return "2G";
        case ConnectionType::DATA_3G:
            return "3G";
        case ConnectionType::DATA_4G:
            return "4G";
        default:
            return "Unknown";
        }
    }

    inline std::string ad_position_to_string(AdPosition pos)
    {
        switch (pos)
        {
        case AdPosition::ABOVE_FOLD:
            return "Above Fold";
        case AdPosition::BELOW_FOLD:
            return "Below Fold";
        case AdPosition::HEADER:
            return "Header";
        case AdPosition::FOOTER:
            return "Footer";
        case AdPosition::SIDEBAR:
            return "Sidebar";
        case AdPosition::FULL_SCREEN:
            return "Full Screen";
        default:
            return "Unknown";
        }
    }

    inline std::string auction_type_to_string(AuctionType type)
    {
        switch (type)
        {
        case AuctionType::FIRST_PRICE:
            return "First Price";
        case AuctionType::SECOND_PRICE:
            return "Second Price";
        default:
            return "Unknown";
        }
    }

    inline std::string gender_to_string(Gender gender)
    {
        switch (gender)
        {
        case Gender::MALE:
            return "Male";
        case Gender::FEMALE:
            return "Female";
        default:
            return "Unknown";
        }
    }

    // -------------------------------------------------------------------------
    // Helper Functions - String to Enum
    // -------------------------------------------------------------------------

    inline std::optional<DeviceType> string_to_device_type(const std::string &str)
    {
        if (str == "Mobile" || str == "1")
            return DeviceType::MOBILE;
        if (str == "Personal Computer" || str == "PC" || str == "2")
            return DeviceType::PERSONAL_COMPUTER;
        if (str == "TV" || str == "3")
            return DeviceType::TV;
        if (str == "Phone" || str == "4")
            return DeviceType::PHONE;
        if (str == "Tablet" || str == "5")
            return DeviceType::TABLET;
        if (str == "Connected Device" || str == "6")
            return DeviceType::CONNECTED_DEVICE;
        if (str == "Set-Top Box" || str == "7")
            return DeviceType::SET_TOP_BOX;
        return std::nullopt;
    }

    inline std::optional<OS> string_to_os(const std::string &str)
    {
        if (str == "iOS" || str == "1")
            return OS::IOS;
        if (str == "Android" || str == "2")
            return OS::ANDROID;
        if (str == "Windows" || str == "3")
            return OS::WINDOWS;
        if (str == "OSX" || str == "Mac" || str == "4")
            return OS::OSX;
        if (str == "Linux" || str == "5")
            return OS::LINUX;
        return std::nullopt;
    }

    inline std::optional<ConnectionType> string_to_connection_type(const std::string &str)
    {
        if (str == "Ethernet" || str == "1")
            return ConnectionType::ETHERNET;
        if (str == "WiFi" || str == "WIFI" || str == "2")
            return ConnectionType::WIFI;
        if (str == "2G" || str == "3")
            return ConnectionType::DATA_2G;
        if (str == "3G" || str == "4")
            return ConnectionType::DATA_3G;
        if (str == "4G" || str == "5")
            return ConnectionType::DATA_4G;
        return std::nullopt;
    }

    inline std::optional<AdPosition> string_to_ad_position(const std::string &str)
    {
        if (str == "Above Fold" || str == "1")
            return AdPosition::ABOVE_FOLD;
        if (str == "Below Fold" || str == "3")
            return AdPosition::BELOW_FOLD;
        if (str == "Header" || str == "4")
            return AdPosition::HEADER;
        if (str == "Footer" || str == "5")
            return AdPosition::FOOTER;
        if (str == "Sidebar" || str == "6")
            return AdPosition::SIDEBAR;
        if (str == "Full Screen" || str == "7")
            return AdPosition::FULL_SCREEN;
        return std::nullopt;
    }

    inline std::optional<AuctionType> string_to_auction_type(const std::string &str)
    {
        if (str == "First Price" || str == "1")
            return AuctionType::FIRST_PRICE;
        if (str == "Second Price" || str == "2")
            return AuctionType::SECOND_PRICE;
        return std::nullopt;
    }

    inline std::optional<Gender> string_to_gender(const std::string &str)
    {
        if (str == "M" || str == "Male")
            return Gender::MALE;
        if (str == "F" || str == "Female")
            return Gender::FEMALE;
        if (str == "O" || str == "Unknown")
            return Gender::UNKNOWN;
        return std::nullopt;
    }

} // namespace flow_ad::openrtb
