// Author: airprofly

#include "openrtb_parser.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include <fstream>
#include <sstream>

namespace flow_ad::openrtb
{

    // -------------------------------------------------------------------------
    // Internal helper utilities (anonymous namespace)
    // -------------------------------------------------------------------------

    namespace
    {
        // 读取文件内容
        std::string read_file_content(const std::string &file_path)
        {
            std::ifstream file(file_path);
            if (!file.is_open())
                return "";
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        // 获取可选字符串字段
        std::optional<std::string> get_opt_string(const rapidjson::Value &obj,
                                                  const char *key)
        {
            if (obj.HasMember(key) && obj[key].IsString())
                return std::string(obj[key].GetString(), obj[key].GetStringLength());
            return std::nullopt;
        }

        // 获取可选整数字段
        template <typename T = int>
        std::optional<T> get_opt_int(const rapidjson::Value &obj, const char *key)
        {
            if (obj.HasMember(key) && obj[key].IsInt())
                return static_cast<T>(obj[key].GetInt());
            return std::nullopt;
        }

        // 获取可选双精度字段
        std::optional<double> get_opt_double(const rapidjson::Value &obj,
                                             const char *key)
        {
            if (obj.HasMember(key) && obj[key].IsNumber())
                return obj[key].GetDouble();
            return std::nullopt;
        }

        // 获取可选布尔字段
        std::optional<bool> get_opt_bool(const rapidjson::Value &obj, const char *key)
        {
            if (obj.HasMember(key) && obj[key].IsBool())
                return obj[key].GetBool();
            return std::nullopt;
        }

        // 获取可选整数数组字段
        std::optional<std::vector<int>> get_opt_int_array(const rapidjson::Value &obj,
                                                          const char *key)
        {
            if (!obj.HasMember(key) || !obj[key].IsArray())
                return std::nullopt;
            std::vector<int> result;
            for (const auto &v : obj[key].GetArray())
                if (v.IsInt())
                    result.push_back(v.GetInt());
            return result;
        }

        // 获取可选字符串数组字段
        std::optional<std::vector<std::string>>
        get_opt_string_array(const rapidjson::Value &obj, const char *key)
        {
            if (!obj.HasMember(key) || !obj[key].IsArray())
                return std::nullopt;
            std::vector<std::string> result;
            for (const auto &v : obj[key].GetArray())
                if (v.IsString())
                    result.push_back(v.GetString());
            return result;
        }

        // 将 rapidjson Value 序列化为 JSON 字符串
        std::string value_to_string(const rapidjson::Value &val)
        {
            rapidjson::StringBuffer sb;
            rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
            val.Accept(writer);
            return sb.GetString();
        }

    } // namespace

    // -------------------------------------------------------------------------
    // Public API: parse single request
    // -------------------------------------------------------------------------

    ParseResult OpenRTBParser::parse(const std::string &json_str)
    {
        ParseResult result;
        result.success = false;
        result.error_type = ParseError::NONE;
        result.error_position = 0;

        rapidjson::Document doc;
        doc.Parse(json_str.c_str(), json_str.size());

        if (doc.HasParseError())
        {
            result.error_type = ParseError::INVALID_JSON;
            result.error_message =
                std::string("JSON parse error: ") +
                rapidjson::GetParseError_En(doc.GetParseError());
            result.error_position = static_cast<size_t>(doc.GetErrorOffset());
            return result;
        }

        if (!doc.IsObject())
        {
            result.error_type = ParseError::INVALID_JSON;
            result.error_message = "Root element is not a JSON object";
            return result;
        }

        // 验证必填字段
        if (!doc.HasMember("id") || !doc["id"].IsString())
        {
            result.error_type = ParseError::MISSING_REQUIRED_FIELD;
            result.error_message = "Missing or invalid required field: 'id'";
            return result;
        }

        if (!doc.HasMember("imp") || !doc["imp"].IsArray())
        {
            result.error_type = ParseError::MISSING_REQUIRED_FIELD;
            result.error_message = "Missing or invalid required field: 'imp'";
            return result;
        }

        // 解析完整请求对象
        auto request_opt = parse_request(doc);
        if (!request_opt)
        {
            result.error_type = ParseError::INVALID_FIELD_VALUE;
            result.error_message = "Failed to parse request: " + last_error_;
            return result;
        }

        result.request = std::move(*request_opt);
        result.success = true;

        // 严格模式下执行详细验证
        if (strict_mode_)
        {
            auto errors = validate_detailed(result.request);
            if (!errors.empty())
            {
                result.success = false;
                result.error_type = ParseError::INVALID_FIELD_VALUE;
                result.error_message = "Validation failed: " + errors[0];
            }
        }

        return result;
    }

