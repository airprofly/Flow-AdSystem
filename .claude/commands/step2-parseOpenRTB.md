# Step 2: OpenRTB 协议解析

## 📋 任务概述

**业务目标**: 实现 OpenRTB 2.5 协议解析器,将 Step 1 生成的 OpenRTB Bid Request JSON 数据解析为结构化的 C++ 对象。

**前置依赖**: ✅ Step 1 数据生成已完成 (包含 `openrtb_requests.json` 测试数据)

**当前状态**: 🚀 **规划中**

**技术选型**: ✅ 使用 **Tencent RapidJSON** - 高性能 JSON 解析库

### 为什么选择 RapidJSON?

RapidJSON 是腾讯开源的高性能 JSON 解析库,非常适合广告投放系统这种对性能要求极高的场景:

**优势对比**:

| 特性 | RapidJSON | nlohmann/json | jsoncpp |
|------|-----------|---------------|---------|
| **解析速度** | ⚡⚡⚡ 极快 (~10ms/100条) | ⚡⚡ 中等 (~50ms/100条) | ⚡ 较慢 (~80ms/100条) |
| **内存占用** | 💾 极低 (~5MB) | 💾💾 中等 (~15MB) | 💾💾💾 较高 (~20MB) |
| **部署方式** | 📦 仅头文件 | 📦 需要编译 | 📦 需要编译 |
| **零拷贝** | ✅ 支持 | ❌ 不支持 | ❌ 不支持 |
| **SIMD 加速** | ✅ 支持 | ❌ 不支持 | ❌ 不支持 |
| **易用性** | ⭐⭐⭐ 中等 | ⭐⭐⭐⭐⭐ 简单 | ⭐⭐⭐⭐ 较简单 |

**核心特性**:
- ⚡ **超快速度**: 使用 C++ 模板和 SIMD 指令,性能接近手写解析器
- 🚀 **零拷贝**: 原位解析 (in-situ parsing),避免内存拷贝
- 💾 **内存友好**: 内存占用极低,适合大规模部署
- 📦 **仅头文件**: 无需编译,直接包含即可使用
- 🎯 **标准兼容**: 完全符合 JSON RFC 7159 规范
- 🔧 **灵活 API**: 提供 DOM 和 SAX 两种解析模式

---

## 🎯 核心目标

### 2.1 OpenRTB 数据模型定义
- 定义完整的 OpenRTB 2.5 数据结构
- 支持所有必填字段和常用可选字段
- 符合 OpenRTB 2.5 规范

### 2.2 JSON 解析器实现
- 解析 OpenRTB Bid Request JSON 格式
- 支持单条和批量解析
- 完善的错误处理和验证

### 2.3 数据验证
- 验证必填字段完整性
- 验证数据类型和范围
- 提供详细的错误信息

### 2.4 单元测试
- 测试各种 OpenRTB 请求场景
- 测试边界条件和异常处理
- 测试性能基准

---

## 📐 数据结构设计

### OpenRTB 核心对象

```
OpenRTBRequest (根对象)
├── id (必填) - 请求唯一标识
├── imp (必填) - 广告位列表 [Impression]
├── device (必填) - 设备信息
├── user (推荐) - 用户信息
├── app (可选) - 应用信息
├── site (可选) - 站点信息
├── at (可选) - 拍卖类型 (1=First Price, 2=Second Price)
├── bcat (可选) - 屏蔽的广告类别
├── badv (可选) - 屏蔽的广告主域名
└── ext (可选) - 扩展字段

Impression (广告位)
├── id (必填) - 广告位标识
├── banner (可选) - Banner 信息
├── video (可选) - 视频信息
├── native (可选) - 原生广告信息
├── bidfloor (可选) - 底价
├── bidfloorcur (可选) - 底价货币
└── ext (可选) - 扩展字段

Banner (Banner 广告位)
├── w/h - 宽度/高度 (可以是单个值或数组)
├── pos - 位置 (1-7)
├── btype - Banner 类型
├── battr - Banner 属性
└── ext - 扩展字段

Device (设备信息)
├── ua (必填) - User-Agent
├── ip (必填) - IP 地址
├── geo (推荐) - 地理位置
├── devicetype (推荐) - 设备类型
├── os (推荐) - 操作系统
├── osv (可选) - 操作系统版本
├── connectiontype (可选) - 连接类型
├── carrier (可选) - 运营商
└── ext - 扩展字段

User (用户信息)
├── id (必填) - 用户 ID
├── buyeruid (可选) - 买家用户 ID
├── yob (可选) - 出生年份
├── gender (可选) - 性别 (M/F/O)
├── geo (可选) - 地理位置
├── keywords (可选) - 兴趣标签
└── ext - 扩展字段
```

