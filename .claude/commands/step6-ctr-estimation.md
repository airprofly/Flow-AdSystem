# Step 6: CTR 预估模块 - 离线训练 + 在线预估

## 🎯 核心设计原则

### 问题本质
**点击率(CTR)预估问题**
- 广告池: 10万+ 广告创意
- 流量规模: 日均 10亿+ 请求
- 特征维度: 100+ 维特征(用户特征、广告特征、上下文特征)
- 延迟要求: p99 < 5ms(在线预估)
- 准确性要求: AUC > 0.75

### 核心挑战
1. **高并发低延迟**: 在线预估需要在 5ms 内完成复杂模型推理
2. **模型复杂度**: 需要深度学习模型(如 DeepFM、Wide&Deep)才能达到准确率要求
3. **特征工程**: 需要处理稀疏特征、连续特征、序列特征等
4. **模型更新**: 需要支持模型热更新,不中断服务
5. **A/B 测试**: 需要支持多模型同时在线,便于流量分流实验

---

## 🏗️ 生产级架构设计

### 1. 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      离线训练 pipeline                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  数据采集    │→ │  特征工程    │→ │  模型训练    │          │
│  │  (日志)      │  │  (特征库)    │  │  (TensorFlow)│          │
│  └──────────────┘  └──────────────┘  └──────┬───────┘          │
│                                              │                  │
│                                         ┌────▼───────┐          │
│                                         │  模型评估   │          │
│                                         │  (AUC/LogLoss)│        │
│                                         └────┬───────┘          │
│                                              │                  │
│                                         ┌────▼───────┐          │
│                                         │  模型导出   │          │
│                                         │  (PMML/ONNX)│         │
│                                         └────┬───────┘          │
└──────────────────────────────────────────────┼──────────────────┘
                                               │ 模型文件
                                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                        模型存储                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │  模型版本库  │  │  特征配置    │  │  实验配置    │          │
│  │  (S3/HDFS)   │  │  (Feature Spec)│ │  (AB Test)   │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
                                               │
                                               ▼ 模型加载
┌─────────────────────────────────────────────────────────────────┐
│                    在线预估服务 (C++)                            │
│  ┌──────────────────────────────────────────────────┐           │
│  │              特征提取引擎                         │           │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐         │           │
│  │  │用户特征 │  │广告特征 │  │上下文   │         │           │
│  │  │提取器   │  │提取器   │  │特征     │         │           │
│  │  └────┬────┘  └────┬────┘  └────┬────┘         │           │
│  │       │            │            │               │           │
│  │  ┌────▼─────────────▼────────────▼──┐           │           │
│  │  │      特征拼接 & 归一化            │           │           │
│  │  └──────────────┬───────────────────┘           │           │
│  └─────────────────┼───────────────────────────────┘           │
│                    │                                           │
│  ┌─────────────────▼───────────────────────────────┐           │
│  │              模型推理引擎                         │           │
│  │  ┌──────────────────────────────────────────┐   │           │
│  │  │  模型缓存 (LRU Cache)                    │   │           │
│  │  │  ├─ Model v1 (20% 流量)                  │   │           │
│  │  │  ├─ Model v2 (30% 流量)                  │   │           │
│  │  │  └─ Model v3 (50% 流量)  ⭐ 主模型        │   │           │
│  │  └───────────────┬──────────────────────────┘   │           │
│  │                  │                               │           │
│  │  ┌───────────────▼──────────────────────────┐   │           │
│  │  │  推理引擎 (ONNX Runtime / LibTorch)      │   │           │
│  │  │  - 批处理优化                            │   │           │
│  │  │  - SIMD 加速                             │   │           │
│  │  │  - 多线程推理                            │   │           │
│  │  └───────────────┬──────────────────────────┘   │           │
│  └──────────────────┼───────────────────────────────┘           │
│                     │                                           │
│  ┌──────────────────▼───────────────────────────────┐           │
│  │              后处理 & 输出                         │           │
│  │  - CTR → pCVR 转换                               │           │
│  │  - 置信度计算                                     │           │
│  │  - 模型解释 (SHAP)                               │           │
│  └───────────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────────┐
│                    广告投放服务                                   │
│  预估 CTR → 计算 eCPM = CTR × 出价 → 排序 → 返回广告             │
└─────────────────────────────────────────────────────────────────┘
```

---

## 💾 核心数据结构设计

### 1. 特征定义 (FeatureSpec)

```cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>

namespace flow_ad {
namespace ctr {

// 特征类型
enum class FeatureType : uint32_t {
    DENSE_FLOAT = 0,    // 连续特征(浮点数)
    SPARSE_INT = 1,     // 稀疏特征(整数 ID)
    EMBEDDING = 2,      // 嵌入特征(向量)
    ONE_HOT = 3,        // One-Hot 编码特征
    BUCKET = 4,         // 分桶特征
    SEQUENCE = 5        // 序列特征
};

// 特征配置
struct FeatureConfig {
    std::string name;           // 特征名称
    FeatureType type;           // 特征类型
    uint32_t dimension;         // 特征维度
    std::optional<double> min_value;  // 最小值(用于归一化)
    std::optional<double> max_value;  // 最大值(用于归一化)
    std::vector<std::string> bucket_boundaries;  // 分桶边界

    // 嵌入表配置(仅用于 EMBEDDING 类型)
    uint32_t embedding_dim;     // 嵌入维度
    std::string embedding_table_name;  // 嵌入表名称
};

// 特征组(用于特征交叉)
struct FeatureGroup {
    std::string name;
    std::vector<std::string> feature_names;  // 包含的特征
    std::string interaction_type;  // 交叉类型: "add", "multiply", "concat"
};

// 特征规范(整个模型的特征配置)
class FeatureSpec {
public:
    FeatureSpec() = default;

    // 添加特征
    void add_feature(const FeatureConfig& config) {
        features_[config.name] = config;
        feature_order_.push_back(config.name);
    }

    // 添加特征组
    void add_feature_group(const FeatureGroup& group) {
        feature_groups_.push_back(group);
    }