    ParseResult OpenRTBParser::parse_file(const std::string &file_path)
    {
        std::string content = read_file_content(file_path);
        if (content.empty())
        {
            ParseResult result;
            result.success = false;
            result.error_type = ParseError::INVALID_JSON;
            result.error_message = "Failed to read file: " + file_path;
            result.error_position = 0;
            return result;
        }
        return parse(content);
    }

    // -------------------------------------------------------------------------
    // Public API: batch parse
    // -------------------------------------------------------------------------

    std::vector<ParseResult>
    OpenRTBParser::parse_batch_file(const std::string &file_path)
    {
        std::string content = read_file_content(file_path);
        if (content.empty())
            return {};
        return parse_batch(content);
    }

    std::vector<ParseResult>
    OpenRTBParser::parse_batch(const std::string &json_array_str)
    {
        std::vector<ParseResult> results;

        rapidjson::Document doc;
        doc.Parse(json_array_str.c_str(), json_array_str.size());

        if (doc.HasParseError())
        {
            ParseResult r;
            r.success = false;
            r.error_type = ParseError::INVALID_JSON;
            r.error_message = std::string("Batch JSON parse error: ") +
                              rapidjson::GetParseError_En(doc.GetParseError());
            r.error_position = static_cast<size_t>(doc.GetErrorOffset());
            results.push_back(std::move(r));
            return results;
        }

        if (!doc.IsArray())
        {
            // 不是数组 – 尝试解析为单个请求
            results.push_back(parse(json_array_str));
            return results;
        }

        results.reserve(doc.GetArray().Size());
        for (const auto &item : doc.GetArray())
        {
            if (!item.IsObject())
            {
                ParseResult r;
                r.success = false;
                r.error_type = ParseError::INVALID_JSON;
                r.error_message = "Array element is not a JSON object";
                r.error_position = 0;
                results.push_back(std::move(r));
                continue;
            }

            ParseResult r;
            r.success = false;
            r.error_type = ParseError::NONE;
            r.error_position = 0;

            if (!item.HasMember("id") || !item["id"].IsString())
            {
                r.error_type = ParseError::MISSING_REQUIRED_FIELD;
                r.error_message = "Missing or invalid required field: 'id'";
                results.push_back(std::move(r));
                continue;
            }

            if (!item.HasMember("imp") || !item["imp"].IsArray())
            {
                r.error_type = ParseError::MISSING_REQUIRED_FIELD;
                r.error_message = "Missing or invalid required field: 'imp'";
                results.push_back(std::move(r));
                continue;
            }

            auto request_opt = parse_request(item);
            if (!request_opt)
            {
                r.error_type = ParseError::INVALID_FIELD_VALUE;
                r.error_message = "Failed to parse request: " + last_error_;
                results.push_back(std::move(r));
                continue;
            }

            r.request = std::move(*request_opt);
            r.success = true;

            if (strict_mode_)
            {
                auto errors = validate_detailed(r.request);
                if (!errors.empty())
                {
                    r.success = false;
                    r.error_type = ParseError::INVALID_FIELD_VALUE;
                    r.error_message = "Validation failed: " + errors[0];
                }
            }

            results.push_back(std::move(r));
        }

        return results;
    }

