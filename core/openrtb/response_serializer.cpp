// Author: airprofly
//
// Response 序列化器实现

#include "response_serializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace flow_ad::openrtb {

using json = nlohmann::json;

std::string ResponseSerializer::serialize(const BidResponse& response, bool pretty) {
    try {
        json j = serialize_response(response);

        if (pretty) {
            return j.dump(2);  // 缩进 2 个空格
        } else {
            return j.dump();
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to serialize response: ") + e.what());
    }
}

bool ResponseSerializer::save_to_file(
    const BidResponse& response,
    const std::string& file_path,
    bool pretty
) {
    try {
        std::string json_str = serialize(response, pretty);

        std::ofstream file(file_path);
        if (!file.is_open()) {
            return false;
        }

        file << json_str;
        file.close();

        return true;
    } catch (...) {
        return false;
    }
}

nlohmann::json ResponseSerializer::serialize_bid(const Bid& bid) {
    json j;

    // 必填字段
    j["id"] = bid.id;
    j["impid"] = bid.impid;
    j["price"] = bid.price;

    // 推荐字段
    if (bid.adid.has_value()) {
        j["adid"] = *bid.adid;
    }
    if (bid.nurl.has_value()) {
        j["nurl"] = *bid.nurl;
    }
    if (bid.adm.has_value()) {
        j["adm"] = *bid.adm;
    }

    // 可选字段
    if (bid.adomain.has_value()) {
        j["adomain"] = *bid.adomain;
    }
    if (bid.cat.has_value()) {
        j["cat"] = *bid.cat;
    }
    if (bid.attr.has_value()) {
        j["attr"] = *bid.attr;
    }
    if (bid.api.has_value()) {
        j["api"] = *bid.api;
    }
    if (bid.protocol.has_value()) {
        j["protocol"] = *bid.protocol;
    }
    if (bid.qagmediarating.has_value()) {
        j["qagmediarating"] = *bid.qagmediarating;
    }
    if (bid.language.has_value()) {
        j["language"] = *bid.language;
    }
    if (bid.bundle.has_value()) {
        j["bundle"] = *bid.bundle;
    }
    if (bid.ext.has_value()) {
        // 尝试解析 ext 字段 (可能是 JSON 字符串)
        try {
            j["ext"] = json::parse(*bid.ext);
        } catch (...) {
            // 如果解析失败,直接存储为字符串
            j["ext"] = *bid.ext;
        }
    }

    return j;
}

nlohmann::json ResponseSerializer::serialize_seatbid(const SeatBid& seatbid) {
    json j;

    // bid (必填)
    json bid_array = json::array();
    for (const auto& bid : seatbid.bid) {
        bid_array.push_back(serialize_bid(bid));
    }
    j["bid"] = bid_array;

    // 推荐字段
    if (seatbid.seat.has_value()) {
        j["seat"] = *seatbid.seat;
    }
    if (seatbid.group.has_value()) {
        j["group"] = *seatbid.group;
    }
    if (seatbid.ext.has_value()) {
        try {
            j["ext"] = json::parse(*seatbid.ext);
        } catch (...) {
            j["ext"] = *seatbid.ext;
        }
    }

    return j;
}

nlohmann::json ResponseSerializer::serialize_response(const BidResponse& response) {
    json j;

    // 必填字段
    j["id"] = response.id;

    // seatbid (必填)
    json seatbid_array = json::array();
    for (const auto& seatbid : response.seatbid) {
        seatbid_array.push_back(serialize_seatbid(seatbid));
    }
    j["seatbid"] = seatbid_array;

    // 推荐字段
    if (response.bidid.has_value()) {
        j["bidid"] = *response.bidid;
    }
    if (response.cur.has_value()) {
        j["cur"] = *response.cur;
    }

    // 可选字段
    if (response.nbr.has_value()) {
        j["nbr"] = *response.nbr;
    }
    if (response.ext.has_value()) {
        try {
            j["ext"] = json::parse(*response.ext);
        } catch (...) {
            j["ext"] = *response.ext;
        }
    }

    return j;
}

} // namespace flow_ad::openrtb