    // 获取特征配置
    const FeatureConfig* get_feature(const std::string& name) const {
        auto it = features_.find(name);
        return it != features_.end() ? &it->second : nullptr;
    }

    // 获取所有特征(按顺序)
    const std::vector<std::string>& get_feature_order() const {
        return feature_order_;
    }

    // 获取所有特征组
    const std::vector<FeatureGroup>& get_feature_groups() const {
        return feature_groups_;
    }

    // 序列化/反序列化(用于加载配置)
    std::string serialize() const;
    static FeatureSpec deserialize(const std::string& data);

private:
    std::unordered_map<std::string, FeatureConfig> features_;
    std::vector<std::string> feature_order_;
    std::vector<FeatureGroup> feature_groups_;
};

} // namespace ctr
} // namespace flow_ad
```

---

### 2. 特征提取上下文 (FeatureContext)

```cpp
#pragma once
#include "feature_spec.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <variant>

namespace flow_ad {
namespace ctr {

// 特征值类型(使用 std::variant 支持多种类型)
using FeatureValue = std::variant<
    float,                    // 连续特征
    int64_t,                  // 稀疏特征 ID
    std::vector<float>,       // 嵌入向量
    std::vector<int64_t>      // 序列特征
>;

// 特征提取上下文
class FeatureContext {
public:
    FeatureContext() = default;

    // 设置特征值
    void set(const std::string& name, FeatureValue value) {
        features_[name] = std::move(value);
    }

    // 获取特征值
    template<typename T>
    std::optional<T> get(const std::string& name) const {
        auto it = features_.find(name);
        if (it == features_.end()) {
            return std::nullopt;
        }

        if (std::holds_alternative<T>(it->second)) {
            return std::get<T>(it->second);
        }
        return std::nullopt;
    }

    // 检查特征是否存在
    bool has(const std::string& name) const {
        return features_.find(name) != features_.end();
    }

    // 清空所有特征
    void clear() {
        features_.clear();
    }

    // 获取所有特征名称
    std::vector<std::string> get_feature_names() const {
        std::vector<std::string> names;
        names.reserve(features_.size());
        for (const auto& [name, _] : features_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::unordered_map<std::string, FeatureValue> features_;
};

} // namespace ctr
} // namespace flow_ad
```

---

### 3. 特征提取器接口 (FeatureExtractor)

```cpp
#pragma once
#include "feature_context.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace flow_ad {
namespace ctr {

// OpenRTB 请求的前向声明
namespace openrtb {
struct BidRequest;
struct BidRequest_User;
struct BidRequest_Imp;
} // namespace openrtb

// 特征提取器接口
class FeatureExtractor {
public:
    virtual ~FeatureExtractor() = default;

    /**
     * 从 OpenRTB 请求中提取特征
     * @param request OpenRTB 竞价请求
     * @param imp 广告展示位
     * @param context 输出: 特征上下文
     */
    virtual void extract(
        const openrtb::BidRequest& request,
        const openrtb::BidRequest_Imp& imp,
        FeatureContext& context
    ) = 0;

    /**
     * 获取特征名称
     */
    virtual std::string get_name() const = 0;
};

// 特征提取器工厂
class FeatureExtractorFactory {
public:
    using Creator = std::function<std::unique_ptr<FeatureExtractor>()>;

    // 注册特征提取器
    void register_extractor(const std::string& name, Creator creator) {
        creators_[name] = std::move(creator);
    }

    // 创建特征提取器
    std::unique_ptr<FeatureExtractor> create(const std::string& name) const {
        auto it = creators_.find(name);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    // 获取所有已注册的提取器名称
    std::vector<std::string> get_registered_names() const {
        std::vector<std::string> names;
        names.reserve(creators_.size());
        for (const auto& [name, _] : creators_) {
            names.push_back(name);
        }
        return names;
    }

private:
    std::unordered_map<std::string, Creator> creators_;
};

} // namespace ctr
} // namespace flow_ad
```

---

### 4. 模型配置 (ModelConfig)

```cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace flow_ad {
namespace ctr {

// 模型类型
enum class ModelType : uint32_t {
    LR = 0,              // 逻辑回归
    FM = 1,              // 因子分解机
    DEEP_FM = 2,         // DeepFM
    WIDE_DEEP = 3,       // Wide & Deep
    DIN = 4,             // Deep Interest Network
    AUTO_INT = 5         // AutoInt
};

// 模型配置
struct ModelConfig {
    // 基本信息
    std::string model_name;          // 模型名称
    std::string model_version;       // 模型版本(如 "v1.0.0")
    ModelType model_type;            // 模型类型
    std::string model_path;          // 模型文件路径(ONNX/PB)

    // 特征配置
    std::string feature_spec_path;   // 特征规范文件路径

    // 流量分配(A/B 测试)
    double traffic_fraction;         // 流量比例(0-1)

    // 性能配置
    uint32_t thread_pool_size;       // 推理线程池大小
    uint32_t batch_size;             // 批处理大小
    bool enable_simd;                // 是否启用 SIMD 加速

    // 缓存配置
    uint32_t feature_cache_size;     // 特征缓存大小
    uint32_t embedding_cache_size;   // 嵌入缓存大小

    // 模型元数据
    uint64_t training_samples;       // 训练样本数
    double auc;                      // AUC 指标
    double logloss;                  // LogLoss 指标
    std::chrono::system_clock::time_point training_time;

    // 是否启用
    bool is_enabled() const {
        return !model_name.empty() && !model_path.empty();
    }

    // 序列化
    std::string serialize() const;
    static ModelConfig deserialize(const std::string& data);
};

} // namespace ctr
} // namespace flow_ad
```

---

### 5. 模型推理引擎 (ModelInferenceEngine)

```cpp
#pragma once
#include "model_config.h"
#include "feature_spec.h"
#include "feature_context.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace flow_ad {
namespace ctr {

// 推理结果
struct InferenceResult {
    double ctr;                     // 预估 CTR
    double confidence;              // 置信度
    uint64_t model_version_hash;    // 模型版本哈希(用于监控)
    std::unordered_map<std::string, double> feature_contributions;  // 特征贡献度(用于解释)
};

// 模型推理引擎接口
class ModelInferenceEngine {
public:
    virtual ~ModelInferenceEngine() = default;