    // -------------------------------------------------------------------------
    // Internal parse helpers (all take const rapidjson::Value&)
    // -------------------------------------------------------------------------

    std::optional<Geo> OpenRTBParser::parse_geo(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Geo geo;
        geo.country = get_opt_string(obj, "country");
        geo.region = get_opt_string(obj, "region");
        geo.city = get_opt_string(obj, "city");
        geo.lat = get_opt_double(obj, "lat");
        geo.lon = get_opt_double(obj, "lon");
        geo.type = get_opt_int(obj, "type");
        if (obj.HasMember("ext"))
            geo.ext = value_to_string(obj["ext"]);
        return geo;
    }

    std::optional<Banner> OpenRTBParser::parse_banner(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Banner banner;

        // 宽度: 支持单个整数或整数数组
        if (obj.HasMember("w"))
        {
            if (obj["w"].IsArray())
            {
                std::vector<int> ws;
                for (const auto &v : obj["w"].GetArray())
                    if (v.IsInt())
                        ws.push_back(v.GetInt());
                banner.w = std::move(ws);
            }
            else if (obj["w"].IsInt())
            {
                banner.w = std::vector<int>{obj["w"].GetInt()};
            }
        }

        // 高度: 支持单个整数或整数数组
        if (obj.HasMember("h"))
        {
            if (obj["h"].IsArray())
            {
                std::vector<int> hs;
                for (const auto &v : obj["h"].GetArray())
                    if (v.IsInt())
                        hs.push_back(v.GetInt());
                banner.h = std::move(hs);
            }
            else if (obj["h"].IsInt())
            {
                banner.h = std::vector<int>{obj["h"].GetInt()};
            }
        }

        banner.wmax = get_opt_int(obj, "wmax");
        banner.hmax = get_opt_int(obj, "hmax");
        banner.wmin = get_opt_int(obj, "wmin");
        banner.hmin = get_opt_int(obj, "hmin");
        banner.pos = get_opt_int(obj, "pos");
        banner.btype = get_opt_int_array(obj, "btype");
        banner.battr = get_opt_int_array(obj, "battr");
        if (obj.HasMember("ext"))
            banner.ext = value_to_string(obj["ext"]);
        return banner;
    }

    std::optional<Video> OpenRTBParser::parse_video(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Video video;
        video.mimes = get_opt_int_array(obj, "mimes");
        video.w = get_opt_int(obj, "w");
        video.h = get_opt_int(obj, "h");
        video.startdelay = get_opt_int(obj, "startdelay");
        video.protocols = get_opt_int_array(obj, "protocols");
        video.maxduration = get_opt_int(obj, "maxduration");
        video.api = get_opt_int_array(obj, "api");
        if (obj.HasMember("ext"))
            video.ext = value_to_string(obj["ext"]);
        return video;
    }

    std::optional<Native> OpenRTBParser::parse_native(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Native native_;
        native_.request = get_opt_string(obj, "request");
        native_.ver = get_opt_string(obj, "ver");

        if (obj.HasMember("assets") && obj["assets"].IsArray())
        {
            for (const auto &asset_val : obj["assets"].GetArray())
            {
                if (!asset_val.IsObject())
                    continue;
                std::unordered_map<std::string, std::string> asset;
                for (auto it = asset_val.MemberBegin(); it != asset_val.MemberEnd();
                     ++it)
                {
                    if (it->value.IsString())
                        asset[it->name.GetString()] = it->value.GetString();
                }
                native_.assets.push_back(std::move(asset));
            }
        }

        if (obj.HasMember("ext"))
            native_.ext = value_to_string(obj["ext"]);
        return native_;
    }

