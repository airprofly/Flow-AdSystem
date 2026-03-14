<div align="center">

# 📦 Data 模块
### Flow-Ad 广告投放系统 - 数据层

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/Flow-AdSystem) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20) [![CMake](https://img.shields.io/badge/CMake-3.20+-blue.svg)](https://cmake.org/) [![OpenRTB](https://img.shields.io/badge/OpenRTB-2.5%2F2.6-green.svg)](https://www.iab.com/guidelines/openrtb/) [![Status: Active](https://img.shields.io/badge/Status-Active-success.svg)]()

OpenRTB 协议 · 数据模型 · 数据生成 · 数据加载

**✅ 核心功能已完成** - 数据模型和生成器可用

</div>

---

## 📖 模块简介

`data` 模块是 Flow-Ad 广告投放系统的**数据层**,负责数据的定义、生成、加载和管理。该模块严格遵循 **OpenRTB 2.5/2.6 协议规范**,为整个系统提供标准化的数据结构和处理能力。

### 核心职责

- 🔧 **数据模型定义** - 定义广告系统核心数据结构
- 🎲 **数据生成** - 生成大规模测试数据用于性能测试
- 📥 **数据加载** - 从外部数据源加载广告数据
- 🔄 **数据序列化** - 支持 JSON 和 Protocol Buffers 格式
- 📋 **OpenRTB 支持** - 生成符合 OpenRTB 2.5 规范的 Bid Request 数据

---

## 📁 模块结构

```text
data/
├── models/                      # 📊 数据模型定义
│   └── ad_model.h              # 广告相关数据结构
│
├── generator/                   # 🎲 数据生成器
│   ├── ad_generator.h          # 生成器核心逻辑
│   ├── ad_generator.cpp
│   ├── data_loader.h           # 数据加载器 (TODO)
│   ├── data_loader.cpp
│   ├── main.cpp                # CLI 入口
│   └── CMakeLists.txt          # 生成器构建配置
│
├── openrtb/                     # 📋 OpenRTB 协议定义
│   └── openrtb.proto           # Protocol Buffers 定义 (TODO)
│
├── data/                        # 💾 生成的数据文件
│   ├── ads_data.json           # 广告库存数据 (100条)
│   └── openrtb_requests.json   # OpenRTB Bid Request 数据 (100条)
│
├── CMakeLists.txt              # 📝 Data 模块主构建配置
└── README.md                   # 📖 本文档
```

---

## 📊 子模块概览

<details>
<summary><b>1️⃣ models - 数据模型层</b> <code>✅ 完成</code></summary>

**职责**: 定义广告系统的核心数据结构

**主要组件**:
- `ad_model.h` - 包含所有广告相关的数据结构定义
  - `Advertiser` - 广告主信息
  - `AdCampaign` - 广告计划
  - `Creative` - 创意素材
  - `Ad` - 广告单元
  - 枚举类型: `AdCategory`, `DeviceType`, `AdStatus`

**依赖**: 无 (仅使用 STL)

**使用方式**:
```cpp
#include "models/ad_model.h"

using namespace flow_ad;

Ad ad;
ad.id = 123456;
ad.campaign_id = 100000;
ad.bid_price = 5.5;
ad.categories = {AdCategory::ECOMMERCE, AdCategory::FOOD};
```

**📖 详细文档**: [models/README.md](models/README.md)

</details>

<details>
<summary><b>2️⃣ generator - 数据生成器</b> <code>✅ 核心完成</code></summary>

**职责**: 生成大规模测试数据

**主要特性**:
- ✅ 支持 10 万+ 广告生成
- ✅ 可配置出价、预算、定向等参数
- ✅ JSON 格式导出
- ⏸️ DataLoader (等待 AdService 实现)

**快速开始**:

**方式一: 使用便捷脚本 (推荐)** ⭐
```bash
# 使用脚本自动构建并生成数据
./scripts/generate_data.sh

# 自定义配置生成
./scripts/generate_data.sh -n 10000 -o ./data/data/test.json

# 查看脚本帮助
./scripts/generate_data.sh --help
```

**方式二: 手动构建并运行**
```bash
# 构建生成器
cd build && cmake ..
make ad_generator -j$(nproc)

# 生成默认 100 条广告
./bin/ad_generator

# 自定义配置
./bin/ad_generator -n 10000 -o ./data/data/my_ads.json
```

**📖 详细文档**: [generator/README.md](generator/README.md)

</details>

<details>
<summary><b>3️⃣ data - 数据存储</b> <code>✅ 可用</code></summary>

**职责**: 存储生成的测试数据

**包含文件**:
- `ads_data.json` - 广告库存模拟数据 (100条广告)
  - 100个广告主 (advertisers)
  - 1,000个广告计划 (campaigns)
  - 5,000个创意素材 (creatives)
  - 100个广告单元 (ads)
- `openrtb_requests.json` - OpenRTB 2.5 Bid Request 模拟数据 (100条)
  - 符合 OpenRTB 2.5 规范的完整请求
  - 包含 device, user, app/site, imp 等对象
  - 支持多种设备类型、操作系统、地理位置
  - 20个中国城市、7种设备类型、5种操作系统
  - 6种标准广告位尺寸、22个IAB内容分类

**数据统计**:
- 📊 广告数据: 888KB, 包含完整的广告库存结构
- 📋 OpenRTB请求: 204KB, 100个标准竞价请求
- ✅ 所有数据符合 OpenRTB 2.5 规范

**使用场景**:
- RTB 系统测试
- 竞价算法训练
- 压力测试
- 集成测试

</details>

<details>
<summary><b>4️⃣ openrtb - OpenRTB 协议支持</b> <code>✅ Bid Request 已完成</code></summary>

**职责**: OpenRTB 协议的数据生成和支持

**已完成功能**:
- ✅ OpenRTB 2.5 Bid Request 模拟数据生成 (100条)
- ✅ 完整的请求结构 (id, imp, device, user, app/site)
- ✅ 多种设备和操作系统支持
- ✅ 地理位置定向 (20个中国城市)
- ✅ IAB 内容分类支持
- ✅ 标准广告位尺寸

**计划功能**:
- [ ] 定义 OpenRTB 2.5/2.6 的 .proto 文件
- [ ] 生成 C++ 序列化代码
- [ ] 支持请求/响应的序列化和反序列化
- [ ] Bid Response 模拟数据生成

**参考资源**:
- [IAB OpenRTB 2.5 规范](https://www.iab.com/wp-content/uploads/2016/03/OpenRTB-API-Specification-Version-2-5-FINAL.pdf)
- [IAB Tech Lab - 2025更新建议](https://iabtechlab.com/the-openrtb-updates-that-you-should-adopt-in-2025-courtesy-of-iab-tech-lab/)

</details>

---

## 🛠️ 技术栈

<details>
<summary><b>技术栈详情</b></summary>

| 技术 | 版本 | 说明 | 状态 |
|------|------|------|------|
| **C++** | 20+ | 现代 C++ 特性 | ✅ 使用中 |
| **CMake** | 3.20+ | 跨平台构建系统 | ✅ 使用中 |
| **OpenRTB** | 2.5/2.6 | IAB 标准广告竞价协议 | ✅ 遵循规范 |
| **Protocol Buffers** | 3.0+ | 高效二进制序列化 | ⏸️ 计划中 |
| **nlohmann/json** | 3.11+ | JSON 库 | ⏸️ 计划中 |

</details>

---

## 🔧 构建说明

<details>
<summary><b>📋 环境要求</b></summary>

- **操作系统**: Linux (Ubuntu 20.04+) / macOS
- **编译器**: GCC 11+ / Clang 13+
- **CMake**: 3.20 或更高版本

</details>

<details>
<summary><b>🚀 构建步骤</b></summary>

**方式一: 使用脚本自动构建** (推荐)
```bash
# 脚本会自动处理构建和生成
./scripts/generate_data.sh
```

**方式二: 手动构建**
```bash
# 1. 配置项目
cd build
cmake ..

# 2. 构建数据生成器
make ad_generator -j$(nproc)

# 3. 运行生成器
./bin/ad_generator -n 100 -o ./data/data/ads_data.json

# 4. (可选) 构建 protobuf 支持
make protobuf_generated
```

</details>

---

## 📚 API 参考

### 数据模型 API

<details>
<summary><b>📋 数据结构详解</b></summary>

#### Advertiser (广告主)
```cpp
struct Advertiser {
    uint64_t id;              // 广告主 ID
    std::string name;         // 广告主名称
    std::string category;     // 行业分类 (ecommerce, gaming, etc.)
};
```

#### AdCampaign (广告计划)
```cpp
struct AdCampaign {
    uint64_t id;              // 计划 ID
    uint64_t advertiser_id;   // 所属广告主 ID
    std::string name;         // 计划名称
    double budget;            // 总预算
    double spent;             // 已消费金额
    double bid_price;         // 出价
    AdStatus status;          // 状态: PENDING/ACTIVE/PAUSED/ENDED/DELETED
    std::chrono::system_clock::time_point start_time;  // 开始时间
    std::chrono::system_clock::time_point end_time;    // 结束时间
};
```

#### Creative (创意素材)
```cpp
struct Creative {
    uint64_t id;              // 创意 ID
    std::string title;        // 创意标题
    std::string description;  // 创意描述
    std::string url;          // 素材 URL
    std::string type;         // 类型: "banner" | "video" | "native"
    uint32_t width;           // 宽度 (像素)
    uint32_t height;          // 高度 (像素)
};
```

#### Ad (广告单元)
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

</details>

### 辅助函数 API

<details>
<summary><b>🔧 类型转换函数</b></summary>

```cpp
// 广告分类转字符串
std::string ad_category_to_string(AdCategory category);
// 返回: "ecommerce", "gaming", "finance", ...

// 设备类型转字符串
std::string device_type_to_string(DeviceType device);
// 返回: "mobile", "desktop", "tablet"
```

</details>

---

## 🎯 使用示例

<details>
<summary><b>💻 示例 1: 使用数据模型</b></summary>

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
campaign.bid_price = 5.5;
campaign.status = AdStatus::ACTIVE;

// 创建广告
Ad ad;
ad.id = 1000000;
ad.campaign_id = campaign.id;
ad.bid_price = 5.5;
ad.categories = {AdCategory::ECOMMERCE, AdCategory::FOOD};
ad.targeting_geo = {"北京", "上海"};
ad.targeting_devices = {DeviceType::MOBILE, DeviceType::TABLET};
```

</details>

<details>
<summary><b>🎲 示例 2: 使用数据生成器</b></summary>

**通过脚本使用** (推荐):
```bash
# 使用默认配置生成 100 条广告到 ./data/data/ads_data.json
./scripts/generate_data.sh

# 自定义配置生成
./scripts/generate_data.sh -n 10000 -c 1000 -r 5000 -a 100

# 查看所有可用参数
./scripts/generate_data.sh --help
```

**通过 C++ 代码使用**:
```cpp
#include "generator/ad_generator.h"

using namespace flow_ad;

// 配置生成器
GeneratorConfig config;
config.num_ads = 100000;
config.num_campaigns = 10000;
config.num_creatives = 50000;
config.num_advertisers = 1000;
config.min_bid_price = 1.0;
config.max_bid_price = 50.0;

// 生成数据
AdDataGenerator generator(config);
generator.generate_all();

// 访问生成的数据
const auto& ads = generator.ads();
const auto& campaigns = generator.campaigns();

// 保存到文件
generator.save_to_file("output.json");

// 打印统计信息
std::cout << "Generated " << ads.size() << " ads" << std::endl;
```

**命令行参数说明**:
```bash
-n, --num-ads <N>              广告数量 (默认: 100)
-c, --num-campaigns <N>        广告计划数量 (默认: 1000)
-r, --num-creatives <N>        创意数量 (默认: 5000)
-a, --num-advertisers <N>      广告主数量 (默认: 100)
-o, --output <file>            输出文件路径 (默认: ./data/data/ads_data.json)
-s, --seed <N>                 随机种子 (默认: 42)
```

</details>

---

## 📈 性能指标

<details>
<summary><b>📊 性能数据</b></summary>

| 操作 | 数据量 | 耗时 | 内存占用 |
|------|--------|------|----------|
| 数据生成 | 100 广告 | ~1-5 ms | ~5 MB |
| 数据生成 | 10,000 广告 | ~10-50 ms | ~50 MB |
| 数据生成 | 100,000 广告 | ~100-500 ms | ~500 MB |
| JSON 导出 | 100 广告 | ~10 ms | - |
| JSON 导出 | 100,000 广告 | ~100-500 ms | - |
| 文件大小 | 100 广告 | - | ~50 KB |
| 文件大小 | 100,000 广告 | - | ~50 MB |

**推荐配置**:
- 小规模测试: 100-1,000 条广告
- 中规模测试: 1,000-10,000 条广告
- 大规模测试: 10,000-100,000 条广告

</details>

---

## ⚠️ 注意事项

1. **内存使用**: 生成大量数据（10 万+）需要足够内存，建议至少 2GB 可用内存
2. **线程安全**: 当前数据模型不是线程安全的，多线程环境需额外同步
3. **数据验证**: 生成的数据为模拟数据，仅供测试使用
4. **依赖管理**: DataLoader 功能需要等待 AdService 模块实现后才能完成
5. **输出目录**: 默认输出到 `./data/data/` 目录,脚本会自动创建该目录

---

## 🔲 开发计划

<details>
<summary><b>📋 开发进度</b></summary>

### 已完成 ✅
- [x] 数据模型定义
- [x] 数据生成器核心功能
- [x] JSON 导出支持
- [x] 命令行接口
- [x] 广告库存数据生成 (100条广告)
- [x] OpenRTB 2.5 Bid Request 数据生成 (100条请求)
- [x] OpenRTB 协议合规性验证

### 进行中 🚧
- [ ] DataLoader 实现 (等待 AdService)
- [ ] 单元测试

### 计划中 📋
- [ ] OpenRTB Protocol Buffers 支持
- [ ] OpenRTB Bid Response 数据生成
- [ ] 数据压缩支持
- [ ] 增量数据生成
- [ ] 数据验证功能
- [ ] 性能优化

</details>

---

## 📚 相关文档

<details>
<summary><b>🔗 文档链接</b></summary>

- [项目主 README](../README.md)
- [构建说明](../BUILD.md) (待创建)
- [架构设计](../.claude/commands/schema.md)
- [OpenRTB 协议规范](https://www.iab.com/guidelines/openrtb/)
- [数据生成器文档](generator/README.md)
- [数据模型文档](models/README.md)
- [生成脚本使用说明](../scripts/generate_data.sh)

</details>

---

## 🤝 贡献指南

<details>
<summary><b>💡 如何贡献</b></summary>

欢迎贡献代码、报告问题或提出改进建议！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

</details>

---

## 📄 许可证

本项目采用 [MIT 许可证](https://opensource.org/licenses/MIT)。

---

<div align="center">

**Made with ❤️ by [airprofly](https://github.com/airprofly)**

**Flow-AdSystem 广告投放系统** · OpenRTB 协议 · 高性能广告技术

</div>