    /**
     * 初始化模型
     * @param config 模型配置
     * @param spec 特征规范
     * @return true = 成功; false = 失败
     */
    virtual bool initialize(
        const ModelConfig& config,
        const FeatureSpec& spec
    ) = 0;

    /**
     * 推理(单个样本)
     * @param context 特征上下文
     * @param result 输出: 推理结果
     * @return true = 成功; false = 失败
     */
    virtual bool predict(
        const FeatureContext& context,
        InferenceResult& result
    ) = 0;

    /**
     * 推理(批量)
     * @param contexts 特征上下文列表
     * @param results 输出: 推理结果列表
     * @return true = 成功; false = 失败
     */
    virtual bool predict_batch(
        const std::vector<FeatureContext>& contexts,
        std::vector<InferenceResult>& results
    ) = 0;

    /**
     * 获取模型配置
     */
    virtual const ModelConfig& get_config() const = 0;

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_predictions;
        uint64_t total_batch_predictions;
        double avg_latency_ms;
        double p50_latency_ms;
        double p99_latency_ms;
    };
    virtual Stats get_stats() const = 0;

    /**
     * 重新加载模型(热更新)
     */
    virtual bool reload_model(const std::string& model_path) = 0;
};

} // namespace ctr
} // namespace flow_ad
```

---

## 🔧 具体实现:基于 ONNX Runtime 的推理引擎

### 1. ONNX Runtime 推理引擎实现

```cpp
#pragma once
#include "model_inference_engine.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>

namespace flow_ad {
namespace ctr {

class ONNXRuntimeEngine : public ModelInferenceEngine {
public:
    ONNXRuntimeEngine();
    ~ONNXRuntimeEngine() override;

    // 禁止拷贝和移动
    ONNXRuntimeEngine(const ONNXRuntimeEngine&) = delete;
    ONNXRuntimeEngine& operator=(const ONNXRuntimeEngine&) = delete;

    // 实现 ModelInferenceEngine 接口
    bool initialize(
        const ModelConfig& config,
        const FeatureSpec& spec
    ) override;

    bool predict(
        const FeatureContext& context,
        InferenceResult& result
    ) override;

    bool predict_batch(
        const std::vector<FeatureContext>& contexts,
        std::vector<InferenceResult>& results
    ) override;

    const ModelConfig& get_config() const override {
        return config_;
    }

    Stats get_stats() const override;

    bool reload_model(const std::string& model_path) override;

private:
    // ONNX Runtime 对象
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::MemoryInfo> memory_info_;

    // 模型配置和特征规范
    ModelConfig config_;
    FeatureSpec feature_spec_;

    // 输入输出名称
    std::vector<const char*> input_names_;
    std::vector<const char*> output_names_;

    // 特征到模型输入的映射
    struct InputMapping {
        std::string feature_name;
        std::string input_name;
        std::vector<int64_t> shape;
        ONNXTensorElementDataType type;
    };
    std::vector<InputMapping> input_mappings_;

    // 统计信息
    mutable std::mutex stats_mutex_;
    std::atomic<uint64_t> total_predictions_{0};
    std::atomic<uint64_t> total_batch_predictions_{0};
    std::vector<double> latencies_;  // 延迟样本(用于计算分位数)

    // 辅助方法
    bool build_input_mappings();
    bool prepare_input_tensor(
        const FeatureContext& context,
        const InputMapping& mapping,
        std::vector<float>& tensor_data
    );

    void record_latency(double latency_ms);
    Stats calculate_stats() const;
};

} // namespace ctr
} // namespace flow_ad
```

---

### 2. ONNX Runtime 推理引擎实现

```cpp
#include "onnx_runtime_engine.h"
#include <iostream>
#include <fstream>
#include <numeric>

namespace flow_ad {
namespace ctr {

ONNXRuntimeEngine::ONNXRuntimeEngine()
    : env_(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "FlowAdCTR")),
      session_options_(std::make_unique<Ort::SessionOptions>()),
      memory_info_(std::make_unique<Ort::MemoryInfo>(
          Ort::MemoryInfo::CreateCpu(
              OrtArenaAllocator,
              OrtMemTypeDefault
          )
      )) {

    // 配置 Session 选项
    session_options_->SetIntraOpNumThreads(config_.thread_pool_size);
    session_options_->SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_ALL
    );

    if (config_.enable_simd) {
        session_options_->AddConfigEntry("session.intra_op_allow_spinning", "1");
    }
}

ONNXRuntimeEngine::~ONNXRuntimeEngine() {
    // ONNX Runtime 对象会自动释放
}

bool ONNXRuntimeEngine::initialize(
    const ModelConfig& config,
    const FeatureSpec& spec
) {
    config_ = config;
    feature_spec_ = spec;

    // 加载 ONNX 模型
    try {
        session_ = std::make_unique<Ort::Session>(
            *env_,
            config.model_path.c_str(),
            *session_options_
        );
    } catch (const Ort::Exception& e) {
        std::cerr << "Failed to load ONNX model: " << e.what() << std::endl;
        return false;
    }

    // 构建输入映射
    if (!build_input_mappings()) {
        return false;
    }

    return true;
}

bool ONNXRuntimeEngine::build_input_mappings() {
    // 获取输入数量
    size_t num_inputs = session_->GetInputCount();

    // 获取特征规范中的所有特征
    const auto& feature_order = feature_spec_.get_feature_order();

    // 构建特征到模型输入的映射
    for (size_t i = 0; i < num_inputs; ++i) {
        char* input_name = session_->GetInputName(i, Ort::AllocatorWithDefaultOptions());
        auto type_info = session_->GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();

        InputMapping mapping;
        mapping.input_name = input_name;

        // 查找对应的特征
        for (const auto& feature_name : feature_order) {
            // 简化逻辑:假设输入名称就是特征名称
            if (mapping.input_name == feature_name) {
                mapping.feature_name = feature_name;
                break;
            }
        }

        // 获取张量形状
        tensor_info.GetDimensionsCount();
        tensor_info.GetDimensions(mapping.shape.data(), mapping.shape.size());

        // 获取数据类型
        mapping.type = tensor_info.GetElementType();

        input_mappings_.push_back(mapping);
        input_names_.push_back(input_name);
    }

    // 获取输出名称
    size_t num_outputs = session_->GetOutputCount();
    for (size_t i = 0; i < num_outputs; ++i) {
        char* output_name = session_->GetOutputName(i, Ort::AllocatorWithDefaultOptions());
        output_names_.push_back(output_name);
    }

    return true;
}

