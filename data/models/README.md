<div align="center">

# 📊 数据模型层
### Data Models for FastMatch-Ad

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20) [![Status: Stable](https://img.shields.io/badge/Status-Stable-success.svg)]()

OpenRTB 协议 · 类型安全 · 零依赖

**✅ 稳定可用** - 定义系统核心数据结构

</div>

---

## 📖 模块简介

`models` 模块定义了 FastMatch-Ad 广告投放系统的**核心数据结构**。所有数据结构严格遵循 **OpenRTB 2.5/2.6 协议规范**，提供类型安全的 C++ 实现。

### 设计原则

- ✅ **类型安全** - 使用强类型枚举避免魔法数字
- ✅ **零依赖** - 仅依赖 STL，无外部库依赖
- ✅ **轻量级** - 所有类型均为 POD 或简单结构体
- ✅ **OpenRTB 兼容** - 数据结构与 OpenRTB 协议对应
- ✅ **完整数据链** - 支持广告主→计划→创意→广告的完整数据链

---

## 📋 数据模型

### 枚举类型

#### `AdCategory` - 广告分类

```cpp
enum class AdCategory {
    ECOMMERCE = 0,      // 电商
    GAMING = 1,         // 游戏
    FINANCE = 2,        // 金融
    EDUCATION = 3,      // 教育
    HEALTH = 4,         // 健康
    AUTOMOTIVE = 5,     // 汽车
    TRAVEL = 6,         // 旅游
    ENTERTAINMENT = 7,  // 娱乐
    TECHNOLOGY = 8,     // 科技
    FOOD = 9            // 美食
};
```

#### `DeviceType` - 设备类型

```cpp
enum class DeviceType {
    MOBILE = 0,         // 移动端
    DESKTOP = 1,        // 桌面端
    TABLET = 2          // 平板
};
```

#### `AdStatus` - 广告状态

```cpp
enum class AdStatus {
    PENDING = 0,        // 待审核
    ACTIVE = 1,         // 活跃
    PAUSED = 2,         // 暂停
    ENDED = 3,          // 已结束
    DELETED = 4         // 已删除
};
```

---

### 数据结构

#### `Advertiser` - 广告主

```cpp
struct Advertiser {
    uint64_t id;              // 广告主 ID
    std::string name;         // 广告主名称
    std::string category;     // 行业分类 (ecommerce, gaming, etc.)
};
```

**说明**:
- `id` - 全局唯一标识符
- `name` - 广告主公司名称
- `category` - 行业分类，使用小写字符串

**ID 范围**: 10000 - 19999

**与 OpenRTB 映射**:
- OpenRTB Bid Response 中的 `adid` 可映射到此 `id`
- OpenRTB 中的 `adomain` (广告主域名) 可用于标识广告主

#### `AdCampaign` - 广告计划

```cpp
struct AdCampaign {
    uint64_t id;              // 计划 ID
    uint64_t advertiser_id;   // 所属广告主 ID
    std::string name;         // 计划名称
    double budget;            // 总预算
    double spent;             // 已消费金额
    double bid_price;         // 出价
    AdStatus status;          // 状态
    std::chrono::system_clock::time_point start_time;  // 开始时间
    std::chrono::system_clock::time_point end_time;    // 结束时间
};
```

**说明**:
- `id` - 全局唯一标识符
- `advertiser_id` - 关联的广告主 ID
- `budget` - 计划总预算（元）
- `spent` - 已消费金额（元）
- `bid_price` - 竞价出价（元）
- `status` - 当前状态
- `start_time/end_time` - 投放时间范围

**ID 范围**: 100000 - 199999

**与 OpenRTB 映射**:
- `bid_price` → OpenRTB Bid Response 中的 `price` (需要转换为微秒)
- `budget` 可用于预算控制逻辑
- `status` 决定是否参与竞价

#### `Creative` - 创意素材

```cpp
struct Creative {
    uint64_t id;              // 创意 ID
    std::string title;        // 创意标题
    std::string description;  // 创意描述
    std::string url;          // 素材 URL
    std::string type;         // 类型: "banner" | "video" | "native"
    uint32_t width;           // 宽度（像素）
    uint32_t height;          // 高度（像素）
};
```

**说明**:
- `id` - 全局唯一标识符
- `type` - 创意类型
  - `"banner"` - 横幅广告
  - `"video"` - 视频广告
  - `"native"` - 原生广告
- `width/height` - 素材尺寸

**ID 范围**: 500000 - 599999

**与 OpenRTB 映射**:
- OpenRTB Bid Response 中的 `crid` (Creative ID) 可映射到此 `id`
- `width/height` 对应 OpenRTB Bid Request 中的 `banner.w` 和 `banner.h`
- `type` 对应 OpenRTB 中的 `creative` 属性
- 完整的创意内容通常在 Bid Response 的 `adm` (Ad Markup) 字段中返回

#### `Ad` - 广告单元

```cpp
struct Ad {
    uint64_t id;                       // 广告 ID
    uint64_t campaign_id;              // 所属计划 ID
    uint64_t creative_id;              // 所用创意 ID
    double bid_price;                  // 出价
    std::vector<AdCategory> categories;// 广告分类
    std::vector<std::string> targeting_geo;       // 地域定向
    std::vector<DeviceType> targeting_devices;    // 设备定向
    std::vector<std::string> targeting_interests; // 兴趣定向
};
```

**说明**:
- `id` - 全局唯一标识符
- `campaign_id` - 关联的广告计划 ID
- `creative_id` - 使用的创意 ID
- `bid_price` - 竞价出价（元）
- `categories` - 广告分类列表
- `targeting_geo` - 地域定向城市列表
- `targeting_devices` - 设备类型列表
- `targeting_interests` - 兴趣标签列表

**ID 范围**: 1000000+

**与 OpenRTB 映射**:
- `id` → OpenRTB Bid Response 中的 `adid` 或自定义扩展
- `campaign_id` 可用于追踪和报告
- `creative_id` → OpenRTB 中的 `crid`
- `bid_price` → OpenRTB `price` (需要转换为微秒)
- `categories` → OpenRTB `cat` (IAB 分类)
- `targeting_geo` → 在 Bid Request 验证阶段使用
- `targeting_devices` → 对应 Bid Request 中的 `device.devicetype`
- `targeting_interests` → 可映射到 Bid Request 中的用户兴趣数据

---

## 🔧 辅助函数

### 类型转换函数

#### `ad_category_to_string`

```cpp
std::string ad_category_to_string(AdCategory category);
```

将 `AdCategory` 枚举转换为小写字符串。

**返回值**:
- `"ecommerce"`, `"gaming"`, `"finance"`, `"education"`,
  `"health"`, `"automotive"`, `"travel"`, `"entertainment"`,
  `"technology"`, `"food"`
- `"unknown"` - 无效分类

#### `device_type_to_string`

```cpp
std::string device_type_to_string(DeviceType device);
```

将 `DeviceType` 枚举转换为小写字符串。

**返回值**:
- `"mobile"`, `"desktop"`, `"tablet"`
- `"unknown"` - 无效设备类型

---

## 💡 使用示例

### 基本使用

```cpp
#include "models/ad_model.h"

using namespace flow_ad;

// 创建广告主
Advertiser advertiser;
advertiser.id = 10000;
advertiser.name = "阿里巴巴";
advertiser.category = "ecommerce";

// 创建广告计划
AdCampaign campaign;
campaign.id = 100000;
campaign.advertiser_id = advertiser.id;
campaign.name = "双11大促";
campaign.budget = 100000.0;
campaign.spent = 0.0;
campaign.bid_price = 5.5;
campaign.status = AdStatus::ACTIVE;
campaign.start_time = std::chrono::system_clock::now();
campaign.end_time = campaign.start_time + std::chrono::hours(24 * 7);

// 创建创意
Creative creative;
creative.id = 500000;
creative.title = "限时特惠，全场5折！";
creative.description = "双11狂欢，不容错过";
creative.url = "https://example.com/ad123.jpg";
creative.type = "banner";
creative.width = 320;
creative.height = 50;

// 创建广告
Ad ad;
ad.id = 1000000;
ad.campaign_id = campaign.id;
ad.creative_id = creative.id;
ad.bid_price = 5.5;
ad.categories = {AdCategory::ECOMMERCE, AdCategory::FOOD};
ad.targeting_geo = {"北京", "上海", "广州"};
ad.targeting_devices = {DeviceType::MOBILE, DeviceType::TABLET};
ad.targeting_interests = {"购物", "美食", "时尚"};
```

### 使用辅助函数

```cpp
#include "models/ad_model.h"

using namespace flow_ad;

// 转换枚举为字符串
AdCategory category = AdCategory::ECOMMERCE;
std::string category_str = ad_category_to_string(category);
// 返回: "ecommerce"

DeviceType device = DeviceType::MOBILE;
std::string device_str = device_type_to_string(device);
// 返回: "mobile"
```

---

## 📐 设计规范

### 命名约定

- **类型名**: PascalCase (如 `AdCampaign`)
- **枚举值**: UPPER_SNAKE_CASE (如 `ACTIVE`, `PAUSED`)
- **成员变量**: snake_case (如 `advertiser_id`)
- **函数**: snake_case (如 `ad_category_to_string`)

### ID 分配规范

| 类型 | ID 范围 | 示例 | OpenRTB 映射 |
|------|---------|------|-------------|
| Advertiser | 10000 - 19999 | 10000, 10001, ... | `adid` / `adomain` |
| Campaign | 100000 - 199999 | 100000, 100001, ... | 自定义扩展 |
| Creative | 500000 - 599999 | 500000, 500001, ... | `crid` |
| Ad | 1000000+ | 1000000, 1000001, ... | `adid` |

**OpenRTB 字段说明**:
- `adid` - 广告 ID (可选)
- `crid` - 创意 ID (推荐)
- `adomain` - 广告主域名数组 (用于广告主验证)
- `cat` - IAB 内容分类数组
- `attr` - 创意属性数组

### 数据验证

建议在使用数据模型前进行验证：

```cpp
bool validate_ad(const Ad& ad) {
    if (ad.id == 0) return false;
    if (ad.campaign_id == 0) return false;
    if (ad.creative_id == 0) return false;
    if (ad.bid_price <= 0.0) return false;
    if (ad.categories.empty()) return false;
    return true;
}
```

---

## 🔄 版本历史

### v1.1.0 (当前版本)
- ✅ 新增 OpenRTB 协议映射说明
- ✅ 完善数据模型的 ID 范围规范
- ✅ 添加与 OpenRTB 字段的对应关系

### v1.0.0
- ✅ 初始数据模型定义
- ✅ 4 种枚举类型
- ✅ 4 种核心数据结构
- ✅ 类型转换辅助函数

---

## 📚 相关文档

- [Data 模块主文档](../README.md)
- [数据生成器文档](../generator/README.md)
- [OpenRTB 协议规范](https://www.iab.com/guidelines/openrtb/)
- [IAB OpenRTB 2.5 规范 (PDF)](https://www.iab.com/wp-content/uploads/2016/03/OpenRTB-API-Specification-Version-2-5-FINAL.pdf)
- [项目主 README](../../README.md)
- [架构设计文档](../../.claude/commands/schema.md)

---

<div align="center">

**Made with ❤️ by [airprofly](https://github.com/airprofly)**

**FastMatch-Ad 广告投放系统** · OpenRTB 协议 · 类型安全

</div>