    std::optional<Impression>
    OpenRTBParser::parse_impression(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
        {
            last_error_ = "Impression element is not an object";
            return std::nullopt;
        }

        Impression imp;

        if (!obj.HasMember("id") || !obj["id"].IsString())
        {
            last_error_ = "Missing or invalid 'id' in impression";
            return std::nullopt;
        }
        imp.id = std::string(obj["id"].GetString(), obj["id"].GetStringLength());

        if (obj.HasMember("banner") && obj["banner"].IsObject())
            imp.banner = parse_banner(obj["banner"]);

        if (obj.HasMember("video") && obj["video"].IsObject())
            imp.video = parse_video(obj["video"]);

        if (obj.HasMember("native") && obj["native"].IsObject())
            imp.native_ = parse_native(obj["native"]);

        imp.bidfloor = get_opt_double(obj, "bidfloor");
        imp.bidfloorcur = get_opt_string(obj, "bidfloorcur");
        imp.bidfloor_priority = get_opt_bool(obj, "bidfloor_priority");
        imp.secure = get_opt_int_array(obj, "secure");
        imp.iframebuster = get_opt_int_array(obj, "iframebuster");
        if (obj.HasMember("ext"))
            imp.ext = value_to_string(obj["ext"]);

        return imp;
    }

    std::optional<Device> OpenRTBParser::parse_device(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Device device;
        device.ua = get_opt_string(obj, "ua");
        device.ip = get_opt_string(obj, "ip");
        device.ipv6 = get_opt_string(obj, "ipv6");

        if (obj.HasMember("geo") && obj["geo"].IsObject())
            device.geo = parse_geo(obj["geo"]);

        device.devicetype = get_opt_int(obj, "devicetype");
        device.os = get_opt_int(obj, "os");
        device.osv = get_opt_string(obj, "osv");
        device.dnt = get_opt_int(obj, "dnt");
        device.lmt = get_opt_int(obj, "lmt");
        device.connectiontype = get_opt_int(obj, "connectiontype");
        device.carrier = get_opt_string(obj, "carrier");
        device.make = get_opt_string(obj, "make");
        device.model = get_opt_string(obj, "model");
        if (obj.HasMember("ext"))
            device.ext = value_to_string(obj["ext"]);
        return device;
    }

    std::optional<User> OpenRTBParser::parse_user(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        User user;

        if (!obj.HasMember("id") || !obj["id"].IsString())
        {
            last_error_ = "Missing or invalid 'id' in user";
            return std::nullopt;
        }
        user.id = std::string(obj["id"].GetString(), obj["id"].GetStringLength());

        user.buyeruid = get_opt_string(obj, "buyeruid");
        user.yob = get_opt_int(obj, "yob");
        user.gender = get_opt_string(obj, "gender");

        if (obj.HasMember("geo") && obj["geo"].IsObject())
            user.geo = parse_geo(obj["geo"]);

        user.keywords = get_opt_string_array(obj, "keywords");
        if (obj.HasMember("ext"))
            user.ext = value_to_string(obj["ext"]);
        return user;
    }

    std::optional<App> OpenRTBParser::parse_app(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        App app;
        app.id = get_opt_string(obj, "id");
        app.name = get_opt_string(obj, "name");
        app.bundle = get_opt_string(obj, "bundle");
        app.domain = get_opt_string(obj, "domain");
        app.storeurl = get_opt_string(obj, "storeurl");
        app.cat = get_opt_int_array(obj, "cat");
        app.sectioncat = get_opt_string_array(obj, "sectioncat");
        app.pagecat = get_opt_string_array(obj, "pagecat");
        app.ver = get_opt_string(obj, "ver");
        if (obj.HasMember("ext"))
            app.ext = value_to_string(obj["ext"]);
        return app;
    }

    std::optional<Site> OpenRTBParser::parse_site(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
            return std::nullopt;

        Site site;
        site.id = get_opt_string(obj, "id");
        site.name = get_opt_string(obj, "name");
        site.domain = get_opt_string(obj, "domain");
        site.cat = get_opt_int_array(obj, "cat");
        site.sectioncat = get_opt_string_array(obj, "sectioncat");
        site.pagecat = get_opt_string_array(obj, "pagecat");
        site.page = get_opt_string(obj, "page");
        site.ref = get_opt_string(obj, "ref");
        if (obj.HasMember("ext"))
            site.ext = value_to_string(obj["ext"]);
        return site;
    }