bool ONNXRuntimeEngine::predict(
    const FeatureContext& context,
    InferenceResult& result
) {
    auto start = std::chrono::high_resolution_clock::now();

    // 准备输入张量
    std::vector<std::vector<float>> input_tensors;
    std::vector<Ort::Value> input_ort_values;

    for (const auto& mapping : input_mappings_) {
        std::vector<float> tensor_data;
        if (!prepare_input_tensor(context, mapping, tensor_data)) {
            return false;
        }

        // 创建 ONNX 张量
        auto input_tensor = Ort::Value::CreateTensor<float>(
            *memory_info_,
            tensor_data.data(),
            tensor_data.size(),
            mapping.shape.data(),
            mapping.shape.size()
        );

        input_ort_values.push_back(std::move(input_tensor));
    }

    // 运行推理
    try {
        auto output_ort_values = session_->Run(
            Ort::RunOptions{nullptr},
            input_names_.data(),
            input_ort_values.data(),
            input_names_.size(),
            output_names_.data(),
            output_names_.size()
        );

        // 提取输出(假设输出是 CTR 概率)
        float* output_data = output_ort_values[0].GetTensorMutableData<float>();
        result.ctr = static_cast<double>(output_data[0]);

        // 计算"置信度"(简化:使用固定值)
        result.confidence = 0.8;

        // 记录模型版本
        result.model_version_hash = std::hash<std::string>{}(config_.model_version);

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX inference failed: " << e.what() << std::endl;
        return false;
    }

    // 记录延迟
    auto end = std::chrono::high_resolution_clock::now();
    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    record_latency(latency_ms);

    total_predictions_.fetch_add(1, std::memory_order_relaxed);

    return true;
}

bool ONNXRuntimeEngine::predict_batch(
    const std::vector<FeatureContext>& contexts,
    std::vector<InferenceResult>& results
) {
    results.resize(contexts.size());

    // 简化实现:循环调用单个预测
    // 实际生产环境应该实现真正的批量推理(将多个样本打包成一个批次)
    for (size_t i = 0; i < contexts.size(); ++i) {
        if (!predict(contexts[i], results[i])) {
            return false;
        }
    }

    total_batch_predictions_.fetch_add(1, std::memory_order_relaxed);

    return true;
}

Stats ONNXRuntimeEngine::get_stats() const {
    return calculate_stats();
}

bool ONNXRuntimeEngine::reload_model(const std::string& model_path) {
    // 创建新会话
    try {
        auto new_session = std::make_unique<Ort::Session>(
            *env_,
            model_path.c_str(),
            *session_options_
        );

        // 原子替换
        std::lock_guard<std::mutex> lock(stats_mutex_);
        session_ = std::move(new_session);
        config_.model_path = model_path;

        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "Failed to reload model: " << e.what() << std::endl;
        return false;
    }
}

bool ONNXRuntimeEngine::prepare_input_tensor(
    const FeatureContext& context,
    const InputMapping& mapping,
    std::vector<float>& tensor_data
) {
    // 从 FeatureContext 中提取特征值
    auto feature_spec = feature_spec_.get_feature(mapping.feature_name);
    if (!feature_spec) {
        return false;
    }

    // 根据特征类型处理
    switch (feature_spec->type) {
        case FeatureType::DENSE_FLOAT: {
            auto value = context.get<float>(mapping.feature_name);
            if (value.has_value()) {
                // 归一化(如果配置了 min/max)
                float normalized = value.value();
                if (feature_spec->min_value.has_value() && feature_spec->max_value.has_value()) {
                    double min = feature_spec->min_value.value();
                    double max = feature_spec->max_value.value();
                    normalized = (normalized - min) / (max - min);
                }
                tensor_data.push_back(normalized);
            } else {
                // 缺失值:使用默认值
                tensor_data.push_back(0.0f);
            }
            break;
        }
        case FeatureType::SPARSE_INT: {
            auto value = context.get<int64_t>(mapping.feature_name);
            if (value.has_value()) {
                tensor_data.push_back(static_cast<float>(value.value()));
            } else {
                // 缺失值:使用 0(表示 unknown)
                tensor_data.push_back(0.0f);
            }
            break;
        }
        case FeatureType::EMBEDDING: {
            auto value = context.get<std::vector<float>>(mapping.feature_name);
            if (value.has_value()) {
                const auto& embedding = value.value();
                tensor_data.insert(tensor_data.end(), embedding.begin(), embedding.end());
            } else {
                // 缺失值:使用零向量
                tensor_data.resize(feature_spec->embedding_dim, 0.0f);
            }
            break;
        }
        default:
            return false;
    }

    return true;
}

void ONNXRuntimeEngine::record_latency(double latency_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    // 只保留最近 1000 个样本
    if (latencies_.size() >= 1000) {
        latencies_.erase(latencies_.begin());
    }
    latencies_.push_back(latency_ms);
}

Stats ONNXRuntimeEngine::calculate_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Stats stats{};
    stats.total_predictions = total_predictions_.load(std::memory_order_relaxed);
    stats.total_batch_predictions = total_batch_predictions_.load(std::memory_order_relaxed);

    if (!latencies_.empty()) {
        // 计算平均延迟
        double sum = std::accumulate(latencies_.begin(), latencies_.end(), 0.0);
        stats.avg_latency_ms = sum / latencies_.size();

        // 计算分位数
        std::vector<double> sorted_latencies = latencies_;
        std::sort(sorted_latencies.begin(), sorted_latencies.end());

        stats.p50_latency_ms = sorted_latencies[sorted_latencies.size() * 0.5];
        stats.p99_latency_ms = sorted_latencies[sorted_latencies.size() * 0.99];
    }

    return stats;
}

} // namespace ctr
} // namespace flow_ad
```

---

## 🔄 CTR 管理器 (CTRManager)

```cpp
#pragma once
#include "model_inference_engine.h"
#include "feature_extractor.h"
#include "feature_spec.h"
#include "model_config.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <random>

