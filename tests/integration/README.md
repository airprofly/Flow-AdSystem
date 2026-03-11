# OpenRTB + 索引模块集成测试

## 概述

本测试目录包含 **OpenRTB 协议解析**（Step 2）和**广告索引召回**（Step 3）的集成测试，展示如何将两个模块组合使用，实现完整的广告投放流程。

## 测试文件

```
tests/integration/
└── integration_test_openrtb_indexing.cpp    # 集成测试代码
```

## 测试内容

### 1. 完整工作流程测试 (`CompleteWorkflow`)

展示完整的广告投放流程：

```
OpenRTB JSON 请求 → OpenRTBParser 解析 → AdIndex 召回 → 获取广告详情
```

**测试步骤**：
1. 准备 OpenRTB JSON 请求（包含地域、设备、兴趣等信息）
2. 使用 `OpenRTBParser::parse()` 解析请求
3. 使用 `AdIndex::query()` 进行广告召回
4. 获取召回广告的详细信息并验证

**示例输出**：
```
Step 1: 解析 OpenRTB 请求...
✓ OpenRTB 请求解析成功
  - 请求 ID: req-001
  - 广告位数量: 1
  - 设备类型: Mobile
  - 地理位置: 北京

Step 2: 执行广告召回...
✓ 广告召回完成
  - 总广告数: 4
  - 召回广告数: 1
  - 召回耗时: 0.002 ms
  - 召回率: 25.00%
```

### 2. 多条件定向测试 (`MultiConditionTargeting`)

测试各种定向条件组合：

| 测试用例 | 预期召回 | 说明 |
|---------|---------|------|
| 北京+移动+游戏 | 1 | 精准匹配广告 1 |
| 上海+桌面+购物 | 1 | 精准匹配广告 2 |
| 广州+平板+学习 | 1 | 精准匹配广告 3 |
| 深圳+平板+理财 | 1 | 精准匹配广告 4 |
| 北京+桌面 | 1 | 匹配支持所有设备的广告 3 |
| 不存在的城市 | 0 | 无匹配广告 |

### 3. 性能压力测试 (`PerformanceStressTest`)

执行 1000 次查询测试，验证系统性能：

**性能指标**：
- 平均耗时: < 0.001 ms
- QPS: > 2,000,000
- 最大耗时: < 0.01 ms

### 4. 边界条件测试 (`EdgeCases`)

测试各种边界情况：

- 空兴趣列表
- 无效设备类型
- 无地域信息

## 运行测试

### 编译

```bash
cd build
cmake --build . --target integration_test_openrtb_indexing
```

### 运行

```bash
# 直接运行
./build/bin/integration_test_openrtb_indexing

# 使用 CTest
ctest -R integration_test_openrtb_indexing

# 查看详细输出
./build/bin/integration_test_openrtb_indexing --gtest_verbose
```

## 代码示例

### 基本使用流程

```cpp
#include "core/openrtb/openrtb_parser.h"
#include "core/indexing/ad_index.hpp"
#include "data/models/ad_model.h"

using namespace flow_ad;
using namespace flow_ad::openrtb;
using namespace flow_ad::indexing;

// 1. 创建解析器和索引
OpenRTBParser parser;
AdIndex index;

// 2. 添加广告到索引
Ad ad;
ad.id = 1;
ad.targeting_geo = {"北京", "上海"};
ad.targeting_devices = {DeviceType::MOBILE};
ad.targeting_interests = {"游戏"};
index.add_ad(ad.id, ad);

// 3. 准备 OpenRTB JSON 请求
std::string openrtb_json = R"({
    "id": "req-001",
    "imp": [{"id": "imp-1", "banner": {"w": [300], "h": [250]}}],
    "device": {
        "geo": {"city": "北京"},
        "devicetype": 1
    },
    "user": {
        "id": "user-001",
        "keywords": ["游戏"]
    }
})";

// 4. 解析 OpenRTB 请求
auto parse_result = parser.parse(openrtb_json);
if (!parse_result.success) {
    std::cerr << "解析失败: " << parse_result.error_message << std::endl;
    return;
}

// 5. 执行广告召回
QueryResult query_result = index.query(parse_result.request);

// 6. 处理召回结果
std::cout << "召回广告数量: " << query_result.candidate_ad_ids.size() << std::endl;
std::cout << "召回耗时: " << query_result.recall_time_ms << " ms" << std::endl;

for (uint64_t ad_id : query_result.candidate_ad_ids) {
    const Ad* ad_ptr = index.get_ad(ad_id);
    if (ad_ptr) {
        std::cout << "广告 ID: " << ad_ptr->id << ", 出价: " << ad_ptr->bid_price << std::endl;
    }
}
```

## 关键点说明

### DeviceType 映射

OpenRTB 2.5 规范的 DeviceType 与内部 Ad 模型的 DeviceType 不同：

