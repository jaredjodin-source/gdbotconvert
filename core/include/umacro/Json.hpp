#pragma once

#include <umacro/Bytes.hpp>

#include <nlohmann/json.hpp>
#include <optional>

namespace umacro {

/// Lenient accessors: bots disagree on types (numbers stored as strings,
/// booleans stored as 0/1) so every getter falls back to a default.
double jsonNumber(nlohmann::json const& json, std::string const& key, double fallback);
bool jsonBool(nlohmann::json const& json, std::string const& key, bool fallback);
std::string jsonString(nlohmann::json const& json, std::string const& key, std::string const& fallback = "");

std::optional<nlohmann::json> parseJson(std::span<const uint8_t> data);
/// Tries msgpack first (GDR1 default) and falls back to plain JSON.
std::optional<nlohmann::json> parseJsonOrMsgpack(std::span<const uint8_t> data);

Bytes jsonToBytes(nlohmann::json const& json);

}