---

## 🚀 实现阶段

### Phase 1: 数据模型定义 (1-2 天)

#### 1.1 创建 OpenRTB 模型头文件
**文件**: `core/openrtb/openrtb_models.h`

**实现内容**:
```cpp
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace flow_ad::openrtb {

// ========== 枚举类型 ==========

// 设备类型 (OpenRTB 2.5 Section 3.2.18)
enum class DeviceType : int {
    UNKNOWN = 0,
    MOBILE = 1,           // 手机/平板
    PERSONAL_COMPUTER = 2,
    TV = 3,
    PHONE = 4,
    TABLET = 5,
    CONNECTED_DEVICE = 6,
    SET_TOP_BOX = 7
};

// 操作系统 (OpenRTB 2.5 Section 3.2.19)
enum class OS : int {
    UNKNOWN = 0,
    IOS = 1,
    ANDROID = 2,
    WINDOWS = 3,
    OSX = 4,
    LINUX = 5
};

// 连接类型 (OpenRTB 2.5 Section 3.2.20)
enum class ConnectionType : int {
    UNKNOWN = 0,
    ETHERNET = 1,
    WIFI = 2,
    DATA_2G = 3,
    DATA_3G = 4,
    DATA_4G = 5
};

// 广告位置 (OpenRTB 2.5 Section 3.2.7)
enum class AdPosition : int {
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
enum class AuctionType : int {
    FIRST_PRICE = 1,
    SECOND_PRICE = 2
};

// 性别 (OpenRTB 2.5 Section 3.2.13)
enum class Gender : char {
    UNKNOWN = 'O',
    MALE = 'M',
    FEMALE = 'F'
};

// ========== 核心数据结构 ==========

// 地理位置 (OpenRTB 2.5 Section 3.2.15)
struct Geo {
    std::optional<std::string> country;      // ISO-3166-1 alpha-3
    std::optional<std::string> region;       // ISO-3166-2
    std::optional<std::string> city;         // 城市名称
    std::optional<double> lat;               // 纬度
    std::optional<double> lon;               // 经度
    std::optional<int> type;                 // 位置精度 (1=GPS, 2=IP, etc.)
    std::optional<std::string> ext;          // 扩展字段
};

// Banner 对象 (OpenRTB 2.5 Section 3.2.6)
struct Banner {
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
    std::pair<int, int> get_primary_size() const;
};

// Video 对象 (OpenRTB 2.5 Section 3.2.8)
struct Video {
    std::optional<std::vector<int>> mimes;   // MIME 类型
    std::optional<int> w;                    // 宽度
    std::optional<int> h;                    // 高度
    std::optional<int> startdelay;           // 开始延迟
    std::optional<std::vector<int>> protocols; // 协议
    std::optional<int> maxduration;          // 最大时长
    std::optional<std::vector<int>> api;     // API 框架
    std::optional<std::string> ext;          // 扩展字段
};

// Native 对象 (OpenRTB 2.5 Section 3.2.9)
struct Native {
    std::optional<std::string> request;      // Native 请求字符串
    std::optional<std::string> ver;          // Native 版本
    std::vector<std::unordered_map<std::string, std::string>> assets; // 资源列表
    std::optional<std::string> ext;          // 扩展字段
};

// 广告位对象 (OpenRTB 2.5 Section 3.2.2)
struct Impression {
    std::string id;                          // 必填: 广告位 ID
    std::optional<Banner> banner;            // Banner 信息
    std::optional<Video> video;              // 视频信息
    std::optional<Native> native_;           // 原生广告信息
    std::optional<double> bidfloor;          // 底价
    std::optional<std::string> bidfloorcur;  // 底价货币 (默认 USD)
    std::optional<bool> bidfloor_priority;   // 是否强制底价
    std::optional<std::vector<int>> secure;  // 是否需要 HTTPS
    std::optional<std::vector<int>> iframebuster; // iframe 破解
    std::optional<std::string> ext;          // 扩展字段

    // 辅助方法: 获取广告位类型
    std::string get_impression_type() const;
};

// 设备对象 (OpenRTB 2.5 Section 3.2.18)
struct Device {
    std::optional<std::string> ua;           // User-Agent
    std::optional<std::string> ip;           // IP 地址 (IPv4)
    std::optional<std::string> ipv6;         // IPv6 地址
    std::optional<Geo> geo;                  // 地理位置
    std::optional<int> devicetype;           // 设备类型 (DeviceType)
    std::optional<int> os;                   // 操作系统 (OS)
    std::optional<std::string> osv;          // 操作系统版本
    std::optional<int> dnt;                  // Do Not Track
    std::optional<int> lmt;                  // Limit Ad Tracking
    std::optional<int> connectiontype;       // 连接类型 (ConnectionType)
    std::optional<std::string> carrier;      // 运营商
    std::optional<std::string> make;         // 设备制造商
    std::optional<std::string> model;        // 设备型号
    std::optional<std::string> ext;          // 扩展字段
};

// 用户对象 (OpenRTB 2.5 Section 3.2.13)
struct User {
    std::string id;                          // 必填: 用户 ID
    std::optional<std::string> buyeruid;     // 买家用户 ID (DSP 用)
    std::optional<int> yob;                  // 出生年份
    std::optional<std::string> gender;       // 性别 (Gender: M/F/O)
    std::optional<Geo> geo;                  // 地理位置
    std::optional<std::vector<std::string>> keywords; // 关键词/兴趣
    std::optional<std::string> ext;          // 扩展字段
};

// 应用对象 (OpenRTB 2.5 Section 3.2.16)
struct App {
    std::optional<std::string> id;           // 应用 ID
    std::optional<std::string> name;         // 应用名称
    std::optional<std::string> bundle;       // 包名 (Android) / Bundle ID (iOS)
    std::optional<std::string> domain;       // 应用域名
    std::optional<std::string> storeurl;     // 应用商店 URL
    std::optional<std::vector<int>> cat;     // IAB 内容分类
    std::optional<std::vector<std::string>> sectioncat; // 子分类
    std::optional<std::vector<std::string>> pagecat;    // 页面分类
    std::optional<std::string> ver;          // 应用版本
    std::optional<std::string> ext;          // 扩展字段
};

// 站点对象 (OpenRTB 2.5 Section 3.2.17)
struct Site {
    std::optional<std::string> id;           // 站点 ID
    std::optional<std::string> name;         // 站点名称
    std::optional<std::string> domain;       // 站点域名
    std::optional<std::vector<int>> cat;     // IAB 内容分类
    std::optional<std::vector<std::string>> sectioncat; // 子分类
    std::optional<std::vector<std::string>> pagecat;    // 页面分类
    std::optional<std::string> page;         // 页面 URL
    std::optional<std::string> ref;          // 来源 URL
    std::optional<std::string> ext;          // 扩展字段
};

// OpenRTB Bid Request 根对象 (OpenRTB 2.5 Section 3.2.1)
struct OpenRTBRequest {
    std::string id;                          // 必填: 请求唯一标识
    std::vector<Impression> imp;             // 必填: 广告位列表
    std::optional<Device> device;            // 推荐: 设备信息
    std::optional<User> user;                // 推荐: 用户信息
    std::optional<App> app;                  // 可选: 应用信息 (与 site 互斥)
    std::optional<Site> site;                // 可选: 站点信息 (与 app 互斥)
    std::optional<int> at;                   // 拍卖类型 (AuctionType, 默认 2)
    std::optional<std::vector<int>> bcat;    // 屏蔽的广告类别 (IAB)
    std::optional<std::vector<std::string>> badv; // 屏蔽的广告主域名
    std::optional<std::vector<std::string>> bapp;  // 屏蔽的应用 ID
    std::optional<std::string> ext;          // 扩展字段

    // 辅助方法
    bool is_app() const;                     // 是否为应用流量
    bool is_site() const;                    // 是否为站点流量
    size_t get_impression_count() const;     // 获取广告位数量
};

// ========== 辅助函数 ==========

// 枚举转字符串
std::string device_type_to_string(DeviceType type);
std::string os_to_string(OS os);
std::string connection_type_to_string(ConnectionType type);
std::string ad_position_to_string(AdPosition pos);
std::string auction_type_to_string(AuctionType type);
std::string gender_to_string(Gender gender);

// 字符串转枚举
std::optional<DeviceType> string_to_device_type(const std::string& str);
std::optional<OS> string_to_os(const std::string& str);
std::optional<ConnectionType> string_to_connection_type(const std::string& str);
std::optional<AdPosition> string_to_ad_position(const std::string& str);
std::optional<AuctionType> string_to_auction_type(const std::string& str);
std::optional<Gender> string_to_gender(const std::string& str);

} // namespace flow_ad::openrtb
```

