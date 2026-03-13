#pragma once

#include <string>
#include <vector>
#include <memory>

namespace ctr {

/**
 * @brief 模型类型枚举
 */
enum class ModelType {
    DEEP_FM,        // DeepFM 模型
    WIDE_DEEP,      // Wide & Deep 模型
    DEEP_CROSS,     // Deep & Cross 模型
    UNKNOWN
};

/**
 * @brief 模型配置
 */
struct ModelConfig {
    std::string model_name;           // 模型名称
    std::string model_path;           // ONNX 模型文件路径
    ModelType model_type;             // 模型类型
    float traffic_fraction;           // 流量分配比例 (0.0 - 1.0)
    int64_t embedding_dim;            // 嵌入维度
    std::vector<int> mlp_dims;        // MLP 隐藏层维度
    float dropout;                    // Dropout 比例
    bool enabled;                     // 是否启用

    ModelConfig()
        : model_type(ModelType::DEEP_FM)
        , traffic_fraction(1.0f)
        , embedding_dim(32)
        , dropout(0.2f)
        , enabled(true) {}
};

/**
 * @brief 推理结果
 */
struct InferenceResult {
    float ctr;                        // 预估 CTR (0.0 - 1.0)
    double confidence;                // 置信度
    double inference_time_ms;         // 推理耗时 (毫秒)
    bool success;                     // 是否成功
    std::string error_message;        // 错误信息

    InferenceResult()
        : ctr(0.0f)
        , confidence(0.0)
        , inference_time_ms(0.0)
        , success(false)
        , error_message("") {}
};

/**
 * @brief 批量推理结果
 */
struct BatchInferenceResult {
    std::vector<float> ctrs;          // CTR 预估值列表
    std::vector<bool> successes;      // 成功标志列表
    std::vector<std::string> errors;  // 错误信息列表
    double total_time_ms;             // 总耗时
    double avg_time_ms;               // 平均耗时

    BatchInferenceResult()
        : total_time_ms(0.0)
        , avg_time_ms(0.0) {}
};

} // namespace ctr
