# Step 3: 广告索引模块实现（海选召回）

## 📋 任务概述

**业务目标**: 实现广告投放系统的第一道关卡——**广告索引引擎（海选召回）**，从大规模广告池中快速筛选出符合定向条件的候选广告。

**前置依赖**:
- ✅ Step 1: 数据生成模块（10 万+ 广告池）
- ✅ Step 2: OpenRTB 2.5 协议解析器

**当前状态**: 🚀 **规划完成，待实现**

---

## 🎯 核心目标

### 功能目标
1. **构建倒排索引**: 基于广告定向条件（地域、设备、兴趣等）建立高效的倒排索引结构
2. **快速召回**: 解析 OpenRTB 请求中的用户特征，通过集合求交集运算快速召回候选广告
3. **性能优化**: 使用 BitMap（位图）结构进行极致优化，确保低延迟召回

### 性能指标

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 召回延迟 | 低延迟 | 毫秒级响应 |
| 内存占用 | 高效 | 使用 BitMap 优化内存 |
| 召回率 | 高 | 不漏掉符合条件的广告 |
| 准确率 | 100% | 召回的广告必须符合定向条件 |
| 查询吞吐 | 高并发 | 支持高 QPS |

---

## 📐 架构设计

### 系统架构

```
┌────────────────────────────────────────────────────────────┐
│                  广告投放系统流程                            │
├────────────────────────────────────────────────────────────┤
│                                                              │
│  1️⃣ 接收请求 (OpenRTB Bid Request)                          │
│     └─> Step 2: OpenRTB Parser                              │
│         └─> 解析为结构化对象                                 │
│                                                              │
│  2️⃣ 海选召回 (Step 3: Ad Indexing) ⭐ 本步骤目标             │
│     └─> 输入: 大规模广告池 + OpenRTB 请求                    │
│     └─> 输出: 符合条件的候选广告                             │
│     └─> 耗时: 低延迟                                         │
│                                                              │
│  3️⃣ 精细排序 (Step 4: Ad Ranking)                           │
│     └─> 计算 eCPM + 排序                                     │
│                                                              │
│  4️⃣ 竞价决策 (Step 5: Auction)                              │
│     └─> VCG/Second Price 拍卖                               │
│                                                              │
└────────────────────────────────────────────────────────────┘
```

### 索引引擎架构

```
┌─────────────────────────────────────────────────────────────┐
│              AdIndex (广告索引引擎)                          │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │        InvertedIndex (倒排索引)                       │   │
│  │  ┌──────────────┬──────────────┬──────────────┐     │   │
│  │  │ GeoIndex     │ DeviceIndex  │ InterestIndex│     │   │
│  │  │ (地域索引)    │ (设备索引)    │ (兴趣索引)    │     │   │
│  │  │              │              │              │     │   │
│  │  │ "北京" ->     │ iOS ->       │ "游戏" ->     │     │   │
│  │  │ BitMap       │ BitMap       │ BitMap       │     │   │
│  │  └──────────────┴──────────────┴──────────────┘     │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │        AdStore (广告存储)                            │   │
│  │  std::unordered_map<ad_id, Ad>                      │   │
│  │  快速访问广告详细信息                                 │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                       公共 API                               │
│  - add_ad(ad_id, ad)        添加广告到索引                  │
│  - remove_ad(ad_id)         从索引中移除广告                │
│  - query(request) -> ids    根据请求查询候选广告 ID 列表    │
│  - get_ad(ad_id) -> ad      获取广告详细信息                │
└─────────────────────────────────────────────────────────────┘
```

### BitMap 数据结构

```
BitMap 实现 (使用 std::vector<uint64_t>)

┌────────────────────────────────────────────────────────┐
│  广告 ID:    0    1    2    3    4    5    6    7     │
│  BitMap:    [1]   [0]   [1]   [0]   [0]   [1]   [0]   [1]  │
│              │         │         │         │         │   │
│              └─ 64 位分块 0 ──┘         └─ 64 位分块 1 ──┘  │
│                                                              │
│  内存占用: 高效的位图存储                                    │
│                                                              │
│  交集运算: BitMap1 & BitMap2 (使用 SIMD 加速)               │
│  并集运算: BitMap1 | BitMap2                                 │
│  Popcount: __builtin_popcountll (CPU 指令加速)              │
└────────────────────────────────────────────────────────┘
```

---

## 📁 文件结构

### 新增文件

