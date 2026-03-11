<div align="center">

# 🎲 广告数据生成器
### Ad Data Generator for OpenRTB

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20) [![Status: Ready](https://img.shields.io/badge/Status-Ready-success.svg)]()

大规模生成 · 可配置 · OpenRTB 协议

**✅ 已完成并可用** - 可以立即开始生成测试数据

</div>

---

## 📖 功能简介

高性能的**广告数据生成器**，用于生成符合 **OpenRTB 2.5/2.6 协议规范**的模拟广告数据和竞价请求数据。支持生成 **10万+ 广告池** 和 **标准 OpenRTB Bid Request**，可灵活配置出价范围、预算、地域定向等参数。

### 核心特性

- ✅ **大规模数据生成** - 支持 10万+ 广告、1万+ 计划、5万+ 创意
- ✅ **符合 OpenRTB 规范** - 数据结构严格遵循 OpenRTB 2.5/2.6 协议
- ✅ **可定制配置** - 灵活配置出价范围、预算、地域定向等参数
- ✅ **可重复生成** - 使用随机种子确保数据可重复
- ✅ **JSON 格式导出** - 支持导出为标准 JSON 格式
- ✅ **Bid Request 生成** - 生成完整的 OpenRTB 2.5 竞价请求数据

---

## 📌 功能模块

### 核心生成功能

#### 1. 广告主生成
- 生成广告主 ID、名称、行业分类
- 支持 1000+ 广告主
- 预置知名广告主名称库

#### 2. 广告计划生成
- 生成广告计划 ID、预算、出价
- 随机分配投放时间范围
- 支持多种状态（活跃/暂停/待审）

#### 3. 创意素材生成
- 生成多种类型创意（Banner/Video/Native）
- 支持多种标准尺寸
- 自动生成创意标题和描述

#### 4. 广告生成
- 生成广告 ID、定向条件
- 支持地域、设备、兴趣多维度定向
- 随机分配分类标签

#### 5. OpenRTB Bid Request 生成 🆕
- 生成符合 OpenRTB 2.5 规范的完整竞价请求
- 包含 device, user, app/site, imp 等核心对象
- 支持多种设备类型（7种）、操作系统（5种）
- 地理位置覆盖 20 个中国城市
- 支持 6 种标准广告位尺寸
- 包含 22 个 IAB 内容分类
- 支持 First Price 和 Second Price 拍卖类型

---

## 🎯 定向维度支持

### 地域定向 (20+ 城市)
北京、上海、广州、深圳、杭州、成都、重庆、武汉、西安、南京、天津、苏州、长沙、郑州、青岛、大连、厦门、无锡、宁波、沈阳

### 设备定向 (7种类型)
- 移动端/平板 (Mobile/Tablet)
- 个人电脑 (PC)
- 联网电视 (Connected TV)
- 手机 (Phone)
- 联网设备 (Connected Device)
- 机顶盒 (Set Top Box)

### 操作系统 (5种)
iOS, Android, Windows, macOS, Linux

### 兴趣定向 (18+ 标签)
购物、游戏、理财、教育、健康、汽车、旅游、娱乐、科技、美食、运动、时尚、母婴、房产、家居、阅读、音乐、视频

### 分类定向 (10 大分类)
电商、游戏、金融、教育、健康、汽车、旅游、娱乐、科技、美食

### IAB 内容分类 (22+ 类别)
IAB1 (Arts & Entertainment), IAB2 (Automotive), IAB3 (Business), IAB5 (Education), IAB7 (Health & Fitness), IAB8 (Food & Drink), IAB12 (News), IAB16 (Sports), IAB17 (Style & Fashion), IAB18 (Technology & Computing), IAB19 (Travel), IAB22 (Shopping), IAB23 (Gaming) 等

---

## 📋 已生成的数据

### 广告库存数据 (ads_data.json)

**文件位置**: [`data/data/ads_data.json`](data/data/ads_data.json)

**数据统计**:
- 📊 文件大小: 888 KB
- 👥 广告主: 100 个
- 📋 广告计划: 1,000 个
- 🎨 创意素材: 5,000 个
- 📢 广告单元: 100 个

**数据结构**:
```json
{
  "advertisers": [...],    // 广告主信息
  "campaigns": [...],      // 广告计划
  "creatives": [...],      // 创意素材
  "ads": [...]            // 广告单元
}
```

### OpenRTB Bid Request 数据 (openrtb_requests.json) 🆕

**文件位置**: [`data/data/openrtb_requests.json`](data/data/openrtb_requests.json)

**数据统计**:
- 📊 文件大小: 204 KB
- 📝 请求数量: 100 个
- 👥 唯一用户: 100 个
- 📱 设备类型: 7 种
- 💻 操作系统: 5 种
- 🌍 覆盖城市: 20 个
- 📦 广告位尺寸: 6 种

**OpenRTB 2.5 合规性**:
- ✅ 必填字段: `id`, `imp` (100/100)
- ✅ 设备对象: 完整的 `device` 信息
- ✅ 用户对象: 包含 `user` 识别信息
- ✅ 应用/站点: 支持 `app` 和 `site` 两种场景
- ✅ Imp 对象: 包含 `id`, `banner`, `bidfloor` (187/187)

**请求示例**:
```json
{
  "id": "req-20260311054219-000001",
  "imp": [{
    "id": "imp-0",
    "banner": {"w": 300, "h": 600, "pos": 4},
    "bidfloor": 16.89,
    "bidfloorcur": "CNY"
  }],
  "device": {
    "ua": "Mozilla/5.0...",
    "ip": "190.140.125.58",
    "geo": {"country": "CN", "city": "深圳"},
    "devicetype": 6,
    "os": 1,
    "connectiontype": 2
  },
  "user": {
    "id": "user-2571945",
    "buyeruid": "buyeruid-329258",
    "yob": 1987,
    "gender": "O"
  },
  "app": {
    "id": "app009",
    "name": "时尚杂志",
    "cat": ["IAB17"],
    "publisher": {...}
  },
  "at": 1,
  "tmax": 120,
  "bcat": ["IAB26", "IAB25"...],
  "badv": ["blocked-advertiser-1.com"...]
}
```

**数据分布**:
- 📱 设备类型: Mobile (13), Desktop (16), CTV (16), Phone (14), Tablet (11), Connected (18), STB (12)
- 💻 操作系统: iOS (24), Android (17), Windows (17), macOS (22), Linux (20)
- 🌍 热门城市: 长沙 (11), 深圳 (9), 南京 (7), 杭州 (7), 大连 (7)
- 📦 广告位: 728x90 (37), 300x250 (36), 300x600 (34), 160x600 (30), 320x50 (29), 320x480 (21)
- 💰 拍卖类型: First Price (58), Second Price (42)

---

## 🚀 快速开始

### 1. 构建生成器

```bash
# 在项目根目录
mkdir build && cd build
cmake ..
make ad_data_generator -j$(nproc)
```

### 2. 使用便捷脚本 (推荐)

```bash
# 生成默认 100 条广告到 ./data/data/ads_data.json
./scripts/generate_data.sh

# 自定义配置
./scripts/generate_data.sh -n 10000 -o ./data/data/my_ads.json

# 小规模测试（1000 广告）
./scripts/generate_data.sh -n 1000 -c 100 -r 500 -a 50

# 中等规模测试（1 万广告）
./scripts/generate_data.sh -n 10000 -c 1000 -r 5000 -a 100

# 大规模生产（10 万广告）
./scripts/generate_data.sh -n 100000 -c 10000 -r 50000 -a 1000
```

### 3. 直接运行

#### 使用默认配置（生成 100 条广告）
```bash
./build/bin/ad_generator
```

#### 使用命令行参数

| 参数 | 简写 | 说明 | 默认值 |
|------|------|------|--------|
| `--num-ads` | `-n` | 生成广告数量 | 100 |
| `--num-campaigns` | `-c` | 生成计划数量 | 10,000 |
| `--num-creatives` | `-r` | 生成创意数量 | 50,000 |
| `--num-advertisers` | `-a` | 生成广告主数量 | 1,000 |
| `--output` | `-o` | 输出文件路径 | ./data/data/ads_data.json |
| `--seed` | `-s` | 随机种子 | 42 |
| `--min-bid` | - | 最低出价 (元) | 0.1 |
| `--max-bid` | - | 最高出价 (元) | 100.0 |
| `--min-budget` | - | 最低预算 (元) | 100.0 |
| `--max-budget` | - | 最高预算 (元) | 100,000.0 |

---

## 💻 在代码中使用

```cpp
#include "generator/ad_generator.h"

using namespace flow_ad;

// 创建生成器（使用默认配置：100条广告）
GeneratorConfig config;
// config.num_ads = 100;  // 默认值，可省略
// config.min_bid_price = 1.0;
// config.max_bid_price = 50.0;

AdDataGenerator generator(config);
generator.generate_all();

// 访问生成的数据
const auto& ads = generator.ads();
const auto& campaigns = generator.campaigns();
const auto& creatives = generator.creatives();
const auto& advertisers = generator.advertisers();

// 保存到文件
generator.save_to_file("./data/data/output.json");

// 打印统计信息
std::cout << "Generated " << ads.size() << " ads" << std::endl;
```

**配置示例**:
```cpp
// 小规模测试（100条广告）
GeneratorConfig config;  // 使用默认值即可

// 中等规模测试（1万条广告）
config.num_ads = 10000;
config.num_campaigns = 1000;
config.num_creatives = 5000;
config.num_advertisers = 100;

// 大规模测试（10万条广告）
config.num_ads = 100000;
config.num_campaigns = 10000;
config.num_creatives = 50000;
config.num_advertisers = 1000;
```

---

## 💾 输出格式

### 广告库存数据格式 (ads_data.json)

生成的 JSON 文件结构：

```json
{
  "advertisers": [
    {
      "id": 10000,
      "name": "阿里巴巴",
      "category": "ecommerce"
    }
  ],
  "campaigns": [
    {
      "id": 100000,
      "advertiser_id": 10000,
      "name": "Campaign_100000",
      "budget": 50000.0,
      "bid_price": 5.5,
      "status": 1
    }
  ],
  "creatives": [
    {
      "id": 500000,
      "title": "限时特惠，不容错过！",
      "type": "banner",
      "width": 320,
      "height": 50
    }
  ],
  "ads": [
    {
      "id": 1000000,
      "campaign_id": 100000,
      "creative_id": 500000,
      "bid_price": 5.5,
      "categories": [0, 1],
      "targeting_geo": ["北京", "上海"]
    }
  ]
}
```

**注意**:
- `category` 使用英文小写 (如 "ecommerce", "gaming")
- `status` 值: 0=PENDING, 1=ACTIVE, 2=PAUSED
- `categories` 使用枚举整数值

### OpenRTB Bid Request 格式 (openrtb_requests.json) 🆕

符合 OpenRTB 2.5 规范的竞价请求数据，包含完整的请求对象结构。每个请求包含:
- **id**: 唯一请求标识符
- **imp**: 曝光对象数组 (1-3个)
- **device**: 设备信息 (UA, IP, 地理位置, 设备类型, OS)
- **user**: 用户信息 (ID, 出价者UID, 出生年份, 性别)
- **app** 或 **site**: 应用或站点信息
- **at**: 拍卖类型 (1=First Price, 2=Second Price)
- **tmax**: 最大超时时间 (ms)
- **bcat**: 屏蔽的广告分类
- **badv**: 屏蔽的广告主域名

详细结构见上方 "OpenRTB Bid Request 数据" 部分。

---

## 📈 性能指标

| 数据量 | 生成时间 | 内存占用 | 文件大小 |
|--------|----------|----------|----------|
| 100 | < 1 ms | ~5 MB | ~50 KB |
| 1,000 | < 1 ms | ~10 MB | ~500 KB |
| 10,000 | ~1-10 ms | ~50 MB | ~5 MB |
| 100,000 | ~10-100 ms | ~500 MB | ~50 MB |

**推荐配置**:
- **快速测试**: 100 条广告 (默认配置)
- **功能测试**: 1,000-10,000 条广告
- **性能测试**: 100,000 条广告

---

## 📝 数据分布

| 维度 | 分布说明 |
|------|---------|
| **默认数量** | 100 条广告、10,000 计划、50,000 创意、1,000 广告主 |
| **出价分布** | 0.1 - 100 元，均匀分布 |
| **预算分布** | 100 - 100,000 元，均匀分布 |
| **分类分布** | 10 大分类均匀分布 |
| **地域分布** | 14+ 城市均匀分布 |
| **状态分布** | 80% 活跃、15% 暂停、5% 待审 |
| **输出路径** | 默认 `./data/data/ads_data.json` |

---

## 🧪 运行测试

```bash
# 运行自动化测试套件
./scripts/test_generator.sh

# 测试包括:
# ✅ 小规模测试 (1,000 广告)
# ✅ 中等规模测试 (10,000 广告)
# ✅ 自定义参数测试
# ✅ JSON 格式验证
```

---

## ⚠️ 注意事项

1. **默认配置**: 默认生成 100 条广告到 `./data/data/ads_data.json`，适合快速测试
2. **输出目录**: 默认输出到 `./data/data/` 目录,脚本会自动创建该目录
3. **内存使用**: 生成大量数据（10万+）需要足够内存，建议至少 2GB 可用内存
4. **文件大小**: JSON 格式文件较大，大规模数据建议使用 Protocol Buffers（待实现）
5. **随机性**: 使用相同随机种子可生成相同数据，便于测试和调试
6. **数据真实性**: 生成的数据为模拟数据，仅供测试使用

---

## 🔲 待完成 / TODO

### 当前状态
- ✅ 核心生成功能完成
- ✅ JSON 导出功能完成
- ✅ 命令行接口完成
- ✅ 自动化测试完成
- ✅ OpenRTB 2.5 Bid Request 数据生成完成
- ✅ 广告库存数据生成完成 (100条)
- ⏸️ DataLoader 功能 (等待 AdService 实现)

### 计划功能
- [ ] Protocol Buffers 格式导出
- [ ] 数据压缩支持
- [ ] 增量数据生成
- [ ] 数据验证功能
- [ ] 单元测试
- [ ] OpenRTB Bid Response 数据生成

---

## 📚 相关文档

- [Data 模块主文档](../README.md)
- [项目主 README](../../README.md)
- [架构设计](../../.claude/commands/schema.md)
- [OpenRTB 协议规范](https://www.iab.com/guidelines/openrtb/)
- [IAB OpenRTB 2.5 规范 (PDF)](https://www.iab.com/wp-content/uploads/2016/03/OpenRTB-API-Specification-Version-2-5-FINAL.pdf)
- [IAB Tech Lab - 2025更新建议](https://iabtechlab.com/the-openrtb-updates-you-should-adopt-in-2025-courtesy-of-iab-tech-lab/)

---

<div align="center">

**Made with ❤️ by [airprofly](https://github.com/airprofly)**

**FastMatch-Ad 广告投放系统** · OpenRTB 协议 · 高性能广告技术

</div>