    std::optional<OpenRTBRequest>
    OpenRTBParser::parse_request(const rapidjson::Value &obj)
    {
        if (!obj.IsObject())
        {
            last_error_ = "Request is not a JSON object";
            return std::nullopt;
        }

        OpenRTBRequest request;

        if (!obj.HasMember("id") || !obj["id"].IsString())
        {
            last_error_ = "Missing or invalid 'id' in request";
            return std::nullopt;
        }
        request.id = std::string(obj["id"].GetString(), obj["id"].GetStringLength());

        // imp (必填)
        if (!obj.HasMember("imp") || !obj["imp"].IsArray())
        {
            last_error_ = "Missing or invalid 'imp' in request";
            return std::nullopt;
        }

        for (const auto &imp_val : obj["imp"].GetArray())
        {
            auto imp = parse_impression(imp_val);
            if (!imp)
                return std::nullopt; // last_error_ already set
            request.imp.push_back(std::move(*imp));
        }

        if (obj.HasMember("device") && obj["device"].IsObject())
            request.device = parse_device(obj["device"]);

        if (obj.HasMember("user") && obj["user"].IsObject())
            request.user = parse_user(obj["user"]);

        if (obj.HasMember("app") && obj["app"].IsObject())
            request.app = parse_app(obj["app"]);

        if (obj.HasMember("site") && obj["site"].IsObject())
            request.site = parse_site(obj["site"]);

        request.at = get_opt_int(obj, "at");
        request.bcat = get_opt_int_array(obj, "bcat");
        request.badv = get_opt_string_array(obj, "badv");
        request.bapp = get_opt_string_array(obj, "bapp");
        if (obj.HasMember("ext"))
            request.ext = value_to_string(obj["ext"]);

        return request;
    }

    // -------------------------------------------------------------------------
    // Validation
    // -------------------------------------------------------------------------

    bool OpenRTBParser::validate(const OpenRTBRequest &request)
    {
        if (request.id.empty())
            return false;
        if (request.imp.empty())
            return false;
        for (const auto &imp : request.imp)
            if (imp.id.empty())
                return false;
        return true;
    }

    std::vector<std::string>
    OpenRTBParser::validate_detailed(const OpenRTBRequest &request)
    {
        std::vector<std::string> errors;

        if (request.id.empty())
            errors.push_back("Request ID is empty");

        if (request.imp.empty())
            errors.push_back("No impressions in request");

        for (size_t i = 0; i < request.imp.size(); ++i)
        {
            const auto &imp = request.imp[i];
            if (imp.id.empty())
                errors.push_back("Impression " + std::to_string(i) + " has empty ID");

            if (!imp.banner && !imp.video && !imp.native_)
                errors.push_back("Impression " + std::to_string(i) +
                                  " has no ad type (banner/video/native)");
        }

        if (request.device)
        {
            if (!request.device->ua && !request.device->ip)
                errors.push_back("Device must have either 'ua' or 'ip'");
        }

        if (request.user)
        {
            if (request.user->id.empty())
                errors.push_back("User ID is empty");
        }

        return errors;
    }

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    std::string OpenRTBParser::to_json(const OpenRTBRequest &request)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto &alloc = doc.GetAllocator();

        doc.AddMember("id",
                      rapidjson::Value(request.id.c_str(), alloc).Move(), alloc);