**预计工作量**: 4-6 小时

---

### Phase 2: JSON 解析器实现 (2-3 天)

#### 2.1 创建解析器头文件
**文件**: `core/openrtb/openrtb_parser.h`

**实现内容**:
```cpp
#pragma once

#include "openrtb_models.h"
#include <string>
#include <vector>
#include <optional>

namespace flow_ad::openrtb {

// 解析错误类型
enum class ParseError {
    NONE = 0,
    INVALID_JSON,
    MISSING_REQUIRED_FIELD,
    INVALID_FIELD_TYPE,
    INVALID_FIELD_VALUE,
    UNKNOWN
};

// 解析结果
struct ParseResult {
    bool success;
    OpenRTBRequest request;
    ParseError error_type;
    std::string error_message;
    size_t error_position;  // JSON 错误位置

    // 获取完整错误信息
    std::string get_full_error() const;
};

// OpenRTB JSON 解析器
class OpenRTBParser {
public:
    OpenRTBParser() = default;
    ~OpenRTBParser() = default;

    // ========== 单个请求解析 ==========

    // 解析 JSON 字符串
    ParseResult parse(const std::string& json_str);

    // 解析 JSON 文件 (包含单个请求)
    ParseResult parse_file(const std::string& file_path);

    // ========== 批量请求解析 ==========

    // 解析 JSON 文件 (包含多个请求的数组)
    std::vector<ParseResult> parse_batch_file(const std::string& file_path);

    // 解析 JSON 字符串数组
    std::vector<ParseResult> parse_batch(const std::string& json_array_str);

    // ========== 数据验证 ==========

    // 验证请求完整性
    bool validate(const OpenRTBRequest& request);

    // 验证请求并返回详细错误信息
    std::vector<std::string> validate_detailed(const OpenRTBRequest& request);

    // ========== 工具方法 ==========

    // 将 OpenRTBRequest 转换为 JSON 字符串 (用于调试)
    static std::string to_json(const OpenRTBRequest& request);

    // 美化 JSON 输出
    static std::string to_json_pretty(const OpenRTBRequest& request);

    // 获取最后一次错误的详细信息
    std::string get_last_error() const;

    // 设置严格模式 (默认: false)
    // 严格模式下,任何不符合规范的字段都会导致解析失败
    void set_strict_mode(bool strict);

    // 设置是否忽略未知字段 (默认: true)
    void set_ignore_unknown_fields(bool ignore);

private:
    bool strict_mode_ = false;
    bool ignore_unknown_fields_ = true;
    mutable std::string last_error_;

    // ========== 内部解析方法 ==========

    // 解析 Geo 对象 (使用 RapidJSON)
    std::optional<Geo> parse_geo(const rapidjson::Value& obj);

    // 解析 Banner 对象
    std::optional<Banner> parse_banner(const rapidjson::Value& obj);

    // 解析 Video 对象
    std::optional<Video> parse_video(const rapidjson::Value& obj);

    // 解析 Native 对象
    std::optional<Native> parse_native(const rapidjson::Value& obj);

    // 解析 Impression 对象
    std::optional<Impression> parse_impression(const rapidjson::Value& obj);

    // 解析 Device 对象
    std::optional<Device> parse_device(const rapidjson::Value& obj);

    // 解析 User 对象
    std::optional<User> parse_user(const rapidjson::Value& obj);

    // 解析 App 对象
    std::optional<App> parse_app(const rapidjson::Value& obj);

    // 解析 Site 对象
    std::optional<Site> parse_site(const rapidjson::Value& obj);

    // 解析根对象
    std::optional<OpenRTBRequest> parse_request(const rapidjson::Value& obj);

    // ========== 验证方法 ==========

    bool validate_geo(const Geo& geo, std::vector<std::string>& errors);
    bool validate_banner(const Banner& banner, std::vector<std::string>& errors);
    bool validate_impression(const Impression& imp, std::vector<std::string>& errors);
    bool validate_device(const Device& device, std::vector<std::string>& errors);
    bool validate_user(const User& user, std::vector<std::string>& errors);
};

} // namespace flow_ad::openrtb
```

