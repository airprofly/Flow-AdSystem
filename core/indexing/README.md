<div align="center">

# 🔧 广告索引模块
### Ad Indexing Module - High-Performance Ad Retrieval System

[![GitHub](https://img.shields.io/badge/GitHub-Repository-black?logo=github)](https://github.com/airprofly/FastMatch-Ad) [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20) [![CMake](https://img.shields.io/badge/CMake-3.20+-blue.svg)](https://cmake.org/) [![RapidJSON](https://img.shields.io/badge/RapidJSON-Header-orange.svg)](https://rapidjson.org/)

倒排索引 · BitMap · 高性能召回 · OpenRTB 2.5

</div>

---

## 📖 项目简介

广告索引模块是 **FastMatch-Ad 广告投放系统**的核心组件（Step 3），负责从大规模广告池中快速召回符合定向条件的候选广告。本模块基于**倒排索引**和**BitMap**数据结构实现，支持毫秒级的广告召回性能，完美兼容 **OpenRTB 2.5** 协议。

### 核心特性

- ⚡ **超低延迟**: 毫秒级召回性能（小规模 < 1ms，10 万广告 < 5ms）
- 🎯 **精准召回**: 100% 召回准确率，支持多维度定向条件
- 📊 **高效压缩**: BitMap 数据结构，内存占用优化
- 🔌 **OpenRTB 兼容**: 自动处理 OpenRTB 2.5 协议转换
- 🧪 **完整测试**: 100% 测试覆盖率，包含大规模性能测试

---

## 📚 功能模块

<details>
<summary><b>🔢 BitMap 数据结构</b> <code>✅ 已完成</code></summary>

#### 功能特性
- 基于 `uint64_t` 数组的高效位图实现
- 支持位运算：交集 (`&`)、并集 (`|`)、差集 (`-`)
- 使用 CPU 指令加速 popcount（`__builtin_popcountll`）
- 动态调整大小，避免内存浪费

#### 性能优化
- 内存布局优化，充分利用 CPU 缓存行
- 预分配内存，避免动态扩容开销
- 零拷贝设计，使用 `const&` 传递

</details>

<details>
<summary><b>🔍 倒排索引 (Inverted Index)</b> <code>✅ 已完成</code></summary>

#### 索引维度
- **地域索引**: 城市/地区 → 广告集合
- **设备索引**: 设备类型 → 广告集合
- **兴趣索引**: 兴趣标签 → 广告集合

#### 查询能力
- AND 条件查询（地域 AND 设备 AND 兴趣）
- OR 条件查询（兴趣1 OR 兴趣2）
- 复合条件查询（AND + OR 组合）
- OpenRTB DeviceType 自动映射转换

</details>

<details>
<summary><b>💾 广告存储 (Ad Store)</b> <code>✅ 已完成</code></summary>

#### 存储结构
- 使用 `std::unordered_map` 实现 O(1) 查找
- 支持广告的增删改查操作
- 内存占用估算功能

#### API 接口
- `add_ad()` - 添加广告
- `remove_ad()` - 移除广告
- `update_ad()` - 更新广告
- `get_ad()` - 获取广告详情

</details>

<details>
<summary><b>🚀 广告索引引擎 (Ad Index)</b> <code>✅ 已完成</code></summary>

#### 统一接口
- 集成倒排索引和广告存储
- 提供简洁的查询 API
- 自动性能统计和监控
- 详细的索引统计信息

#### 查询流程
```
OpenRTB 请求 → InvertedIndex.query() → BitMap 运算 → AdIndex.query() → QueryResult
```

</details>

<details>
<summary><b>📥 数据加载器 (Index Loader)</b> <code>✅ 已完成</code></summary>

#### 加载方式
- 从 JSON 文件批量加载
- 从 JSON 字符串加载
- 从广告向量加载

#### 特性
- 自动错误处理
- 加载统计信息
- 支持 Step 1 生成的广告数据格式

</details>

---

## 📌 项目概览

### 性能指标

| 指标 | 性能 | 说明 |
|------|------|------|
| **召回延迟** | < 1ms (小规模) / < 5ms (10万广告) | 毫秒级响应 |
| **内存占用** | 高效（BitMap 压缩） | 优化的内存布局 |
| **召回准确率** | 100% | 无误召回 |
| **并发支持** | 线程安全（读） | 多线程查询 |

### 技术亮点

1. **BitMap 位运算优化**
   - 使用 `__builtin_popcountll` CPU 指令加速
   - 64 位分块存储，充分利用缓存行
   - 零拷贝设计，避免不必要的内存分配

2. **OpenRTB 2.5 协议兼容**
   - 自动 DeviceType 映射转换
   - 支持地域、设备、兴趣等多维度定向
   - 完整的请求解析和响应生成

3. **高效的数据结构**
   - 倒排索引：`O(1)` 查找复杂度
   - BitMap 集合运算：快速交集/并集
   - 哈希表存储：O(1) 广告详情访问

---

## 📁 项目结构

<details>
<summary><b>查看完整目录结构</b></summary>

```text
core/indexing/
├── bitmap.hpp              # 🔧 BitMap 数据结构（高性能位运算）
├── inverted_index.hpp      # 🔍 倒排索引头文件（基于定向条件的索引）
├── inverted_index.cpp      # 🔍 倒排索引实现
├── ad_store.hpp            # 💾 广告存储头文件（广告详情访问）
├── ad_store.cpp            # 💾 广告存储实现
├── ad_index.hpp            # 🚀 广告索引引擎头文件（统一接口）
├── ad_index.cpp            # 🚀 广告索引引擎实现
├── index_loader.hpp        # 📥 数据加载器头文件（从 JSON 加载）
├── index_loader.cpp        # 📥 数据加载器实现
├── CMakeLists.txt          # 📝 构建配置
└── README.md               # 📖 本文档

tests/unit/
└── indexing_test.cpp       # 🧪 单元测试（100% 覆盖率）
```

</details>

---

## 🛠️ 技术栈

| 技术 | 版本 | 说明 |
|------|------|------|
| **C++** | 20 | 现代 C++ 特性（RAII、智能指针、移动语义） |
| **CMake** | 3.20+ | 构建系统 |
| **RapidJSON** | latest | JSON 解析（Header-only） |
| **Google Test** | latest | 单元测试框架 |

---

## 🔧 环境配置

### 依赖要求

- **编译器**: GCC 10+ / Clang 12+ / MSVC 2019+
- **CMake**: 3.20 或更高版本
- **C++ 标准**: C++20

### 安装依赖

```bash
# 克隆项目（包含子模块）
git clone --recursive https://github.com/airprofly/FastMatch-Ad.git

# 或单独下载 RapidJSON
cd external
git clone https://github.com/Tencent/rapidjson.git
```

---

## 🚀 快速开始

### 1. 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake ..

# 编译
cmake --build .
```

### 2. 运行测试

```bash
# 运行所有测试
ctest --output-on-failure

# 只运行索引模块测试
ctest -R indexing

# 查看详细测试输出
./bin/indexing_test --gtest_verbose
```

### 3. 使用示例

<details>
<summary><b>基本使用示例</b></summary>

```cpp
#include "core/indexing/ad_index.hpp"
#include "core/openrtb/openrtb_models.h"
#include "data/models/ad_model.h"

using namespace flow_ad;
using namespace flow_ad::indexing;
using namespace flow_ad::openrtb;

// 创建索引
AdIndex index;

// 添加广告
Ad ad;
ad.id = 1;
ad.campaign_id = 101;
ad.creative_id = 1001;
ad.bid_price = 1.5;
ad.categories = {AdCategory::GAMING};
ad.targeting_devices = {DeviceType::MOBILE};
ad.targeting_geo = {"北京", "上海"};
ad.targeting_interests = {"游戏", "竞技"};

index.add_ad(ad.id, ad);

// 查询
OpenRTBRequest request;
request.id = "test-request";

Device device;
device.geo = Geo{};
device.geo->city = "北京";
device.devicetype = static_cast<int>(openrtb::DeviceType::MOBILE);
request.device = device;

User user;
user.id = "user-1";
user.keywords = std::vector<std::string>{"游戏"};
request.user = user;

QueryResult result = index.query(request);

std::cout << "召回广告数量: " << result.candidate_ad_ids.size() << std::endl;
std::cout << "召回耗时: " << result.recall_time_ms << " ms" << std::endl;
```

</details>

<details>
<summary><b>从 JSON 文件加载</b></summary>

```cpp
#include "core/indexing/index_loader.hpp"

AdIndex index;
IndexLoader::load_from_json("data/data/ads_data.json", index);

// 打印统计信息
index.print_stats();
```

</details>

<details>
<summary><b>获取广告详情</b></summary>

```cpp
const Ad* ad_ptr = index.get_ad(1);
if (ad_ptr)
{
    std::cout << "广告 ID: " << ad_ptr->id << std::endl;
    std::cout << "出价: " << ad_ptr->bid_price << std::endl;
}
```

</details>

---

## 📊 性能测试结果

### 小规模测试（1,000 广告）

| 操作 | 平均耗时 | QPS |
|------|---------|-----|
| 添加广告 | 0.05 ms | 20,000 |
| 查询召回 | 0.3 ms | 3,333 |
| 获取详情 | 0.001 ms | 1,000,000 |

### 大规模测试（10,000 广告）

| 操作 | 平均耗时 | QPS |
|------|---------|-----|
| 添加广告 | 0.06 ms | 16,666 |
| 查询召回 | 2.5 ms | 400 |
| 获取详情 | 0.001 ms | 1,000,000 |

### 内存占用

| 广告数量 | 索引内存 | 存储内存 | 总内存 |
|---------|---------|---------|--------|
| 1,000 | 0.5 MB | 0.8 MB | 1.3 MB |
| 10,000 | 5.2 MB | 8.5 MB | 13.7 MB |

---

## 🔌 OpenRTB DeviceType 映射

由于 OpenRTB 2.5 规范的 DeviceType 与内部 Ad 模型的 DeviceType 不同，查询时会自动转换：

| OpenRTB DeviceType | 值 | Ad DeviceType | 值 | 说明 |
|-------------------|---|---------------|---|------|
| MOBILE | 1 | MOBILE | 0 | 手机/移动设备 |
| PERSONAL_COMPUTER | 2 | DESKTOP | 1 | 个人电脑 |
| TABLET | 5 | TABLET | 2 | 平板设备 |

---

## 🧪 测试覆盖

### 测试用例

- ✅ **BitMap 基本操作**: set, clear, test, count, resize
- ✅ **BitMap 位运算**: 交集, 并集, 差集
- ✅ **广告索引 CRUD**: 添加, 删除, 更新, 查询
- ✅ **单条件查询**: 地域, 设备, 兴趣
- ✅ **多条件查询**: AND 组合, OR 组合, 复合条件
- ✅ **边界条件**: 空结果, 无匹配, 无效输入
- ✅ **性能测试**: 大规模数据（10,000 广告）

### 测试结果

```bash
100% tests passed, 0 tests failed out of 49
Total Test time (real) = 0.09 sec
```

---

## 💡 性能优化技巧

### 1. BitMap 优化

```cpp
// ✅ 推荐：预分配大小
BitMap bitmap(10000);

// ❌ 避免：动态扩容
BitMap bitmap;
bitmap.resize(10000);
```

### 2. 批量操作

```cpp
// ✅ 推荐：批量添加
std::vector<std::pair<uint64_t, Ad>> ads = {...};
index.add_ads(ads);

// ❌ 避免：逐个添加
for (auto& [id, ad] : ads)
    index.add_ad(id, ad);
```

### 3. 避免拷贝

```cpp
// ✅ 推荐：使用引用
const BitMap& result = bitmap1 & bitmap2;

// ❌ 避免：不必要的拷贝
BitMap result = bitmap1 & bitmap2;
```

---

## 🔲 已知限制

1. **线程安全**: 当前实现不支持多线程写入
2. **内存占用**: 对于超大规模广告池（百万级），可能需要进一步优化
3. **设备类型映射**: 只支持 MOBILE/DESKTOP/TABLET 三种设备类型
4. **动态更新**: 不支持热加载索引更新

---

## 📝 TODO

- [ ] 支持动态索引更新（热加载）
- [ ] 压缩 BitMap（Roaring Bitmap）
- [ ] 分布式索引支持
- [ ] 更细粒度的定向条件（时间段, 频次控制）
- [ ] 索引分片（Sharding）
- [ ] 多线程写入支持

---

## 🤝 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

### 代码规范

- 遵循 C++20 标准和现代 C++ 最佳实践
- 使用 RAII、智能指针、移动语义
- 添加单元测试（覆盖率 > 90%）
- 添加文档注释

---

## 📄 许可证

本项目采用 [MIT 许可证](https://opensource.org/licenses/MIT)。

---

## 👨‍💻 作者

**airprofly** - [GitHub](https://github.com/airprofly)

---

## 🙏 致谢

- [RapidJSON](https://rapidjson.org/) - 高性能 JSON 解析库
- [Google Test](https://github.com/google/googletest) - Google C++ 测试框架
- [OpenRTB 2.5 规范](https://www.iab.com/guidelines/openrtb-2-5-spec-release/) - 广告竞价协议

---

<div align="center">

**⭐ 如果这个项目对你有帮助，请给个 Star！**

</div>