namespace flow_ad {
namespace ctr {

// CTR 管理器
class CTRManager {
public:
    CTRManager();
    ~CTRManager() = default;

    // 禁止拷贝和移动
    CTRManager(const CTRManager&) = delete;
    CTRManager& operator=(const CTRManager&) = delete;

    /**
     * 初始化
     */
    bool initialize();

    /**
     * 预估 CTR
     * @param request OpenRTB 请求
     * @param imp 广告展示位
     * @param ad_id 广告 ID
     * @return CTR 预估值(0-1)
     */
    std::optional<double> predict_ctr(
        const openrtb::BidRequest& request,
        const openrtb::BidRequest_Imp& imp,
        uint64_t ad_id
    );

    /**
     * 添加模型
     */
    bool add_model(const ModelConfig& config);

    /**
     * 移除模型
     */
    bool remove_model(const std::string& model_name);

    /**
     * 重新加载模型
     */
    bool reload_model(const std::string& model_name, const std::string& model_path);

    /**
     * 获取统计信息
     */
    struct Stats {
        uint64_t total_predictions;
        double avg_latency_ms;
        std::unordered_map<std::string, uint64_t> model_prediction_counts;
    };
    Stats get_stats() const;

private:
    // 特征提取器
    std::vector<std::unique_ptr<FeatureExtractor>> feature_extractors_;

    // 模型引擎映射(模型名称 -> 引擎)
    std::unordered_map<std::string, std::unique_ptr<ModelInferenceEngine>> model_engines_;

    // 流量分配(用于 A/B 测试)
    struct TrafficAllocation {
        std::string model_name;
        double fraction;  // 0-1
    };
    std::vector<TrafficAllocation> traffic_allocations_;

    // 特征规范
    FeatureSpec feature_spec_;

    // 随机数生成器(用于流量分配)
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    mutable std::uniform_real_distribution<> dis_;

    // 统计信息
    mutable std::mutex stats_mutex_;
    std::atomic<uint64_t> total_predictions_{0};
    std::vector<double> latencies_;

    // 辅助方法
    ModelInferenceEngine* select_model_by_traffic();
    void record_latency(double latency_ms);
};

} // namespace ctr
} // namespace flow_ad
```

---

### CTR 管理器实现

```cpp
#include "ctr_manager.h"
#include "onnx_runtime_engine.h"
#include <chrono>
#include <numeric>
#include <algorithm>
#include <fstream>

namespace flow_ad {
namespace ctr {

CTRManager::CTRManager()
    : gen_(rd_()), dis_(0.0, 1.0) {
}

bool CTRManager::initialize() {
    // 1. 加载特征规范
    // TODO: 从配置文件加载
    feature_spec_.add_feature({
        .name = "user_age",
        .type = FeatureType::DENSE_FLOAT,
        .dimension = 1,
        .min_value = 0.0,
        .max_value = 100.0
    });

    feature_spec_.add_feature({
        .name = "user_gender",
        .type = FeatureType::SPARSE_INT,
        .dimension = 1
    });

    // 2. 注册特征提取器
    // TODO: 实现具体的特征提取器

    return true;
}

std::optional<double> CTRManager::predict_ctr(
    const openrtb::BidRequest& request,
    const openrtb::BidRequest_Imp& imp,
    uint64_t ad_id
) {
    auto start = std::chrono::high_resolution_clock::now();

    // 1. 选择模型(基于流量分配)
    ModelInferenceEngine* engine = select_model_by_traffic();
    if (!engine) {
        return std::nullopt;
    }

    // 2. 提取特征
    FeatureContext context;
    for (auto& extractor : feature_extractors_) {
        extractor->extract(request, imp, context);
    }

    // 3. 模型推理
    InferenceResult result;
    if (!engine->predict(context, result)) {
        return std::nullopt;
    }

    // 4. 记录延迟
    auto end = std::chrono::high_resolution_clock::now();
    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    record_latency(latency_ms);

    total_predictions_.fetch_add(1, std::memory_order_relaxed);

    return result.ctr;
}

bool CTRManager::add_model(const ModelConfig& config) {
    // 创建推理引擎
    auto engine = std::make_unique<ONNXRuntimeEngine>();

    if (!engine->initialize(config, feature_spec_)) {
        return false;
    }

    // 添加到映射
    model_engines_[config.model_name] = std::move(engine);

    // 更新流量分配
    traffic_allocations_.push_back({
        config.model_name,
        config.traffic_fraction
    });

    return true;
}

bool CTRManager::remove_model(const std::string& model_name) {
    auto it = model_engines_.find(model_name);
    if (it == model_engines_.end()) {
        return false;
    }

    model_engines_.erase(it);

    // 更新流量分配
    traffic_allocations_.erase(
        std::remove_if(traffic_allocations_.begin(), traffic_allocations_.end(),
                      [&model_name](const TrafficAllocation& alloc) {
                          return alloc.model_name == model_name;
                      }),
        traffic_allocations_.end()
    );

    return true;
}

bool CTRManager::reload_model(const std::string& model_name, const std::string& model_path) {
    auto it = model_engines_.find(model_name);
    if (it == model_engines_.end()) {
        return false;
    }

    return it->second->reload_model(model_path);
}

CTRManager::Stats CTRManager::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    Stats stats{};
    stats.total_predictions = total_predictions_.load(std::memory_order_relaxed);

    if (!latencies_.empty()) {
        double sum = std::accumulate(latencies_.begin(), latencies_.end(), 0.0);
        stats.avg_latency_ms = sum / latencies_.size();
    }

    // 统计每个模型的预测次数
    for (const auto& [name, engine] : model_engines_) {
        stats.model_prediction_counts[name] = engine->get_stats().total_predictions;
    }

