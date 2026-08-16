#include <umacro/Formats.hpp>
#include <umacro/Json.hpp>

#include <gdr/gdr.hpp>
#include <gdr/physics_extension.hpp>

namespace umacro {

namespace {

struct GdrReplay : gdr::Replay<GdrReplay, PhysicsInput> {
    GdrReplay() : Replay("UniversalMacro", 1) {}
};

/// GDReplayFormat 2, the binary format shared by most modern bots.
class Gdr2Format : public Format {
public:
    std::string id() const override { return "gdr2"; }
    std::string name() const override { return "GDReplayFormat 2"; }
    std::vector<std::string> extensions() const override { return {".gdr2"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        return data.size() > 3 && data[0] == 'G' && data[1] == 'D' && data[2] == 'R';
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        std::vector<uint8_t> copy(data.begin(), data.end());
        auto result = GdrReplay::importData(copy);
        if (result.isErr()) throw ParseError(result.unwrapErr());

        auto replay = std::move(result).unwrap();
        Macro macro;
        macro.author = replay.author;
        macro.description = replay.description;
        macro.botName = replay.botInfo.name;
        macro.botVersion = replay.botInfo.version;
        macro.duration = replay.duration;
        macro.gameVersion = replay.gameVersion;
        macro.framerate = replay.framerate;
        macro.seed = replay.seed;
        macro.coins = replay.coins;
        macro.ldm = replay.ldm;
        macro.platformer = replay.platformer;
        macro.levelId = replay.levelInfo.id;
        macro.levelName = replay.levelInfo.name;
        macro.deaths = replay.deaths;

        macro.inputs.reserve(replay.inputs.size());
        for (auto const& input : replay.inputs) {
            Input converted;
            converted.frame = input.frame;
            converted.button = input.button == 0 ? 1 : input.button;
            converted.player2 = input.player2;
            converted.down = input.down;
            converted.x = input.xPosition;
            converted.y = input.yPosition;
            converted.rotation = input.rotation;
            converted.yVelocity = input.yVelocity;
            macro.inputs.push_back(converted);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        GdrReplay replay;
        replay.author = macro.author;
        replay.description = macro.description;
        replay.botInfo = gdr::Bot(macro.botName, macro.botVersion);
        replay.duration = macro.duration;
        replay.gameVersion = macro.gameVersion;
        replay.framerate = macro.framerate;
        replay.seed = macro.seed;
        replay.coins = macro.coins;
        replay.ldm = macro.ldm;
        replay.platformer = macro.platformer;
        replay.levelInfo = gdr::Level(macro.levelName, macro.levelId);
        replay.deaths = macro.deaths;

        replay.inputs.reserve(macro.inputs.size());
        for (auto const& input : macro.inputs) {
            replay.inputs.emplace_back(
                input.frame, input.button, input.player2, input.down,
                input.x, input.y, input.rotation, 0.0, input.yVelocity
            );
        }

        auto result = replay.exportData();
        if (result.isErr()) throw ParseError(result.unwrapErr());
        return std::move(result).unwrap();
    }
};

/// GDReplayFormat 1, stored either as msgpack (.gdr) or as plain JSON.
class Gdr1Format : public Format {
public:
    std::string id() const override { return "gdr"; }
    std::string name() const override { return "GDReplayFormat 1"; }
    std::vector<std::string> extensions() const override { return {".gdr", ".gdr.json"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        // GDR2 also claims `.gdr` sometimes, let it win by magic
        if (data.size() > 3 && data[0] == 'G' && data[1] == 'D' && data[2] == 'R') return false;
        auto json = parseJsonOrMsgpack(data);
        return json && json->is_object() && json->contains("inputs") &&
            (json->contains("framerate") || json->contains("bot") || json->contains("level"));
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        auto parsed = parseJsonOrMsgpack(data);
        if (!parsed) throw ParseError("Not a valid GDR replay (neither msgpack nor JSON)");
        auto const& json = *parsed;

        Macro macro;
        macro.author = jsonString(json, "author");
        macro.description = jsonString(json, "description");
        macro.duration = static_cast<float>(jsonNumber(json, "duration", 0.0));
        macro.gameVersion = static_cast<int>(jsonNumber(json, "gameVersion", 22074));
        macro.framerate = jsonNumber(json, "framerate", 240.0);
        macro.seed = static_cast<int>(jsonNumber(json, "seed", 0.0));
        macro.coins = static_cast<int>(jsonNumber(json, "coins", 0.0));
        macro.ldm = jsonBool(json, "ldm", false);

        if (auto bot = json.find("bot"); bot != json.end() && bot->is_object()) {
            macro.botName = jsonString(*bot, "name");
            auto version = bot->find("version");
            if (version != bot->end()) {
                macro.botVersion = version->is_number()
                    ? version->get<int>()
                    : static_cast<int>(std::atof(version->get<std::string>().c_str()));
            }
        }
        if (auto level = json.find("level"); level != json.end() && level->is_object()) {
            macro.levelId = static_cast<uint32_t>(jsonNumber(*level, "id", 0.0));
            macro.levelName = jsonString(*level, "name");
        }

        auto inputs = json.find("inputs");
        if (inputs == json.end() || !inputs->is_array()) throw ParseError("missing 'inputs' array");

        for (auto const& item : *inputs) {
            Input input;
            input.frame = static_cast<uint64_t>(jsonNumber(item, "frame", 0.0));
            input.button = static_cast<uint8_t>(jsonNumber(item, "btn", 1.0));
            if (input.button == 0) input.button = 1;
            input.player2 = jsonBool(item, "2p", false);
            input.down = jsonBool(item, "down", false);
            input.x = static_cast<float>(jsonNumber(item, "xPos", jsonNumber(item, "x", 0.0)));
            input.y = static_cast<float>(jsonNumber(item, "yPos", jsonNumber(item, "y", 0.0)));
            input.rotation = static_cast<float>(jsonNumber(item, "rot", jsonNumber(item, "rotation", 0.0)));
            input.yVelocity = jsonNumber(item, "yVel", jsonNumber(item, "yAccel", 0.0));
            input.hasCorrection = input.x != 0.f || input.y != 0.f;
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        nlohmann::json json;
        json["gameVersion"] = static_cast<float>(macro.gameVersion);
        json["description"] = macro.description;
        json["version"] = 1.0f;
        json["duration"] = macro.duration;
        json["bot"]["name"] = macro.botName;
        json["bot"]["version"] = std::to_string(macro.botVersion);
        json["level"]["id"] = macro.levelId;
        json["level"]["name"] = macro.levelName;
        json["author"] = macro.author;
        json["seed"] = macro.seed;
        json["coins"] = macro.coins;
        json["ldm"] = macro.ldm;
        json["framerate"] = static_cast<float>(macro.framerate);
        json["inputs"] = nlohmann::json::array();

        for (auto const& input : macro.inputs) {
            nlohmann::json item;
            item["frame"] = input.frame;
            item["btn"] = input.button;
            item["2p"] = input.player2;
            item["down"] = input.down;
            if (input.hasCorrection) {
                item["xPos"] = input.x;
                item["yPos"] = input.y;
                item["rot"] = input.rotation;
                item["yVel"] = input.yVelocity;
            }
            json["inputs"].push_back(item);
        }

        return nlohmann::json::to_msgpack(json);
    }
};

}

void registerGdrFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<Gdr2Format>());
    registry.add(std::make_unique<Gdr1Format>());
}

}
