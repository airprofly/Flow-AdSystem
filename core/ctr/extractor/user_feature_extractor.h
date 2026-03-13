#pragma once

#include "feature_extractor.h"
#include <unordered_map>
#include <string>

namespace ctr {

/**
 * @brief 用户特征提取器
 *
 * 提取用户相关的特征:
 * - 用户年龄 (归一化)
 * - 用户性别 (类别)
 * - 用户地域/城市 (类别)
 * - 用户设备类型 (类别)
 * - 用户历史行为统计 (数值)
 */
class UserFeatureExtractor : public IFeatureExtractor {
public:
    UserFeatureExtractor();
    ~UserFeatureExtractor() override = default;

    /**
     * @brief 从 OpenRTB 请求的 User 对象中提取特征
     * @param request_data OpenRTB JSON 字符串
     * @param feature_context [out] 输出的特征上下文
     * @return 是否成功
     */
    bool extract(const std::string& request_data, FeatureContext& feature_context) override;

    /**
     * @brief 获取支持的特征规范
     */
    FeatureSpec get_feature_spec() const override;

    /**
     * @brief 获取提取器名称
     */
    std::string get_name() const override { return "UserFeatureExtractor"; }

private:
    /**
     * @brief 归一化年龄到 [0, 1]
     * @param age 年龄
     * @return 归一化后的值
     */
    float normalize_age(int age) const;

    /**
     * @brief 将性别字符串转换为类别ID
     * @param gender 性别 ("M"/"F"/"O"/"U")
     * @return 类别ID
     */
    int64_t encode_gender(const std::string& gender) const;

    /**
     * @brief 将设备类型转换为类别ID
     * @param device_type 设备类型
     * @return 类别ID
     */
    int64_t encode_device_type(const std::string& device_type) const;

    FeatureSpec feature_spec_;  // 支持的特征规范
};

} // namespace ctr