    return stats;
}

ModelInferenceEngine* CTRManager::select_model_by_traffic() {
    if (traffic_allocations_.empty()) {
        return nullptr;
    }

    // 生成随机数
    double rand_val = dis_(gen_);

    // 根据流量分配选择模型
    double cumulative = 0.0;
    for (const auto& alloc : traffic_allocations_) {
        cumulative += alloc.fraction;
        if (rand_val <= cumulative) {
            auto it = model_engines_.find(alloc.model_name);
            if (it != model_engines_.end()) {
                return it->second.get();
            }
        }
    }

    // 默认返回第一个模型
    return model_engines_.begin()->second.get();
}

void CTRManager::record_latency(double latency_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    if (latencies_.size() >= 1000) {
        latencies_.erase(latencies_.begin());
    }
    latencies_.push_back(latency_ms);
}

} // namespace ctr
} // namespace flow_ad
```

---

## 🧪 单元测试

```cpp
#include <gtest/gtest.h>
#include "ctr_manager.h"
#include "onnx_runtime_engine.h"
#include "feature_extractor.h"
#include <memory>

// Mock 特征提取器
class MockFeatureExtractor : public ctr::FeatureExtractor {
public:
    void extract(
        const openrtb::BidRequest& request,
        const openrtb::BidRequest_Imp& imp,
        ctr::FeatureContext& context
    ) override {
        // 设置一些模拟特征
        context.set("user_age", 25.0f);
        context.set("user_gender", static_cast<int64_t>(1));
        context.set("ad_id", static_cast<int64_t>(12345));
    }

    std::string get_name() const override {
        return "MockFeatureExtractor";
    }
};

// CTR 管理器测试
class CTRManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctr_manager_ = std::make_unique<ctr::CTRManager>();
        ctr_manager_->initialize();
    }

    std::unique_ptr<ctr::CTRManager> ctr_manager_;
};

// 基本测试:模型加载和预测
TEST_F(CTRManagerTest, BasicPrediction) {
    // 添加模型(需要一个实际的 ONNX 模型文件)
    ctr::ModelConfig config;
    config.model_name = "test_model";
    config.model_version = "v1.0.0";
    config.model_path = "models/test_model.onnx";
    config.traffic_fraction = 1.0;

    // 假设模型文件存在
    // EXPECT_TRUE(ctr_manager_->add_model(config));

    // 测试预测
    openrtb::BidRequest request;
    openrtb::BidRequest_Imp imp;

    // auto ctr = ctr_manager_->predict_ctr(request, imp, 12345);
    // EXPECT_TRUE(ctr.has_value());
    // EXPECT_GE(ctr.value(), 0.0);
    // EXPECT_LE(ctr.value(), 1.0);
}

// 多模型 A/B 测试
TEST_F(CTRManagerTest, MultiModelABTest) {
    // 添加多个模型
    ctr::ModelConfig config1;
    config1.model_name = "model_a";
    config1.model_version = "v1.0.0";
    config1.model_path = "models/model_a.onnx";
    config1.traffic_fraction = 0.3;

    ctr::ModelConfig config2;
    config2.model_name = "model_b";
    config2.model_version = "v1.0.0";
    config2.model_path = "models/model_b.onnx";
    config2.traffic_fraction = 0.7;

    // EXPECT_TRUE(ctr_manager_->add_model(config1));
    // EXPECT_TRUE(ctr_manager_->add_model(config2));

    // 验证流量分配
    // TODO: 运行多次预测,统计每个模型的使用次数
}

// 性能测试
TEST_F(CTRManagerTest, PerformanceTest) {
    // 添加模型
    ctr::ModelConfig config;
    config.model_name = "perf_model";
    config.model_version = "v1.0.0";
    config.model_path = "models/perf_model.onnx";
    config.traffic_fraction = 1.0;

    // EXPECT_TRUE(ctr_manager_->add_model(config));

    // 性能测试
    constexpr int num_iterations = 10000;
    openrtb::BidRequest request;
    openrtb::BidRequest_Imp imp;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        // ctr_manager_->predict_ctr(request, imp, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double qps = num_iterations * 1000.0 / duration.count();
    std::cout << "CTR Prediction QPS: " << qps << std::endl;

    // 期望 QPS > 100K
    // EXPECT_GT(qps, 100000);
}

// 特征提取器测试
TEST_F(CTRManagerTest, FeatureExtraction) {
    // 添加 Mock 特征提取器
    auto extractor = std::make_unique<MockFeatureExtractor>();
    // ctr_manager_->register_feature_extractor(std::move(extractor));

    // 验证特征提取
    openrtb::BidRequest request;
    openrtb::BidRequest_Imp imp;

    ctr::FeatureContext context;
    // extractor->extract(request, imp, context);

    // EXPECT_TRUE(context.has("user_age"));
    // EXPECT_TRUE(context.has("user_gender"));
}
```

---

## 📦 离线训练部分(Python + TensorFlow)

### 1. 数据处理 Pipeline

```python
# data_pipeline.py
import tensorflow as tf
from tensorflow.keras import layers
from typing import Dict, List, Tuple
import numpy as np

