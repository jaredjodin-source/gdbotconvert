#include <umacro/Formats.hpp>

#include <map>

namespace umacro {

namespace {

/// Player state stored once per physics frame by ReplayEngine (32 bytes,
/// matching the C struct layout including padding).
struct FrameData {
    uint32_t frame = 0;
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    double yVelocity = 0.0;
    bool player2 = false;
};

constexpr size_t ACTION_OLD_SIZE = 8;
constexpr size_t ACTION_NEW_SIZE = 16;

FrameData readFrameData(ByteReader& reader) {
    FrameData data;
    data.frame = reader.u32();
    data.x = reader.f32();
    data.y = reader.f32();
    data.rotation = reader.f32();
    data.yVelocity = reader.f64();
    data.player2 = reader.boolean();
    reader.skip(7);
    return data;
}

struct ActionData {
    uint32_t frame = 0;
    bool down = false;
    uint8_t button = 1;
    bool player2 = false;
};

ActionData readActionNew(ByteReader& reader) {
    ActionData action;
    action.frame = reader.u32();
    action.down = reader.boolean();
    reader.skip(3);
    auto button = reader.i32();
    action.button = button <= 0 ? 1 : static_cast<uint8_t>(button);
    action.player2 = reader.boolean();
    reader.skip(3);
    return action;
}

ActionData readActionOld(ByteReader& reader) {
    ActionData action;
    action.frame = reader.u32();
    action.down = reader.boolean();
    action.player2 = reader.boolean();
    reader.skip(2);
    return action;
}

class ReplayEngine1Format : public Format {
public:
    std::string id() const override { return "re"; }
    std::string name() const override { return "ReplayEngine 1"; }
    std::vector<std::string> extensions() const override { return {".re"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "ReplayEngine";
        macro.framerate = reader.f32();

        auto frameCount = reader.u32();
        auto actionCount = reader.u32();

        std::vector<FrameData> frames;
        frames.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; i++) frames.push_back(readFrameData(reader));

        if (actionCount == 0) throw ParseError("replay has no actions");
        auto actionSize = reader.remaining() / actionCount;
        if (actionSize != ACTION_OLD_SIZE && actionSize != ACTION_NEW_SIZE) {
            throw ParseError("unknown action size " + std::to_string(actionSize));
        }

        std::map<std::pair<uint32_t, bool>, ActionData> actions;
        for (uint32_t i = 0; i < actionCount; i++) {
            auto action = actionSize == ACTION_NEW_SIZE ? readActionNew(reader) : readActionOld(reader);
            actions[{action.frame, action.player2}] = action;
        }

        for (auto const& frame : frames) {
            Input input;
            input.frame = frame.frame;
            input.player2 = frame.player2;
            input.x = frame.x;
            input.y = frame.y;
            input.rotation = frame.rotation;
            input.yVelocity = frame.yVelocity;
            input.hasCorrection = true;

            auto action = actions.find({frame.frame, frame.player2});
            input.down = action != actions.end() && action->second.down;
            input.button = action != actions.end() ? action->second.button : 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

class ReplayEngine2Format : public Format {
public:
    std::string id() const override { return "re2"; }
    std::string name() const override { return "ReplayEngine 2"; }
    std::vector<std::string> extensions() const override { return {".re2"}; }

    bool matches(std::span<const uint8_t> data) const override {
        return data.size() >= 3 && data[0] == 'R' && data[1] == 'E' && data[2] == '2';
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        uint8_t magic[3];
        reader.read(magic, 3);
        if (magic[0] != 'R' || magic[1] != 'E' || magic[2] != '2') throw ParseError("invalid magic");

        Macro macro;
        macro.botName = "ReplayEngine";
        macro.framerate = 240.0; // ReplayEngine 2 replays are always 240 tps

        auto count = reader.u32();
        for (uint32_t i = 0; i < count; i++) {
            auto action = readActionNew(reader);
            Input input;
            input.frame = action.frame;
            input.down = action.down;
            input.button = action.button;
            input.player2 = action.player2;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

/// ReplayEngine 3 stores player 1 and player 2 data in four separate blocks.
class ReplayEngine3Format : public Format {
public:
    std::string id() const override { return "re3"; }
    std::string name() const override { return "ReplayEngine 3"; }
    std::vector<std::string> extensions() const override { return {".re3"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "ReplayEngine";
        macro.framerate = reader.f32();

        auto framesPlayer1 = reader.u32();
        auto framesPlayer2 = reader.u32();
        auto actionsPlayer1 = reader.u32();
        auto actionsPlayer2 = reader.u32();

        std::vector<FrameData> frames;
        frames.reserve(framesPlayer1 + framesPlayer2);
        for (uint32_t i = 0; i < framesPlayer1; i++) frames.push_back(readFrameData(reader));
        for (uint32_t i = 0; i < framesPlayer2; i++) {
            auto frame = readFrameData(reader);
            frame.player2 = true;
            frames.push_back(frame);
        }

        std::map<std::pair<uint32_t, bool>, ActionData> actions;
        for (uint32_t player = 0; player < 2; player++) {
            auto count = player == 0 ? actionsPlayer1 : actionsPlayer2;
            for (uint32_t i = 0; i < count; i++) {
                auto action = readActionNew(reader);
                // the last field is `player1`, not `player2`
                action.player2 = player == 1;
                actions[{action.frame, action.player2}] = action;
            }
        }

        for (auto const& frame : frames) {
            Input input;
            input.frame = frame.frame;
            input.player2 = frame.player2;
            input.x = frame.x;
            input.y = frame.y;
            input.rotation = frame.rotation;
            input.yVelocity = frame.yVelocity;
            input.hasCorrection = true;

            auto action = actions.find({frame.frame, frame.player2});
            input.down = action != actions.end() && action->second.down;
            input.button = action != actions.end() ? action->second.button : 1;
            macro.inputs.push_back(input);
        }

        // actions without matching frame data still have to be replayed
        for (auto const& [key, action] : actions) {
            auto exists = std::any_of(frames.begin(), frames.end(), [&](FrameData const& frame) {
                return frame.frame == key.first && frame.player2 == key.second;
            });
            if (exists) continue;

            Input input;
            input.frame = action.frame;
            input.down = action.down;
            input.button = action.button;
            input.player2 = action.player2;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

}

void registerReplayEngineFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<ReplayEngine1Format>());
    registry.add(std::make_unique<ReplayEngine2Format>());
    registry.add(std::make_unique<ReplayEngine3Format>());
}

}
