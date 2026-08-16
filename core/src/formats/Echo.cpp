#include <umacro/Formats.hpp>
#include <umacro/Json.hpp>

namespace umacro {

namespace {

Macro parseEchoOldJson(nlohmann::json const& json) {
    Macro macro;
    macro.botName = "Echo";
    macro.framerate = jsonNumber(json, "FPS", 240.0);
    auto startingFrame = static_cast<uint64_t>(jsonNumber(json, "Starting Frame", 0.0));

    auto actions = json.find("Echo Replay");
    if (actions == json.end() || !actions->is_array()) throw ParseError("missing 'Echo Replay' array");

    for (auto const& action : *actions) {
        Input input;
        input.frame = static_cast<uint64_t>(jsonNumber(action, "Frame", 0.0)) + startingFrame;
        input.player2 = jsonBool(action, "Player 2", false);
        input.down = jsonBool(action, "Hold", false);
        input.button = 1;
        input.x = static_cast<float>(jsonNumber(action, "X Position", 0.0));
        input.y = static_cast<float>(jsonNumber(action, "Y Position", 0.0));
        input.yVelocity = jsonNumber(action, "Y Acceleration", 0.0);
        input.rotation = static_cast<float>(jsonNumber(action, "Rotation", 0.0));
        input.hasCorrection = input.x != 0.f || input.y != 0.f;
        macro.inputs.push_back(input);
    }
    return macro;
}

Macro parseEchoNewJson(nlohmann::json const& json) {
    Macro macro;
    macro.botName = "Echo";
    macro.framerate = jsonNumber(json, "fps", 240.0);

    auto inputs = json.find("inputs");
    if (inputs == json.end() || !inputs->is_array()) throw ParseError("missing 'inputs' array");

    for (auto const& action : *inputs) {
        Input input;
        input.frame = static_cast<uint64_t>(jsonNumber(action, "frame", 0.0));
        input.down = jsonBool(action, "holding", false);
        input.player2 = jsonBool(action, "player_2", false);
        input.button = 1;
        input.x = static_cast<float>(jsonNumber(action, "x_position", 0.0));
        input.y = static_cast<float>(jsonNumber(action, "y_position", 0.0));
        input.yVelocity = jsonNumber(action, "y_vel", 0.0);
        input.rotation = static_cast<float>(jsonNumber(action, "rotation", 0.0));
        input.hasCorrection = input.x != 0.f || input.y != 0.f;
        macro.inputs.push_back(input);
    }
    return macro;
}

/// Binary Echo replay ("META" magic). Debug replays store player state after
/// every input, which makes each record 24 instead of 6 bytes.
Macro parseEchoBinary(std::span<const uint8_t> data) {
    ByteReader reader(data);
    if (reader.u32be() != 0x4D455441) throw ParseError("invalid magic, expected 'META'");

    auto replayType = reader.u32be();
    bool debug = replayType == 0x44424700;
    size_t recordSize = debug ? 24 : 6;

    Macro macro;
    macro.botName = "Echo";
    reader.seek(24);
    macro.framerate = reader.f32();
    reader.seek(48);

    while (reader.remaining() >= recordSize) {
        Input input;
        input.frame = reader.u32();
        input.down = reader.u8() == 1;
        input.player2 = reader.u8() != 0;
        input.button = 1;
        if (debug) {
            input.x = reader.f32();
            input.yVelocity = reader.f64();
            reader.f64(); // x velocity
            input.y = reader.f32();
            input.rotation = reader.f32();
            input.hasCorrection = true;
        }
        macro.inputs.push_back(input);
    }
    return macro;
}

class EchoFormat : public Format {
public:
    std::string id() const override { return "echo"; }
    std::string name() const override { return "Echo Replay"; }
    std::vector<std::string> extensions() const override { return {".echo"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        if (data.size() >= 4 && data[0] == 'M' && data[1] == 'E' && data[2] == 'T' && data[3] == 'A') return true;
        auto json = parseJson(data);
        return json && json->is_object() && (json->contains("Echo Replay") || json->contains("inputs"));
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        auto json = parseJson(data);
        if (!json) return parseEchoBinary(data);
        if (json->contains("Echo Replay")) return parseEchoOldJson(*json);
        return parseEchoNewJson(*json);
    }

    Bytes exportMacro(Macro const& macro) const override {
        nlohmann::json json;
        json["fps"] = macro.framerate;
        json["inputs"] = nlohmann::json::array();

        for (auto const& input : macro.inputs) {
            nlohmann::json action;
            action["frame"] = input.frame;
            action["holding"] = input.down;
            action["player_2"] = input.player2;
            action["x_position"] = input.x;
            action["y_position"] = input.y;
            action["y_vel"] = input.yVelocity;
            action["rotation"] = input.rotation;
            json["inputs"].push_back(action);
        }
        return jsonToBytes(json);
    }
};

}

void registerEchoFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<EchoFormat>());
}

}
