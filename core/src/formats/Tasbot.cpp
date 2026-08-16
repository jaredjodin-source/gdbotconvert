#include <umacro/Formats.hpp>
#include <umacro/Json.hpp>

namespace umacro {

namespace {

/// TASbot stores a per frame state (0 = nothing, 1 = click, 2 = release) for
/// both players instead of edge triggered inputs.
class TasbotFormat : public Format {
public:
    std::string id() const override { return "tasbot"; }
    std::string name() const override { return "TASbot Replay"; }
    std::vector<std::string> extensions() const override { return {".json"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        auto json = parseJson(data);
        return json && json->is_object() && json->contains("macro") && json->contains("fps");
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        auto parsed = parseJson(data);
        if (!parsed) throw ParseError("invalid JSON");
        auto const& json = *parsed;

        Macro macro;
        macro.botName = "TASbot";
        macro.framerate = jsonNumber(json, "fps", 240.0);

        auto events = json.find("macro");
        if (events == json.end() || !events->is_array()) throw ParseError("missing 'macro' array");

        int previous[2] = {0, 0};
        for (auto const& event : *events) {
            auto frame = static_cast<uint64_t>(jsonNumber(event, "frame", 0.0));
            for (int player = 0; player < 2; player++) {
                auto key = player == 0 ? "player_1" : "player_2";
                auto it = event.find(key);
                if (it == event.end()) continue;

                auto click = static_cast<int>(jsonNumber(*it, "click", 0.0));
                if (click == 0) {
                    previous[player] = click;
                    continue;
                }

                Input input;
                input.frame = frame;
                input.player2 = player == 1;
                input.button = 1;
                input.x = static_cast<float>(jsonNumber(*it, "x_position", 0.0));
                input.hasCorrection = input.x != 0.f;

                // two consecutive clicks imply an implicit release in between
                if (click == 1 && previous[player] == 1) {
                    auto release = input;
                    release.down = false;
                    macro.inputs.push_back(release);
                }

                input.down = click == 1;
                macro.inputs.push_back(input);
                previous[player] = click;
            }
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        nlohmann::json json;
        json["fps"] = macro.framerate;
        json["macro"] = nlohmann::json::array();

        for (auto const& input : macro.inputs) {
            nlohmann::json event;
            event["frame"] = input.frame;
            for (int player = 0; player < 2; player++) {
                auto key = player == 0 ? "player_1" : "player_2";
                bool isThisPlayer = (player == 1) == input.player2;
                event[key]["click"] = isThisPlayer ? (input.down ? 1 : 2) : 0;
                event[key]["x_position"] = isThisPlayer ? input.x : 0.f;
            }
            json["macro"].push_back(event);
        }
        return jsonToBytes(json);
    }
};

}

void registerTasbotFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<TasbotFormat>());
}

}
