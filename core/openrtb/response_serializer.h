#pragma once

// Author: airprofly
//
// Response 序列化器
// 将 BidResponse 对象序列化为 JSON 字符串

#include "openrtb_response_models.h"

#include <string>
#include <fstream>

#include <nlohmann/json.hpp>  // 需要完整包含以使用 nlohmann::json

namespace flow_ad::openrtb {

/**
 * @brief Response 序列化器
 *
 * 将 BidResponse 对象序列化为 JSON 字符串
 */
class ResponseSerializer {
public:
    ResponseSerializer() = default;
    ~ResponseSerializer() = default;

    /**
     * @brief 将 BidResponse 序列化为 JSON 字符串
     * @param response BidResponse 对象
     * @param pretty 是否格式化输出 (默认: false)
     * @return JSON 字符串
     */
    static std::string serialize(const BidResponse& response, bool pretty = false);

    /**
     * @brief 将 BidResponse 序列化并保存到文件
     * @param response BidResponse 对象
     * @param file_path 文件路径
     * @param pretty 是否格式化输出
     * @return 是否成功
     */
    static bool save_to_file(
        const BidResponse& response,
        const std::string& file_path,
        bool pretty = true
    );

private:
    /**
     * @brief 序列化 Bid 对象为 JSON
     */
    static nlohmann::json serialize_bid(const Bid& bid);

    /**
     * @brief 序列化 SeatBid 对象为 JSON
     */
    static nlohmann::json serialize_seatbid(const SeatBid& seatbid);

    /**
     * @brief 序列化 BidResponse 对象为 JSON
     */
    static nlohmann::json serialize_response(const BidResponse& response);
};

} // namespace flow_ad::openrtb