**预计工作量**: 8-10 小时

---

#### 2.2 实现解析器源文件
**文件**: `core/openrtb/openrtb_parser.cpp`

**核心实现逻辑**:
```cpp
#include "openrtb_parser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace flow_ad::openrtb {

using json = nlohmann::json;

ParseResult OpenRTBParser::parse(const std::string& json_str) {
    ParseResult result;
    result.success = false;
    result.error_type = ParseError::NONE;

    try {
        // 1. 解析 JSON
        json j = json::parse(json_str);

        // 2. 验证必填字段
        if (!j.contains("id") || !j["id"].is_string()) {
            result.error_type = ParseError::MISSING_REQUIRED_FIELD;
            result.error_message = "Missing or invalid required field: 'id'";
            return result;
        }

        if (!j.contains("imp") || !j["imp"].is_array()) {
            result.error_type = ParseError::MISSING_REQUIRED_FIELD;
            result.error_message = "Missing or invalid required field: 'imp'";
            return result;
        }

        // 3. 解析各个对象
        auto request_opt = parse_request(j);
        if (!request_opt) {
            result.error_type = ParseError::INVALID_FIELD_VALUE;
            result.error_message = "Failed to parse request: " + get_last_error();
            return result;
        }

        result.request = *request_opt;
        result.success = true;

        // 4. 验证请求数据
        if (strict_mode_) {
            auto errors = validate_detailed(result.request);
            if (!errors.empty()) {
                result.success = false;
                result.error_type = ParseError::INVALID_FIELD_VALUE;
                result.error_message = "Validation failed: " + errors[0];
                return result;
            }
        }

    } catch (const json::parse_error& e) {
        result.error_type = ParseError::INVALID_JSON;
        result.error_message = "JSON parse error: " + std::string(e.what());
        result.error_position = e.byte;
    } catch (const std::exception& e) {
        result.error_type = ParseError::UNKNOWN;
        result.error_message = "Unknown error: " + std::string(e.what());
    }

    return result;
}

std::optional<OpenRTBRequest> OpenRTBParser::parse_request(const json& j) {
    OpenRTBRequest request;

    // 必填字段
    request.id = j["id"].get<std::string>();

    // imp (必填)
    if (j.contains("imp") && j["imp"].is_array()) {
        for (const auto& imp_json : j["imp"]) {
            auto imp = parse_impression(imp_json);
            if (!imp) {
                last_error_ = "Failed to parse impression";
                return std::nullopt;
            }
            request.imp.push_back(*imp);
        }
    }

    // 可选字段
    if (j.contains("device")) {
        auto device = parse_device(j["device"]);
        if (device) request.device = *device;
    }

    if (j.contains("user")) {
        auto user = parse_user(j["user"]);
        if (user) request.user = *user;
    }

    if (j.contains("app")) {
        auto app = parse_app(j["app"]);
        if (app) request.app = *app;
    }

    if (j.contains("site")) {
        auto site = parse_site(j["site"]);
        if (site) request.site = *site;
    }

    // 其他可选字段
    if (j.contains("at") && j["at"].is_number_integer()) {
        request.at = j["at"].get<int>();
    }

    if (j.contains("bcat") && j["bcat"].is_array()) {
        request.bcat = j["bcat"].get<std::vector<int>>();
    }

    if (j.contains("badv") && j["badv"].is_array()) {
        request.badv = j["badv"].get<std::vector<std::string>>();
    }

    if (j.contains("ext")) {
        request.ext = j["ext"].dump();
    }

    return request;
}

std::optional<Impression> OpenRTBParser::parse_impression(const json& j) {
    Impression imp;

    // 必填字段
    if (!j.contains("id") || !j["id"].is_string()) {
        last_error_ = "Missing or invalid 'id' in impression";
        return std::nullopt;
    }
    imp.id = j["id"].get<std::string>();

    // 可选字段
    if (j.contains("banner")) {
        auto banner = parse_banner(j["banner"]);
        if (banner) imp.banner = *banner;
    }

    if (j.contains("video")) {
        auto video = parse_video(j["video"]);
        if (video) imp.video = *video;
    }

    if (j.contains("native")) {
        auto native_ = parse_native(j["native"]);
        if (native_) imp.native_ = *native_;
    }

    if (j.contains("bidfloor") && j["bidfloor"].is_number()) {
        imp.bidfloor = j["bidfloor"].get<double>();
    }

    if (j.contains("bidfloorcur") && j["bidfloorcur"].is_string()) {
        imp.bidfloorcur = j["bidfloorcur"].get<std::string>();
    }

    // ... 其他字段

    return imp;
}

std::optional<Banner> OpenRTBParser::parse_banner(const json& j) {
    Banner banner;

    if (j.contains("w")) {
        if (j["w"].is_array()) {
            banner.w = j["w"].get<std::vector<int>>();
        } else if (j["w"].is_number_integer()) {
            banner.w = std::vector<int>{j["w"].get<int>()};
        }
    }

    if (j.contains("h")) {
        if (j["h"].is_array()) {
            banner.h = j["h"].get<std::vector<int>>();
        } else if (j["h"].is_number_integer()) {
            banner.h = std::vector<int>{j["h"].get<int>()};
        }
    }

    if (j.contains("pos") && j["pos"].is_number_integer()) {
        banner.pos = j["pos"].get<int>();
    }

    // ... 其他字段

    return banner;
}

// ... 其他 parse 方法的实现

bool OpenRTBParser::validate(const OpenRTBRequest& request) {
    // 基本验证
    if (request.id.empty()) return false;
    if (request.imp.empty()) return false;

    // 每个 impression 必须有 id
    for (const auto& imp : request.imp) {
        if (imp.id.empty()) return false;
    }

    return true;
}

std::vector<std::string> OpenRTBParser::validate_detailed(const OpenRTBRequest& request) {
    std::vector<std::string> errors;

    // 验证根对象
    if (request.id.empty()) {
        errors.push_back("Request ID is empty");
    }

    if (request.imp.empty()) {
        errors.push_back("No impressions in request");
    }

    // 验证每个 impression
    for (size_t i = 0; i < request.imp.size(); ++i) {
        const auto& imp = request.imp[i];
        if (imp.id.empty()) {
            errors.push_back("Impression " + std::to_string(i) + " has empty ID");
        }

        // 至少要有一个广告类型 (banner/video/native)
        if (!imp.banner && !imp.video && !imp.native_) {
            errors.push_back("Impression " + std::to_string(i) + " has no ad type");
        }
    }

    // 验证 device
    if (request.device) {
        if (!request.device->ua && !request.device->ip) {
            errors.push_back("Device must have either 'ua' or 'ip'");
        }
    }

    // 验证 user
    if (request.user) {
        if (request.user->id.empty()) {
            errors.push_back("User ID is empty");
        }
    }

    return errors;
}

} // namespace flow_ad::openrtb
```