class CTRDataPipeline:
    """CTR 预估训练数据 Pipeline"""

    def __init__(self, feature_spec: Dict):
        self.feature_spec = feature_spec
        self.feature_columns = self._build_feature_columns()

    def _build_feature_columns(self) -> Dict[str, tf.feature_column]:
        """构建 TensorFlow 特征列"""
        columns = {}

        for feature_name, spec in self.feature_spec.items():
            if spec['type'] == 'dense':
                # 连续特征
                columns[feature_name] = tf.feature_column.numeric_column(
                    feature_name,
                    shape=(spec['dimension'],)
                )
            elif spec['type'] == 'sparse':
                # 稀疏特征(分类)
                columns[feature_name] = tf.feature_column.embedding_column(
                    tf.feature_column.categorical_column_with_hash_bucket(
                        feature_name,
                        hash_bucket_size=spec['vocab_size']
                    ),
                    dimension=spec['embedding_dim']
                )
            elif spec['type'] == 'sequence':
                # 序列特征
                columns[feature_name] = tf.feature_column.sequence_numeric_column(
                    feature_name
                )

        return columns

    def create_dataset(
        self,
        data_path: str,
        batch_size: int = 1024,
        shuffle: bool = True
    ) -> tf.data.Dataset:
        """创建训练数据集"""

        def parse_fn(serialized_example):
            """解析 TFRecord"""
            feature_spec = {
                name: tf.io.FixedLenFeature([], dtype=tf.float32)
                for name in self.feature_spec.keys()
            }
            feature_spec['label'] = tf.io.FixedLenFeature([], dtype=tf.float32)

            parsed = tf.io.parse_single_example(serialized_example, feature_spec)

            features = {
                name: parsed[name]
                for name in self.feature_spec.keys()
            }
            label = parsed['label']

            return features, label

        # 读取 TFRecord 文件
        files = tf.io.gfile.glob(data_path)
        dataset = tf.data.TFRecordDataset(files)

        if shuffle:
            dataset = dataset.shuffle(buffer_size=10000)

        dataset = dataset.map(parse_fn, num_parallel_calls=tf.data.AUTOTUNE)
        dataset = dataset.batch(batch_size)
        dataset = dataset.prefetch(tf.data.AUTOTUNE)

        return dataset
```

---

### 2. DeepFM 模型实现

```python
# models/deep_fm.py
import tensorflow as tf
from tensorflow.keras import layers, Model
from typing import Dict, List

class DeepFM(Model):
    """DeepFM 模型实现"""

    def __init__(
        self,
        feature_columns: Dict[str, tf.feature_column],
        embedding_dim: int = 16,
        hidden_units: List[int] = [512, 256, 128],
        dropout_rate: float = 0.1
    ):
        super(DeepFM, self).__init__()

        self.feature_columns = feature_columns
        self.embedding_dim = embedding_dim

        # 构建输入层
        self.inputs = self._build_inputs()

        # 构建嵌入层
        self.embeddings = self._build_embeddings()

        # FM 一阶项
        self.fm_linear = self._build_fm_linear()

        # FM 二阶项
        self.fm_interaction = self._build_fm_interaction()

        # Deep 网络
        self.deep_network = self._build_deep_network(hidden_units, dropout_rate)

        # 输出层
        self.output_layer = layers.Dense(1, activation='sigmoid')

    def _build_inputs(self) -> Dict[str, layers.Input]:
        """构建输入层"""
        inputs = {}
        for name, column in self.feature_columns.items():
            if isinstance(column, tf.feature_column.NumericColumn):
                inputs[name] = layers.Input(
                    shape=(column.shape[0] if column.shape else 1,),
                    name=name,
                    dtype=tf.float32
                )
            elif isinstance(column, tf.feature_column.EmbeddingColumn):
                inputs[name] = layers.Input(
                    shape=(1,),
                    name=name,
                    dtype=tf.int64
                )
        return inputs

    def _build_embeddings(self) -> layers.Layer:
        """构建嵌入层"""
        embeddings = {}

        for name, column in self.feature_columns.items():
            if isinstance(column, tf.feature_column.EmbeddingColumn):
                # 获取词汇表大小
                vocab_size = column.categorical_column.num_buckets

                embeddings[name] = layers.Embedding(
                    input_dim=vocab_size,
                    output_dim=self.embedding_dim,
                    name=f'{name}_embedding'
                )

        return embeddings

    def _build_fm_linear(self) -> layers.Layer:
        """FM 一阶项"""
        linear_inputs = []

        for name, column in self.feature_columns.items():
            if isinstance(column, tf.feature_column.NumericColumn):
                linear_inputs.append(self.inputs[name])
            elif isinstance(column, tf.feature_column.EmbeddingColumn):
                # 嵌入特征的线性部分
                embedded = self.embeddings[name](self.inputs[name])
                linear_inputs.append(tf.reduce_sum(embedded, axis=1))

        if linear_inputs:
            return layers.Concatenate()(linear_inputs)
        else:
            return None

    def _build_fm_interaction(self) -> layers.Layer:
        """FM 二阶项"""
        embedding_list = []

        for name, column in self.feature_columns.items():
            if isinstance(column, tf.feature_column.EmbeddingColumn):
                embedded = self.embeddings[name](self.inputs[name])
                embedding_list.append(tf.squeeze(embedded, axis=1))

        if embedding_list:
            # 计算二阶交互: 0.5 * sum((sum(embeddings))^2 - sum(embeddings^2))
            embeddings_concat = layers.Concatenate()(embedding_list)

            sum_of_squares = tf.reduce_sum(tf.square(embeddings_concat), axis=1, keepdims=True)
            square_of_sum = tf.square(tf.reduce_sum(embeddings_concat, axis=1, keepdims=True))

            interaction = 0.5 * tf.reduce_sum(
                square_of_sum - sum_of_squares,
                axis=1,
                keepdims=True
            )

            return interaction
        else:
            return None

    def _build_deep_network(
        self,
        hidden_units: List[int],
        dropout_rate: float
    ) -> layers.Layer:
        """构建 Deep 网络"""
        # 将所有嵌入拼接
        embedding_list = []

        for name, column in self.feature_columns.items():
            if isinstance(column, tf.feature_column.EmbeddingColumn):
                embedded = self.embeddings[name](self.inputs[name])
                embedding_list.append(tf.squeeze(embedded, axis=1))
            elif isinstance(column, tf.feature_column.NumericColumn):
                embedding_list.append(self.inputs[name])

        if embedding_list:
            deep_input = layers.Concatenate()(embedding_list)

            # 构建 MLP
            deep_network = deep_input
            for units in hidden_units:
                deep_network = layers.Dense(units, activation='relu')(deep_network)
                deep_network = layers.Dropout(dropout_rate)(deep_network)

            return deep_network
        else:
            return None

    def call(self, inputs: Dict[str, tf.Tensor]) -> tf.Tensor:
        """前向传播"""

        # FM 部分
        fm_linear = self.fm_linear
        fm_interaction = self.fm_interaction

        # Deep 部分
        deep = self.deep_network

        # 组合
        if fm_linear is not None and fm_interaction is not None and deep is not None:
            combined = layers.Concatenate()([fm_linear, fm_interaction, deep])
        elif deep is not None:
            combined = deep
        else:
            raise ValueError("Invalid model configuration")

        # 输出
        output = self.output_layer(combined)

        return output