        // Impressions
        rapidjson::Value imp_arr(rapidjson::kArrayType);
        for (const auto &imp : request.imp)
        {
            rapidjson::Value imp_obj(rapidjson::kObjectType);
            imp_obj.AddMember("id",
                              rapidjson::Value(imp.id.c_str(), alloc).Move(), alloc);

            if (imp.banner)
            {
                rapidjson::Value b(rapidjson::kObjectType);
                if (imp.banner->w && !imp.banner->w->empty())
                {
                    if (imp.banner->w->size() == 1)
                    {
                        b.AddMember("w", (*imp.banner->w)[0], alloc);
                    }
                    else
                    {
                        rapidjson::Value wa(rapidjson::kArrayType);
                        for (int v : *imp.banner->w)
                            wa.PushBack(v, alloc);
                        b.AddMember("w", wa, alloc);
                    }
                }
                if (imp.banner->h && !imp.banner->h->empty())
                {
                    if (imp.banner->h->size() == 1)
                    {
                        b.AddMember("h", (*imp.banner->h)[0], alloc);
                    }
                    else
                    {
                        rapidjson::Value ha(rapidjson::kArrayType);
                        for (int v : *imp.banner->h)
                            ha.PushBack(v, alloc);
                        b.AddMember("h", ha, alloc);
                    }
                }
                if (imp.banner->pos)
                    b.AddMember("pos", *imp.banner->pos, alloc);
                imp_obj.AddMember("banner", b, alloc);
            }

            if (imp.video)
            {
                rapidjson::Value v(rapidjson::kObjectType);
                if (imp.video->w)
                    v.AddMember("w", *imp.video->w, alloc);
                if (imp.video->h)
                    v.AddMember("h", *imp.video->h, alloc);
                if (imp.video->maxduration)
                    v.AddMember("maxduration", *imp.video->maxduration, alloc);
                imp_obj.AddMember("video", v, alloc);
            }

            if (imp.bidfloor)
                imp_obj.AddMember("bidfloor", *imp.bidfloor, alloc);
            if (imp.bidfloorcur)
                imp_obj.AddMember(
                    "bidfloorcur",
                    rapidjson::Value(imp.bidfloorcur->c_str(), alloc).Move(), alloc);

            imp_arr.PushBack(imp_obj, alloc);
        }
        doc.AddMember("imp", imp_arr, alloc);

        // Device
        if (request.device)
        {
            rapidjson::Value dev(rapidjson::kObjectType);
            if (request.device->ua)
                dev.AddMember("ua",
                              rapidjson::Value(request.device->ua->c_str(), alloc).Move(),
                              alloc);
            if (request.device->ip)
                dev.AddMember("ip",
                              rapidjson::Value(request.device->ip->c_str(), alloc).Move(),
                              alloc);
            if (request.device->devicetype)
                dev.AddMember("devicetype", *request.device->devicetype, alloc);
            if (request.device->os)
                dev.AddMember("os", *request.device->os, alloc);
            if (request.device->osv)
                dev.AddMember("osv",
                              rapidjson::Value(request.device->osv->c_str(), alloc).Move(),
                              alloc);
            if (request.device->connectiontype)
                dev.AddMember("connectiontype", *request.device->connectiontype, alloc);
            if (request.device->geo)
            {
                rapidjson::Value geo_obj(rapidjson::kObjectType);
                if (request.device->geo->country)
                    geo_obj.AddMember("country",
                                      rapidjson::Value(request.device->geo->country->c_str(), alloc).Move(),
                                      alloc);
                if (request.device->geo->region)
                    geo_obj.AddMember("region",
                                      rapidjson::Value(request.device->geo->region->c_str(), alloc).Move(),
                                      alloc);
                if (request.device->geo->city)
                    geo_obj.AddMember("city",
                                      rapidjson::Value(request.device->geo->city->c_str(), alloc).Move(),
                                      alloc);
                if (request.device->geo->lat)
                    geo_obj.AddMember("lat", *request.device->geo->lat, alloc);
                if (request.device->geo->lon)
                    geo_obj.AddMember("lon", *request.device->geo->lon, alloc);
                if (request.device->geo->type)
                    geo_obj.AddMember("type", *request.device->geo->type, alloc);
                dev.AddMember("geo", geo_obj, alloc);
            }
            doc.AddMember("device", dev, alloc);
        }