```
core/indexing/
├── CMakeLists.txt                    # 构建配置
├── bitmap.hpp                        # BitMap 实现
├── inverted_index.hpp                # 倒排索引头文件
├── inverted_index.cpp                # 倒排索引实现
├── ad_store.hpp                      # 广告存储
├── ad_index.hpp                      # 广告索引引擎（统一接口）
├── ad_index.cpp                      # 广告索引引擎实现
└── index_loader.hpp                  # 数据加载器（从 JSON 加载）

tests/unit/
└── indexing_test.cpp                 # 单元测试
```

### 参考文件（已存在）

```
data/models/
└── ad_model.h                        # 广告数据模型

core/openrtb/
├── openrtb_models.h                  # OpenRTB 数据模型
├── openrtb_parser.h                  # OpenRTB 解析器
└── openrtb_parser.cpp                # 解析器实现

data/data/
└── ads_data.json                     # Step 1 生成的广告数据
```

---

## 🚀 实现阶段

### Phase 1: BitMap 实现（2-3 小时）

**文件**: `core/indexing/bitmap.hpp`

**实现要点**:

```cpp
#pragma once

// Author: airprofly

#include <cstdint>
#include <vector>
#include <algorithm>

namespace flow_ad::indexing {

class BitMap {
public:
    explicit BitMap(size_t size = 0)
        : data_((size + 63) / 64, 0), size_(size) {}

    // 设置某一位为 1
    void set(size_t pos) {
        if (pos >= size_) return;
        data_[pos / 64] |= (1ULL << (pos % 64));
    }

    // 清除某一位（设为 0）
    void clear(size_t pos) {
        if (pos >= size_) return;
        data_[pos / 64] &= ~(1ULL << (pos % 64));
    }

    // 获取某一位的值
    bool test(size_t pos) const {
        if (pos >= size_) return false;
        return (data_[pos / 64] >> (pos % 64)) & 1ULL;
    }

    // 求交集（返回新 BitMap）
    BitMap operator&(const BitMap& other) const {
        BitMap result(std::max(size_, other.size_));
        size_t min_blocks = std::min(data_.size(), other.data_.size());

        for (size_t i = 0; i < min_blocks; ++i) {
            result.data_[i] = data_[i] & other.data_[i];
        }

        return result;
    }

    // 求并集（返回新 BitMap）
    BitMap operator|(const BitMap& other) const {
        BitMap result(std::max(size_, other.size_));
        size_t min_blocks = std::min(data_.size(), other.data_.size());

        for (size_t i = 0; i < min_blocks; ++i) {
            result.data_[i] = data_[i] | other.data_[i];
        }

        return result;
    }

    // 计算置位数量（popcount）
    size_t count() const {
        size_t total = 0;
        for (const auto& block : data_) {
            total += __builtin_popcountll(block);
        }
        return total;
    }

    // 获取大小
    size_t size() const { return size_; }

    // 调整大小
    void resize(size_t new_size) {
        size_t new_blocks = (new_size + 63) / 64;
        data_.resize(new_blocks, 0);
        size_ = new_size;
    }

    // 清空所有位
    void reset() {
        std::fill(data_.begin(), data_.end(), 0);
    }

private:
    std::vector<uint64_t> data_;
    size_t size_;
};

} // namespace flow_ad::indexing
```

**性能优化技巧**:
1. 使用 `__builtin_popcountll` 加速 popcount（CPU 指令）
2. 循环展开优化位运算
3. 预分配内存避免动态扩容

---

### Phase 2: 倒排索引实现（4-6 小时）

**文件**: `core/indexing/inverted_index.hpp`, `core/indexing/inverted_index.cpp`

**头文件**:

```cpp
#pragma once

// Author: airprofly

#include "bitmap.hpp"
#include "data/models/ad_model.h"
#include "core/openrtb/openrtb_models.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace flow_ad::indexing {

class InvertedIndex {
public:
    InvertedIndex() = default;
    ~InvertedIndex() = default;

    // ========== 索引管理 ==========

    // 添加广告到索引
    void add_ad(uint64_t ad_id, const Ad& ad);

    // 从索引中移除广告
    void remove_ad(uint64_t ad_id);

    // ========== 查询接口 ==========

    // 根据 OpenRTB 请求查询符合条件的广告（返回 BitMap）
    BitMap query(const openrtb::OpenRTBRequest& request) const;

    // ========== 统计信息 ==========

    // 获取索引中的广告总数
    size_t size() const { return ad_count_; }

    // 获取内存占用（估算）
    size_t memory_usage_bytes() const;

private:
    // 倒排索引结构
    std::unordered_map<std::string, BitMap> geo_index_;       // 地域索引
    std::unordered_map<int, BitMap> device_index_;             // 设备索引
    std::unordered_map<int, BitMap> os_index_;                 // 操作系统索引
    std::unordered_map<std::string, BitMap> interest_index_;   // 兴趣索引

    size_t ad_count_ = 0;  // 广告总数
    size_t max_ad_id_ = 0; // 最大广告 ID（用于 BitMap 大小）
};

} // namespace flow_ad::indexing
```