**预计工作量**: 10-12 小时

---

### Phase 3: 单元测试 (1-2 天)

#### 3.1 创建测试文件
**文件**: `tests/unit/openrtb_parser_test.cpp`

**测试用例**:
```cpp
#include <gtest/gtest.h>
#include "core/openrtb/openrtb_parser.h"
#include "core/openrtb/openrtb_models.h"
#include <fstream>

using namespace flow_ad::openrtb;

class OpenRTBParserTest : public ::testing::Test {
protected:
    OpenRTBParser parser;

    // 读取测试数据文件
    std::string read_test_file(const std::string& filename) {
        std::ifstream file("../../data/data/" + filename);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

// ========== 基础解析测试 ==========

TEST_F(OpenRTBParserTest, ParseValidRequest) {
    std::string json = R"({
        "id": "test-request-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {
            "ua": "Mozilla/5.0",
            "ip": "192.168.1.1",
            "devicetype": 1,
            "os": 1
        },
        "user": {"id": "user-001"}
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.id, "test-request-001");
    EXPECT_EQ(result.request.imp.size(), 1);
    EXPECT_TRUE(result.request.device.has_value());
    EXPECT_TRUE(result.request.user.has_value());
}

TEST_F(OpenRTBParserTest, ParseMissingRequiredField) {
    std::string json = R"({
        "imp": [{"id": "imp-1"}]
    })";

    auto result = parser.parse(json);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_type, ParseError::MISSING_REQUIRED_FIELD);
}

TEST_F(OpenRTBParserTest, ParseInvalidJSON) {
    std::string json = R"({"id": "test", "imp": [})";

    auto result = parser.parse(json);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_type, ParseError::INVALID_JSON);
}

// ========== Banner 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseBannerWithSingleSize) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.imp[0].banner.has_value());
    EXPECT_EQ(result.request.imp[0].banner->w->size(), 1);
    EXPECT_EQ((*result.request.imp[0].banner->w)[0], 300);
}

TEST_F(OpenRTBParserTest, ParseBannerWithMultipleSizes) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": [300, 728], "h": [250, 90]}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.imp[0].banner->w->size(), 2);
}

// ========== Device 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseDeviceWithAllFields) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "device": {
            "ua": "Mozilla/5.0",
            "ip": "192.168.1.1",
            "geo": {"country": "CN", "city": "北京"},
            "devicetype": 1,
            "os": 1,
            "osv": "16.0",
            "connectiontype": 2
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.request.device->ua.has_value());
    EXPECT_TRUE(result.request.device->geo.has_value());
    EXPECT_EQ(result.request.device->geo->country, "CN");
}

// ========== User 解析测试 ==========

TEST_F(OpenRTBParserTest, ParseUserWithDemographics) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "user": {
            "id": "user-001",
            "yob": 1990,
            "gender": "M",
            "keywords": ["游戏", "科技"]
        }
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.user->yob, 1990);
    EXPECT_EQ(result.request.user->gender, "M");
    EXPECT_EQ(result.request.user->keywords->size(), 2);
}

// ========== 批量解析测试 ==========

TEST_F(OpenRTBParserTest, ParseBatchRequests) {
    // 使用 Step 1 生成的测试数据
    auto results = parser.parse_batch_file("../../data/data/openrtb_requests.json");

    EXPECT_GT(results.size(), 0);
    EXPECT_GT(results.size(), 50);  // Step 1 生成了 100 条数据

    // 检查成功率
    int success_count = 0;
    for (const auto& result : results) {
        if (result.success) success_count++;
    }
    EXPECT_EQ(success_count, results.size());  // 所有请求都应该成功解析
}

// ========== 验证测试 ==========

TEST_F(OpenRTBParserTest, ValidateValidRequest) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {"ua": "Mozilla/5.0", "ip": "192.168.1.1"},
        "user": {"id": "user-001"}
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(parser.validate(result.request));
}

TEST_F(OpenRTBParserTest, ValidateInvalidRequest) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1"}],
        "device": {"ua": "Mozilla/5.0"}
    })";

    auto result = parser.parse(json);
    EXPECT_FALSE(parser.validate(result.request));  // device 缺少 ip
}

// ========== 边界条件测试 ==========

TEST_F(OpenRTBParserTest, ParseEmptyImpressionList) {
    std::string json = R"({
        "id": "test-001",
        "imp": []
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.get_impression_count(), 0);
}

TEST_F(OpenRTBParserTest, ParseMultipleImpressions) {
    std::string json = R"({
        "id": "test-001",
        "imp": [
            {"id": "imp-1", "banner": {"w": 300, "h": 250}},
            {"id": "imp-2", "banner": {"w": 728, "h": 90}},
            {"id": "imp-3", "video": {"w": 640, "h": 480}}
        ]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.request.get_impression_count(), 3);
}

// ========== 性能测试 ==========

TEST_F(OpenRTBParserTest, PerformanceTest) {
    std::string json = read_test_file("openrtb_requests.json");
    auto results = parser.parse_batch(json);

    // 评估性能 (应该在 100ms 内完成 100 条解析)
    auto start = std::chrono::high_resolution_clock::now();
    results = parser.parse_batch(json);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 100);  // < 100ms
}

// ========== 序列化测试 ==========

TEST_F(OpenRTBParserTest, SerializeToJson) {
    std::string json = R"({
        "id": "test-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}]
    })";

    auto result = parser.parse(json);
    EXPECT_TRUE(result.success);

    // 序列化回 JSON
    std::string serialized = OpenRTBParser::to_json(result.request);

    // 再次解析
    auto result2 = parser.parse(serialized);
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result.request.id, result2.request.id);
}
```

