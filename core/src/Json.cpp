#include <umacro/Json.hpp>

namespace umacro {

double jsonNumber(nlohmann::json const& json, std::string const& key, double fallback) {
    if (!json.is_object()) return fallback;
    auto it = json.find(key);
    if (it == json.end()) return fallback;
    if (it->is_number()) return it->get<double>();
    if (it->is_boolean()) return it->get<bool>() ? 1.0 : 0.0;
    if (it->is_string()) {
        try {
            return std::stod(it->get<std::string>());
        } catch (std::exception const&) {
            return fallback;
        }
    }
    return fallback;
}

bool jsonBool(nlohmann::json const& json, std::string const& key, bool fallback) {
    if (!json.is_object()) return fallback;
    auto it = json.find(key);
    if (it == json.end()) return fallback;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number()) return it->get<double>() != 0.0;
    return fallback;
}

std::string jsonString(nlohmann::json const& json, std::string const& key, std::string const& fallback) {
    if (!json.is_object()) return fallback;
    auto it = json.find(key);
    if (it == json.end() || !it->is_string()) return fallback;
    return it->get<std::string>();
}

std::optional<nlohmann::json> parseJson(std::span<const uint8_t> data) {
    auto json = nlohmann::json::parse(data.begin(), data.end(), nullptr, false);
    if (json.is_discarded()) return std::nullopt;
    return json;
}

std::optional<nlohmann::json> parseJsonOrMsgpack(std::span<const uint8_t> data) {
    try {
        return nlohmann::json::from_msgpack(data.begin(), data.end());
    } catch (std::exception const&) {
        return parseJson(data);
    }
}

Bytes jsonToBytes(nlohmann::json const& json) {
    auto dumped = json.dump(2);
    return Bytes(dumped.begin(), dumped.end());
}

}