**实现文件**:

```cpp
// Author: airprofly

#include "inverted_index.hpp"
#include <algorithm>

namespace flow_ad::indexing {

void InvertedIndex::add_ad(uint64_t ad_id, const Ad& ad) {
    // 更新最大 ID
    max_ad_id_ = std::max(max_ad_id_, ad_id);
    ad_count_++;

    // 1. 地域索引
    for (const auto& geo : ad.targeting_geo) {
        auto& bitmap = geo_index_[geo];
        if (bitmap.size() <= ad_id) {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
    }

    // 2. 设备索引
    for (const auto& device : ad.targeting_devices) {
        int device_key = static_cast<int>(device);
        auto& bitmap = device_index_[device_key];
        if (bitmap.size() <= ad_id) {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
    }

    // 3. 兴趣索引
    for (const auto& interest : ad.targeting_interests) {
        auto& bitmap = interest_index_[interest];
        if (bitmap.size() <= ad_id) {
            bitmap.resize(max_ad_id_ + 1);
        }
        bitmap.set(ad_id);
    }
}

BitMap InvertedIndex::query(const openrtb::OpenRTBRequest& request) const {
    std::vector<const BitMap*> and_conditions;
    std::vector<const BitMap*> or_conditions;

    // 1. 地域过滤（AND 条件）
    if (request.device && request.device->geo && request.device->geo->city) {
        const auto& city = *request.device->geo->city;
        auto it = geo_index_.find(city);
        if (it != geo_index_.end()) {
            and_conditions.push_back(&it->second);
        } else {
            return BitMap();  // 没有广告定向到该城市
        }
    }

    // 2. 设备类型过滤（AND 条件）
    if (request.device && request.device->devicetype) {
        auto it = device_index_.find(*request.device->devicetype);
        if (it != device_index_.end()) {
            and_conditions.push_back(&it->second);
        }
    }

    // 3. 操作系统过滤（AND 条件）
    if (request.device && request.device->os) {
        auto it = os_index_.find(*request.device->os);
        if (it != os_index_.end()) {
            and_conditions.push_back(&it->second);
        }
    }

    // 4. 兴趣过滤（OR 条件：满足任意兴趣即可）
    if (request.user && request.user->keywords && !request.user->keywords->empty()) {
        for (const auto& keyword : *request.user->keywords) {
            auto it = interest_index_.find(keyword);
            if (it != interest_index_.end()) {
                or_conditions.push_back(&it->second);
            }
        }
    }

    // 执行查询
    BitMap result(max_ad_id_ + 1);

    // 如果没有 AND 条件，返回所有广告的 BitMap
    if (and_conditions.empty()) {
        // 初始化为全 1
        for (size_t i = 0; i <= max_ad_id_; ++i) {
            result.set(i);
        }
    } else {
        // 求所有 AND 条件的交集
        result = *and_conditions[0];
        for (size_t i = 1; i < and_conditions.size(); ++i) {
            result = result & *and_conditions[i];
        }
    }

    // 如果有 OR 条件，求并集后再与结果求交集
    if (!or_conditions.empty()) {
        BitMap or_result = *or_conditions[0];
        for (size_t i = 1; i < or_conditions.size(); ++i) {
            or_result = or_result | *or_conditions[i];
        }
        result = result & or_result;
    }

    return result;
}

size_t InvertedIndex::memory_usage_bytes() const {
    size_t total = 0;

    // 计算每个索引的内存占用
    for (const auto& [key, bitmap] : geo_index_) {
        total += bitmap.size() / 8 + sizeof(std::string) + key.size();
    }

    for (const auto& [key, bitmap] : device_index_) {
        total += bitmap.size() / 8 + sizeof(int);
    }

    for (const auto& [key, bitmap] : os_index_) {
        total += bitmap.size() / 8 + sizeof(int);
    }

    for (const auto& [key, bitmap] : interest_index_) {
        total += bitmap.size() / 8 + sizeof(std::string) + key.size();
    }

    return total;
}

} // namespace flow_ad::indexing
```