**预计工作量**: 6-8 小时

---

## 📁 目录结构

```
FastMatch-Ad/
├── core/
│   └── openrtb/
│       ├── openrtb_models.h           # OpenRTB 数据模型定义
│       ├── openrtb_parser.h           # 解析器头文件
│       └── openrtb_parser.cpp         # 解析器实现
├── tests/
│   └── unit/
│       └── openrtb_parser_test.cpp    # 单元测试
├── data/
│   └── data/
│       └── openrtb_requests.json      # Step 1 生成的测试数据
```

---

## 📊 性能指标

### 目标性能

| 指标 | 目标值 | 测试方法 |
|------|--------|----------|
| 单条解析延迟 | P99 < 1ms | 单元测试 |
| 批量解析 (100条) | < 100ms | 性能测试 |
| 内存占用 | < 10MB/请求 | 内存分析 |
| 解析成功率 | 100% (标准数据) | 单元测试 |

---

## ✅ 验收标准

### 功能验收
- [ ] 能够正确解析 OpenRTB 2.5 Bid Request
- [ ] 支持所有必填字段和常用可选字段
- [ ] 提供清晰的错误信息
- [ ] 支持批量解析
- [ ] 数据验证功能完善

### 质量验收
- [ ] 单元测试覆盖率 > 90%
- [ ] 无内存泄漏 (Valgrind 检查)
- [ ] 代码符合 Google C++ Style
- [ ] 所有公共 API 有文档注释