```

---

### 3. 训练脚本

```python
# train.py
import tensorflow as tf
from data_pipeline import CTRDataPipeline
from models.deep_fm import DeepFM
import argparse
import json

def train(args):
    # 1. 加载特征规范
    with open(args.feature_spec, 'r') as f:
        feature_spec = json.load(f)

    # 2. 创建数据 Pipeline
    data_pipeline = CTRDataPipeline(feature_spec)

    train_dataset = data_pipeline.create_dataset(
        args.train_data,
        batch_size=args.batch_size,
        shuffle=True
    )

    eval_dataset = data_pipeline.create_dataset(
        args.eval_data,
        batch_size=args.batch_size,
        shuffle=False
    )

    # 3. 构建模型
    feature_columns = data_pipeline.feature_columns
    model = DeepFM(
        feature_columns=feature_columns,
        embedding_dim=args.embedding_dim,
        hidden_units=args.hidden_units,
        dropout_rate=args.dropout_rate
    )

    # 4. 编译模型
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=args.learning_rate),
        loss='binary_crossentropy',
        metrics=[
            tf.keras.metrics.AUC(name='auc'),
            tf.keras.metrics.BinaryCrossentropy(name='logloss')
        ]
    )

    # 5. 训练
    callbacks = [
        tf.keras.callbacks.ModelCheckpoint(
            filepath=args.checkpoint_dir,
            save_best_only=True,
            monitor='val_auc',
            mode='max'
        ),
        tf.keras.callbacks.TensorBoard(log_dir=args.log_dir),
        tf.keras.callbacks.EarlyStopping(
            monitor='val_auc',
            patience=3,
            mode='max'
        )
    ]

    model.fit(
        train_dataset,
        validation_data=eval_dataset,
        epochs=args.epochs,
        callbacks=callbacks
    )

    # 6. 评估
    eval_results = model.evaluate(eval_dataset)
    print(f"Evaluation Results: {eval_results}")

    # 7. 导出 ONNX 模型
    if args.export_onnx:
        import tf2onnx
        onnx_model_path = f"{args.checkpoint_dir}/model.onnx"
        model_proto, _ = tf2onnx.convert.from_keras(
            model,
            output_path=onnx_model_path
        )
        print(f"ONNX model exported to {onnx_model_path}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--train-data', type=str, required=True)
    parser.add_argument('--eval-data', type=str, required=True)
    parser.add_argument('--feature-spec', type=str, required=True)
    parser.add_argument('--checkpoint-dir', type=str, default='./checkpoints')
    parser.add_argument('--log-dir', type=str, default='./logs')
    parser.add_argument('--batch-size', type=int, default=1024)
    parser.add_argument('--epochs', type=int, default=10)
    parser.add_argument('--learning-rate', type=float, default=0.001)
    parser.add_argument('--embedding-dim', type=int, default=16)
    parser.add_argument('--hidden-units', type=int, nargs='+', default=[512, 256, 128])
    parser.add_argument('--dropout-rate', type=float, default=0.1)
    parser.add_argument('--export-onnx', action='store_true')

    args = parser.parse_args()
    train(args)
```

---

## ✅ 验收标准

### 功能验收
- [ ] 支持多种模型类型(LR、FM、DeepFM、Wide & Deep)
- [ ] 支持特征提取(用户特征、广告特征、上下文特征)
- [ ] 支持模型热更新(不中断服务)
- [ ] 支持 A/B 测试(多模型流量分配)
- [ ] 支持离线训练 + 在线预估完整流程
- [ ] 导出 ONNX 模型格式

### 性能验收
- [ ] 单次预测延迟 p99 < 5ms
- [ ] 吞吐量 > 100K QPS
- [ ] 内存占用合理(< 2GB,单模型)
- [ ] 模型加载时间 < 10 秒

### 质量验收
- [ ] 单元测试覆盖率 > 80%
- [ ] 离线训练 AUC > 0.75
- [ ] 在线预估与离线训练误差 < 5%
- [ ] 无内存泄漏

---

## 📚 相关文档

- [项目主 README](../../README.md)
- [架构设计规范](schema.md)
- [Step 3: 广告索引模块](step3-adIndexing.md)
- [Step 4: 频次控制模块](step4-frequency-production.md)
- [Step 5: Pacing 模块](step5-pacing-production.md)
- [ONNX Runtime C++ API](https://onnxruntime.ai/docs/c-api/)
- [TensorFlow Recommenders](https://www.tensorflow.org/recommenders)

---

## 🚀 实施步骤

### Phase 1: 基础设施 (Week 1-2)
1. 搭建 ONNX Runtime C++ 环境
2. 实现特征提取器接口和基础实现
3. 实现 CTR 管理器框架
4. 编写单元测试

### Phase 2: 特征工程 (Week 3-4)
1. 实现用户特征提取器(年龄、性别、地域等)
2. 实现广告特征提取器(创意 ID、行业等)
3. 实现上下文特征提取器(时间、设备等)
4. 实现特征归一化和分桶

### Phase 3: 离线训练 (Week 5-6)
1. 搭建 TensorFlow 训练环境
2. 实现 DeepFM/Wide & Deep 模型
3. 训练模型并评估
4. 导出 ONNX 模型

### Phase 4: 在线预估 (Week 7-8)
1. 实现 ONNX Runtime 推理引擎
2. 实现模型热更新机制
3. 性能优化(SIMD、批处理)
4. 集成到广告投放服务

### Phase 5: A/B 测试和优化 (Week 9-10)
1. 实现流量分配机制
2. 上线 A/B 测试
3. 监控指标(AUC、LogLoss)
4. 优化模型和特征

---

**作者**: airprofly
**创建日期**: 2026-03-11
**状态**: 📝 技术规划方案