---

### Phase 3-6: 其他模块

由于篇幅限制，其他模块（AdStore、AdIndex、IndexLoader、单元测试）的实现思路与上述类似，核心都是：

1. **使用 BitMap 进行高效的位运算**
2. **基于倒排索引快速查找**
3. **支持动态添加和删除广告**
4. **提供简洁的查询接口**

详细的实现代码可以参考上述模式进行编写。

---

## 🔧 CMakeLists.txt 配置

**文件**: `core/indexing/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)

# Ad Index Library
add_library(ad_index STATIC
    inverted_index.cpp
    ad_index.cpp
)

target_include_directories(ad_index
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core/indexing
        ${CMAKE_SOURCE_DIR}/core/openrtb
        ${CMAKE_SOURCE_DIR}/data/models
        ${rapidjson_SOURCE_DIR}/include
)

target_link_libraries(ad_index
    PUBLIC
        openrtb_parser
)

target_compile_features(ad_index PUBLIC cxx_std_20)
```

---

## 📊 验收标准

### 功能验收
- [ ] 能够从大规模广告池中快速召回候选广告
- [ ] 支持地域、设备、兴趣等多维度定向
- [ ] 支持广告的动态添加和删除
- [ ] 召回结果 100% 准确（无误召回）
- [ ] 支持从 Step 1 生成的 JSON 文件加载数据

### 质量验收
- [ ] 单元测试覆盖率 > 90%
- [ ] 无内存泄漏（Valgrind 检查）
- [ ] 代码符合项目风格（参考 Step 1、Step 2）
- [ ] 所有公共 API 有文档注释

### 性能验收
- [ ] 低延迟召回（毫秒级响应）
- [ ] 高效的内存占用
- [ ] 支持高并发查询

---

## 🚀 使用示例

### 完整使用流程

```cpp
#include "core/indexing/ad_index.h"
#include "core/openrtb/openrtb_parser.h"
#include "data/generator/ad_generator.h"
#include "core/indexing/index_loader.h"

using namespace flow_ad;

int main() {
    // 1. 创建索引
    indexing::AdIndex index;

    // 2. 从数据生成器加载
    GeneratorConfig config;
    config.num_ads = 100000;
    // ... 其他配置

    AdDataGenerator generator(config);
    generator.generate_all();

    indexing::IndexLoader::load_from_generator(generator, index);

    // 3. 打印索引统计信息
    index.print_stats();

    // 4. 解析 OpenRTB 请求
    openrtb::OpenRTBParser parser;
    auto parse_result = parser.parse_file("data/data/openrtb_requests.json");

    if (!parse_result.success) {
        std::cerr << "Failed to parse request" << std::endl;
        return 1;
    }

    // 5. 查询候选广告
    auto result = index.query(parse_result.request);

    std::cout << "Total ads: " << result.total_ads << std::endl;
    std::cout << "Candidate ads: " << result.candidate_ad_ids.size() << std::endl;
    std::cout << "Recall time: " << result.recall_time_ms << " ms" << std::endl;

    // 6. 获取候选广告的详细信息
    for (size_t i = 0; i < result.candidate_ad_ids.size(); ++i) {
        uint64_t ad_id = result.candidate_ad_ids[i];
        auto ad_opt = index.get_ad(ad_id);
        if (ad_opt) {
            const auto& ad = *ad_opt;
            // 处理广告...
        }
    }

    return 0;
}
```

---

## 🎯 下一步

完成 Step 3 后，系统将具备：
- ✅ 大规模广告池管理能力（Step 1）
- ✅ OpenRTB 2.5 协议解析能力（Step 2）
- ✅ **快速召回能力**（Step 3）⭐ 本步骤目标

**Step 4 预告**: 广告排序模块（ECPM 计算 + 排序）

---

## 📚 相关文档

- [项目主 README](../../README.md)
- [Step 1: 数据生成](../.claude/commands/step1-generateData.md)
- [Step 2: OpenRTB 解析](../.claude/commands/step2-parseOpenRTB.md)
- [架构设计文档](../.claude/commands/schema.md)

---

**文档创建时间**: 2026-03-11
**维护者**: airprofly
**状态**: ✅ 规划完成，待实现