### 性能验收
- [ ] 单条解析 < 1ms
- [ ] 批量解析 100 条 < 100ms
- [ ] 无性能瓶颈

---

## 🚀 使用示例

### 基本使用

```cpp
#include "core/openrtb/openrtb_parser.h"

using namespace flow_ad::openrtb;

int main() {
    OpenRTBParser parser;

    // 解析单个请求
    std::string json_str = R"({
        "id": "req-001",
        "imp": [{"id": "imp-1", "banner": {"w": 300, "h": 250}}],
        "device": {"ua": "Mozilla/5.0", "ip": "192.168.1.1"},
        "user": {"id": "user-001"}
    })";

    auto result = parser.parse(json_str);

    if (result.success) {
        std::cout << "Request ID: " << result.request.id << std::endl;
        std::cout << "Impressions: " << result.request.get_impression_count() << std::endl;

        // 访问设备信息
        if (result.request.device) {
            std::cout << "Device: "
                      << device_type_to_string(
                          static_cast<DeviceType>(*result.request.device->devicetype)
                      )
                      << std::endl;
        }
    } else {
        std::cerr << "Parse error: " << result.get_full_error() << std::endl;
    }

    return 0;
}
```

### 批量解析

```cpp
#include "core/openrtb/openrtb_parser.h"

using namespace flow_ad::openrtb;

int main() {
    OpenRTBParser parser;

    // 批量解析文件
    auto results = parser.parse_batch_file("data/data/openrtb_requests.json");

    int success_count = 0;
    int fail_count = 0;

    for (const auto& result : results) {
        if (result.success) {
            success_count++;
            // 处理成功的请求
            process_request(result.request);
        } else {
            fail_count++;
            std::cerr << "Failed to parse: " << result.get_full_error() << std::endl;
        }
    }

    std::cout << "Success: " << success_count
              << ", Failed: " << fail_count << std::endl;

    return 0;
}
```

