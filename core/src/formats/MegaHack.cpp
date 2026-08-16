#include <umacro/Formats.hpp>
#include <umacro/Json.hpp>

namespace umacro {

namespace {

Macro macroFromMhrJson(nlohmann::json const& json) {
    auto meta = json.find("meta");
    if (meta == json.end() || !meta->is_object()) throw ParseError("missing 'meta' object");

    Macro macro;
    macro.botName = "Mega Hack";
    macro.framerate = jsonNumber(*meta, "fps", 240.0);

    auto events = json.find("events");
    if (events == json.end() || !events->is_array()) throw ParseError("missing 'events' array");

    for (auto const& event : *events) {
        if (!event.is_object() || event.find("down") == event.end()) continue;

        Input input;
        input.frame = static_cast<uint64_t>(jsonNumber(event, "frame", 0.0));
        input.down = jsonBool(event, "down", false);
        input.player2 = jsonBool(event, "p2", false);
        input.button = 1;
        input.x = static_cast<float>(jsonNumber(event, "x", 0.0));
        input.y = static_cast<float>(jsonNumber(event, "y", 0.0));
        input.rotation = static_cast<float>(jsonNumber(event, "r", 0.0));
        input.yVelocity = jsonNumber(event, "a", 0.0);
        input.hasCorrection = input.x != 0.f || input.y != 0.f;
        macro.inputs.push_back(input);
    }
    return macro;
}

bool looksLikeMhrJson(std::span<const uint8_t> data) {
    auto json = parseJson(data);
    if (!json || !json->is_object()) return false;
    auto meta = json->find("meta");
    return meta != json->end() && meta->is_object() && meta->find("fps") != meta->end();
}

class MhrJsonFormat : public Format {
public:
    std::string id() const override { return "mhrjson"; }
    std::string name() const override { return "Mega Hack Replay (JSON)"; }
    std::vector<std::string> extensions() const override { return {".mhr.json", ".json"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override { return looksLikeMhrJson(data); }

    Macro importMacro(std::span<const uint8_t> data) const override {
        auto json = parseJson(data);
        if (!json) throw ParseError("invalid JSON");
        return macroFromMhrJson(*json);
    }

    Bytes exportMacro(Macro const& macro) const override {
        nlohmann::json json;
        json["_"] = "Exported by Universal Macro";
        json["meta"]["fps"] = macro.framerate;
        json["events"] = nlohmann::json::array();

        for (auto const& input : macro.inputs) {
            nlohmann::json event;
            event["frame"] = input.frame;
            event["down"] = input.down;
            if (input.player2) event["p2"] = true;
            if (input.hasCorrection) {
                event["x"] = input.x;
                event["y"] = input.y;
                event["r"] = input.rotation;
                event["a"] = input.yVelocity;
            }
            json["events"].push_back(event);
        }
        return jsonToBytes(json);
    }
};

/// Binary Mega Hack replay, identified by the "HACK" magic.
class MhrBinaryFormat : public Format {
public:
    std::string id() const override { return "mhrbin"; }
    std::string name() const override { return "Mega Hack Replay (binary)"; }
    std::vector<std::string> extensions() const override { return {".mhr"}; }

    bool matches(std::span<const uint8_t> data) const override {
        return data.size() >= 4 && data[0] == 'H' && data[1] == 'A' && data[2] == 'C' && data[3] == 'K';
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        // a .mhr file may actually contain the JSON variant
        if (looksLikeMhrJson(data)) {
            auto json = parseJson(data);
            return macroFromMhrJson(*json);
        }

        ByteReader reader(data);
        if (reader.u32be() != 0x4841434B) throw ParseError("invalid magic, expected 'HACK'");

        Macro macro;
        macro.botName = "Mega Hack";
        reader.seek(12);
        macro.framerate = reader.u32();
        reader.seek(28);
        auto count = reader.u32();

        for (uint32_t i = 0; i < count; i++) {
            reader.skip(2);
            Input input;
            input.down = reader.u8() == 1;
            input.player2 = reader.u8() != 0;
            input.frame = reader.u32();
            input.button = 1;
            reader.skip(24);
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

}

void registerMegaHackFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<MhrJsonFormat>());
    registry.add(std::make_unique<MhrBinaryFormat>());
}

}
