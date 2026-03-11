#pragma once

// Author: airprofly

#include "openrtb_models.h"
#include "rapidjson/document.h"
#include <string>
#include <vector>

namespace flow_ad::openrtb
{

    // -------------------------------------------------------------------------
    // Parse Error Types
    // -------------------------------------------------------------------------

    enum class ParseError
    {
        NONE = 0,
        INVALID_JSON,
        MISSING_REQUIRED_FIELD,
        INVALID_FIELD_TYPE,
        INVALID_FIELD_VALUE,
        UNKNOWN
    };

    // -------------------------------------------------------------------------
    // Parse Result
    // -------------------------------------------------------------------------

    struct ParseResult
    {
        bool success;
        OpenRTBRequest request;
        ParseError error_type;
        std::string error_message;
        size_t error_position; // JSON 错误位置

        // 获取完整错误信息
        std::string get_full_error() const
        {
            if (success)
            {
                return "No error";
            }

            std::string error_str;
            switch (error_type)
            {
            case ParseError::INVALID_JSON:
                error_str = "Invalid JSON";
                break;
            case ParseError::MISSING_REQUIRED_FIELD:
                error_str = "Missing required field";
                break;
            case ParseError::INVALID_FIELD_TYPE:
                error_str = "Invalid field type";
                break;
            case ParseError::INVALID_FIELD_VALUE:
                error_str = "Invalid field value";
                break;
            default:
                error_str = "Unknown error";
                break;
            }

            std::string result = error_str;
            if (!error_message.empty())
            {
                result += ": " + error_message;
            }
            if (error_position > 0)
            {
                result += " at position " + std::to_string(error_position);
            }
            return result;
        }
    };

    // -------------------------------------------------------------------------
    // OpenRTB JSON Parser
    // -------------------------------------------------------------------------

    class OpenRTBParser
    {
    public:
        OpenRTBParser() = default;
        ~OpenRTBParser() = default;

        // ========== 单个请求解析 ==========

        // 解析 JSON 字符串
        ParseResult parse(const std::string &json_str);

        // 解析 JSON 文件 (包含单个请求)
        ParseResult parse_file(const std::string &file_path);

        // ========== 批量请求解析 ==========

        // 解析 JSON 文件 (包含多个请求的数组)
        std::vector<ParseResult> parse_batch_file(const std::string &file_path);

        // 解析 JSON 字符串数组
        std::vector<ParseResult> parse_batch(const std::string &json_array_str);

        // ========== 数据验证 ==========

        // 验证请求完整性
        bool validate(const OpenRTBRequest &request);

        // 验证请求并返回详细错误信息
        std::vector<std::string> validate_detailed(const OpenRTBRequest &request);

        // ========== 工具方法 ==========

        // 将 OpenRTBRequest 转换为 JSON 字符串 (用于调试)
        static std::string to_json(const OpenRTBRequest &request);

        // 美化 JSON 输出
        static std::string to_json_pretty(const OpenRTBRequest &request);

        // 获取最后一次错误的详细信息
        std::string get_last_error() const { return last_error_; }

        // 设置严格模式 (默认: false)
        // 严格模式下,任何不符合规范的字段都会导致解析失败
        void set_strict_mode(bool strict) { strict_mode_ = strict; }

        // 设置是否忽略未知字段 (默认: true)
        void set_ignore_unknown_fields(bool ignore)
        {
            ignore_unknown_fields_ = ignore;
        }

    private:
        bool strict_mode_ = false;
        bool ignore_unknown_fields_ = true;
        mutable std::string last_error_;

        // ========== 内部解析方法 ==========

        // 解析 Geo 对象
        std::optional<Geo> parse_geo(const rapidjson::Value &obj);

        // 解析 Banner 对象
        std::optional<Banner> parse_banner(const rapidjson::Value &obj);

        // 解析 Video 对象
        std::optional<Video> parse_video(const rapidjson::Value &obj);

        // 解析 Native 对象
        std::optional<Native> parse_native(const rapidjson::Value &obj);

        // 解析 Impression 对象
        std::optional<Impression> parse_impression(const rapidjson::Value &obj);

        // 解析 Device 对象
        std::optional<Device> parse_device(const rapidjson::Value &obj);

        // 解析 User 对象
        std::optional<User> parse_user(const rapidjson::Value &obj);

        // 解析 App 对象
        std::optional<App> parse_app(const rapidjson::Value &obj);

        // 解析 Site 对象
        std::optional<Site> parse_site(const rapidjson::Value &obj);

        // 解析根对象
        std::optional<OpenRTBRequest> parse_request(const rapidjson::Value &obj);

        // ========== 验证方法 ==========

        bool validate_geo(const Geo &geo, std::vector<std::string> &errors);
        bool validate_banner(const Banner &banner, std::vector<std::string> &errors);
        bool validate_impression(const Impression &imp,
                                 std::vector<std::string> &errors);
        bool validate_device(const Device &device, std::vector<std::string> &errors);
        bool validate_user(const User &user, std::vector<std::string> &errors);
    };

} // namespace flow_ad::openrtb