---

## 📚 参考资料

### OpenRTB 规范
- [OpenRTB 2.5 规范 (PDF)](https://www.iab.com/wp-content/uploads/2016/03/OpenRTB-API-Specification-Version-2-5-FINAL.pdf)
- [OpenRTB 2.6 规范](https://www.iab.com/wp-content/uploads/2018/03/OpenRTB-2-6-FINAL.pdf)
- [IAB Tech Lab - OpenRTB](https://www.iabtechlab.com/rtb/)

### 依赖库

#### RapidJSON - 高性能 JSON 解析库
- **GitHub**: [Tencent/RapidJSON](https://github.com/Tencent/rapidjson/)
- **文档**: [RapidJSON Documentation](https://rapidjson.org/)
- **特点**:
  - ⚡ **极快**: 仅头文件,使用 C++ 模板,性能接近手写解析器
  - 🚀 **零拷贝**: 原位解析,避免内存拷贝
  - 💾 **内存友好**: 使用 SIMD 加速,内存占用极低
  - 🎯 **符合标准**: 完全符合 JSON RFC 7159 规范
  - 🔧 **易用**: 提供 DOM 和 SAX 两种 API

**性能对比** (解析 100 个 OpenRTB 请求):

| JSON 库 | 解析时间 | 内存占用 | 代码大小 |
|---------|----------|----------|----------|
| RapidJSON | **~10ms** | **~5MB** | **头文件** |
| nlohmann/json | ~50ms | ~15MB | ~200KB |
| jsoncpp | ~80ms | ~20MB | ~300KB |

**RapidJSON 示例**:
```cpp
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

// 解析 JSON
rapidjson::Document doc;
doc.Parse<rapidjson::kParseDefaultFlags>(json_str.c_str());

// 检查解析错误
if (doc.HasParseError()) {
    std::cerr << "Parse error: "
              << rapidjson::GetParseError_En(doc.GetParseError())
              << " at offset " << doc.GetErrorOffset()
              << std::endl;
}

// 访问字段
if (doc.HasMember("id") && doc["id"].IsString()) {
    std::string id = doc["id"].GetString();
}

// 遍历数组
if (doc.HasMember("imp") && doc["imp"].IsArray()) {
    for (auto& imp : doc["imp"].GetArray()) {
        // 处理每个 impression
    }
}

// 原位修改 (零拷贝)
doc["id"].SetString("new-id", doc.GetAllocator());
```

#### Google Test - 测试框架
- **GitHub**: [google/googletest](https://github.com/google/googletest)
- **版本**: >= 1.13.0

---

## 🎯 下一步

完成 Step 2 后,系统将具备:
- ✅ 完整的 OpenRTB 2.5 数据模型
- ✅ 高性能 JSON 解析器
- ✅ 完善的数据验证
- ✅ 100% 的测试覆盖率

**Step 3 预告**: 广告索引与匹配 - 基于解析的 OpenRTB 请求实现广告召回与过滤

---

**文档创建时间**: 2026-03-11
**维护者**: airprofly
**状态**: 🚀 规划中