        // User
        if (request.user)
        {
            rapidjson::Value usr(rapidjson::kObjectType);
            usr.AddMember("id",
                          rapidjson::Value(request.user->id.c_str(), alloc).Move(), alloc);
            if (request.user->buyeruid)
                usr.AddMember("buyeruid",
                              rapidjson::Value(request.user->buyeruid->c_str(), alloc).Move(),
                              alloc);
            if (request.user->yob)
                usr.AddMember("yob", *request.user->yob, alloc);
            if (request.user->gender)
                usr.AddMember("gender",
                              rapidjson::Value(request.user->gender->c_str(), alloc).Move(),
                              alloc);
            if (request.user->keywords)
            {
                rapidjson::Value kw_arr(rapidjson::kArrayType);
                for (const auto &kw : *request.user->keywords)
                    kw_arr.PushBack(rapidjson::Value(kw.c_str(), alloc).Move(), alloc);
                usr.AddMember("keywords", kw_arr, alloc);
            }
            doc.AddMember("user", usr, alloc);
        }

        // App
        if (request.app)
        {
            rapidjson::Value app_obj(rapidjson::kObjectType);
            if (request.app->id)
                app_obj.AddMember("id",
                                  rapidjson::Value(request.app->id->c_str(), alloc).Move(),
                                  alloc);
            if (request.app->name)
                app_obj.AddMember("name",
                                  rapidjson::Value(request.app->name->c_str(), alloc).Move(),
                                  alloc);
            if (request.app->bundle)
                app_obj.AddMember("bundle",
                                  rapidjson::Value(request.app->bundle->c_str(), alloc).Move(),
                                  alloc);
            if (request.app->ver)
                app_obj.AddMember("ver",
                                  rapidjson::Value(request.app->ver->c_str(), alloc).Move(),
                                  alloc);
            if (request.app->cat)
            {
                rapidjson::Value cat_arr(rapidjson::kArrayType);
                for (int c : *request.app->cat)
                    cat_arr.PushBack(c, alloc);
                app_obj.AddMember("cat", cat_arr, alloc);
            }
            doc.AddMember("app", app_obj, alloc);
        }

        // Site
        if (request.site)
        {
            rapidjson::Value site_obj(rapidjson::kObjectType);
            if (request.site->id)
                site_obj.AddMember("id",
                                   rapidjson::Value(request.site->id->c_str(), alloc).Move(),
                                   alloc);
            if (request.site->name)
                site_obj.AddMember("name",
                                   rapidjson::Value(request.site->name->c_str(), alloc).Move(),
                                   alloc);
            if (request.site->domain)
                site_obj.AddMember("domain",
                                   rapidjson::Value(request.site->domain->c_str(), alloc).Move(),
                                   alloc);
            doc.AddMember("site", site_obj, alloc);
        }

        if (request.at)
            doc.AddMember("at", *request.at, alloc);

        if (request.bcat)
        {
            rapidjson::Value bcat_arr(rapidjson::kArrayType);
            for (int c : *request.bcat)
                bcat_arr.PushBack(c, alloc);
            doc.AddMember("bcat", bcat_arr, alloc);
        }

        if (request.badv)
        {
            rapidjson::Value badv_arr(rapidjson::kArrayType);
            for (const auto &d : *request.badv)
                badv_arr.PushBack(rapidjson::Value(d.c_str(), alloc).Move(), alloc);
            doc.AddMember("badv", badv_arr, alloc);
        }

        rapidjson::StringBuffer sb;
        rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
        doc.Accept(writer);
        return sb.GetString();
    }

    std::string OpenRTBParser::to_json_pretty(const OpenRTBRequest &request)
    {
        std::string compact = to_json(request);
        rapidjson::Document doc;
        doc.Parse(compact.c_str());

        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
        doc.Accept(writer);
        return sb.GetString();
    }

} // namespace flow_ad::openrtb