| OpenRTB DeviceType | 值 | Ad DeviceType | 值 |
|-------------------|---|---------------|---|
| MOBILE | 1 | MOBILE | 0 |
| PERSONAL_COMPUTER | 2 | DESKTOP | 1 |
| TABLET | 5 | TABLET | 2 |

索引模块会自动处理这个映射转换。

### 查询逻辑

1. **地域过滤（AND 条件）**: 必须匹配广告定向的任一城市
2. **设备过滤（AND 条件）**: 必须匹配广告定向的任一设备类型
3. **兴趣过滤（OR 条件）**: 满足广告定向的任一兴趣即可

### 性能优化

- 使用 BitMap 进行高效的集合运算
- CPU 指令加速（`__builtin_popcountll`）
- 零拷贝设计，避免不必要的内存分配
- 预分配内存，避免动态扩容

## 测试结果

```
[==========] Running 4 tests from 1 test suite.
[----------] 4 tests from OpenRTBIndexingIntegrationTest
[ RUN      ] OpenRTBIndexingIntegrationTest.CompleteWorkflow
[       OK ] OpenRTBIndexingIntegrationTest.CompleteWorkflow (0 ms)
[ RUN      ] OpenRTBIndexingIntegrationTest.MultiConditionTargeting
[       OK ] OpenRTBIndexingIntegrationTest.MultiConditionTargeting (0 ms)
[ RUN      ] OpenRTBIndexingIntegrationTest.PerformanceStressTest
[       OK ] OpenRTBIndexingIntegrationTest.PerformanceStressTest (2 ms)
[ RUN      ] OpenRTBIndexingIntegrationTest.EdgeCases
[       OK ] OpenRTBIndexingIntegrationTest.EdgeCases (0 ms)
[----------] 4 tests from 1 test suite ran. (3 ms total)
[  PASSED  ] 4 tests.
```

## 相关文档

- [OpenRTB 解析器文档](../../core/openrtb/README.md)
- [广告索引模块文档](../../core/indexing/README.md)
- [项目主 README](../../README.md)

## 作者

airprofly

## 更新日志

- **2026-03-11**: 初始版本，实现 OpenRTB + 索引模块集成测试
- **2026-03-11**: 新增核心模块集成测试 (索引 + 频控 + Pacing)

---

## 核心模块集成测试

### 文件
`integration_test_index_frequency_pacing.cpp`

### 概述
测试**广告索引**、**频次控制**和**预算Pacing**三个核心模块的协同工作。

### 测试用例

| 测试用例 | 说明 |
|---------|------|
| `ModuleInitialization` | 验证所有模块正确初始化 |
| `AdRetrieval` | 测试广告检索功能 |
| `FrequencyAndPacingCoordination` | 测试频控和 Pacing 的协同工作 |
| `ConcurrencyStressTest` | 并发压力测试 (10线程 × 100请求) |
| `PerformanceBenchmark` | 完整流程性能测试 |
| `MemoryUsage` | 内存使用测试 |

### 性能指标

```
完整流程 QPS: 1,111,111
总内存占用: 5 KB (10个广告)
平均延迟: < 1 μs
```

### 运行

```bash
# 编译
cd build
cmake --build . --target core_modules_integration_test

# 运行
./bin/core_modules_integration_test

# 使用 CTest
ctest -R core_modules_integration_test
```

### 代码示例

```cpp
#include "ad_index.hpp"
#include "frequency_manager.hpp"
#include "pacing_manager.hpp"
#include "async_impression_logger.hpp"
#include "async_pacing_updater.hpp"

// 1. 创建模块
auto index = std::make_unique<AdIndex>();
auto frequency_manager = std::make_shared<FrequencyManager>();
auto pacing_manager = std::make_shared<PacingManager>();

// 2. 创建异步记录器
AsyncImpressionLogger frequency_logger(frequency_manager, 10000, 100, 100);
AsyncPacingUpdater pacing_updater(pacing_manager, 10000, 100, 100);

// 3. 添加广告
Ad ad;
ad.id = 1001;
ad.campaign_id = 201;
index->add_ad(ad.id, ad);

// 4. 配置预算
BudgetConfig config;
config.campaign_id = 201;
config.algorithm = PacingAlgorithm::TOKEN_BUCKET;
config.target_rate = 0.1;
pacing_manager->update_campaign_config(201, config);

// 5. 模拟投放
uint64_t user_id = 12345;
uint64_t ad_id = 1001;

FrequencyCap cap;
cap.hourly_limit = 5;

if (!frequency_manager->is_capped(user_id, ad_id, cap)) {
    const Ad* ad = index->get_ad(ad_id);
    if (ad && pacing_manager->allow_impression(ad->campaign_id, ad->bid_price)) {
        // 允许投放
        frequency_logger.record_impression(user_id, ad_id, TimeWindow::HOUR);
        pacing_updater.record_impression(ad->campaign_id, ad->bid_price);
    }
}
```
