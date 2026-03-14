# 🧪 测试说明

本目录包含 Flow-AdSystem 的所有 GTest 单元测试和集成测试。

## 目录结构

```
tests/
├── unit/                              # 单元测试
│   ├── openrtb_parser_test.cpp        # OpenRTB 协议解析测试
│   ├── openrtb_response_test.cpp      # OpenRTB 响应构建测试
│   ├── indexing_test.cpp              # 广告索引 + BitMap 测试
│   ├── frequency_test.cpp             # 频次控制测试
│   ├── test_pacing.cpp                # Pacing 匀速消费测试
│   ├── ranker_test.cpp                # eCPM 排序测试
│   ├── ctr_test.cpp                   # CTR 预估单元测试
│   ├── ctr_e2e_test.cpp               # CTR 端到端测试（需要 ONNX 模型）
│   └── ctr_latency_benchmark.cpp      # CTR 推理延迟基准测试
└── integration/                       # 集成测试
    ├── integration_test_index_frequency_pacing.cpp   # 索引+频控+Pacing 联调
    └── integration_test_openrtb_indexing.cpp         # OpenRTB+索引 联调
```

## 运行测试

```bash
# 构建（需先完成 cmake 配置）
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# 运行所有测试
cd build
ctest --output-on-failure --parallel $(nproc)

# 按模块过滤
ctest -R "OpenRTB"    --output-on-failure  # OpenRTB 相关
ctest -R "Indexing"   --output-on-failure  # 广告索引
ctest -R "Frequency"  --output-on-failure  # 频次控制
ctest -R "Pacing"     --output-on-failure  # 匀速消费
ctest -R "CTR"        --output-on-failure  # CTR 预估
ctest -R "Ranker"     --output-on-failure  # eCPM 排序
ctest -R "Integration" --output-on-failure # 集成测试
```

## 测试覆盖

| 模块 | 测试文件 | 测试内容 |
|------|----------|----------|
| **OpenRTB 解析** | `openrtb_parser_test.cpp` | 合法/非法 JSON、必填字段、枚举解析 |
| **OpenRTB 响应** | `openrtb_response_test.cpp` | 一价/二价拍卖、Floor Price、No Bid |
| **广告索引** | `indexing_test.cpp` | BitMap 操作、倒排索引、多维度定向 |
| **频次控制** | `frequency_test.cpp` | 时间窗口频控、并发安全、异步日志 |
| **Pacing** | `test_pacing.cpp` | 令牌桶算法、预算消耗、并发压测 |
| **eCPM 排序** | `ranker_test.cpp` | eCPM 计算、TopK 选择、多策略排序 |
| **CTR 预估** | `ctr_test.cpp` / `ctr_e2e_test.cpp` | 特征提取、ONNX 推理、端到端流程 |
| **集成** | `integration_test_*.cpp` | 多模块协同工作、全流程验证 |

## 注意事项

- `ctr_e2e_test` 和 `ctr_latency_benchmark` 需要 `models/deep_fm.onnx` 存在，若模型不存在会自动跳过
- 集成测试会启动异步线程，`TearDown` 含 200ms 等待时间
- 所有测试通过 `FLOW_ADSYSTEM_SOURCE_DIR` 宏定位项目根目录（由 CMake 注入）
