#include <umacro/Formats.hpp>

namespace umacro {

namespace {

/// zBot frame replays: a delta time / speedhack header followed by 6 byte records.
class ZBotFormat : public Format {
public:
    std::string id() const override { return "zbf"; }
    std::string name() const override { return "zBot Frame"; }
    std::vector<std::string> extensions() const override { return {".zbf"}; }
    bool canExport() const override { return true; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        auto delta = reader.f32();
        auto speedhack = reader.f32();
        if (speedhack == 0.f) speedhack = 1.f;
        if (delta == 0.f) throw ParseError("delta is zero");

        Macro macro;
        macro.botName = "zBot";
        macro.framerate = 1.0 / static_cast<double>(delta) / static_cast<double>(speedhack);

        while (reader.remaining() >= 6) {
            Input input;
            input.frame = static_cast<uint64_t>(reader.i32());
            input.down = reader.u8() == 0x31;
            input.player2 = reader.u8() != 0x31;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        ByteWriter writer;
        writer.f32(static_cast<float>(1.0 / macro.framerate));
        writer.f32(1.f);
        for (auto const& input : macro.inputs) {
            writer.i32(static_cast<int32_t>(input.frame));
            writer.u8(input.down ? 0x31 : 0x30);
            writer.u8(input.player2 ? 0x30 : 0x31);
        }
        return writer.release();
    }
};

/// ReplayBot v2 frame replays ("RPLY" magic).
class ReplayBotFormat : public Format {
public:
    std::string id() const override { return "replaybot"; }
    std::string name() const override { return "ReplayBot"; }
    std::vector<std::string> extensions() const override { return {".replay"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        return data.size() >= 4 && data[0] == 'R' && data[1] == 'P' && data[2] == 'L' && data[3] == 'Y';
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        uint8_t magic[4];
        reader.read(magic, 4);
        if (magic[0] != 'R' || magic[1] != 'P' || magic[2] != 'L' || magic[3] != 'Y') {
            throw ParseError("old ReplayBot replays are not supported, they do not store frames");
        }
        if (auto version = reader.u8(); version != 2) {
            throw ParseError("unsupported ReplayBot version " + std::to_string(version));
        }
        if (reader.u8() != 1) throw ParseError("only frame replays are supported");

        Macro macro;
        macro.botName = "ReplayBot";
        macro.framerate = reader.f32();

        while (reader.remaining() >= 5) {
            Input input;
            input.frame = reader.u32();
            auto state = reader.u8();
            input.down = (state & 1) != 0;
            input.player2 = (state >> 1) != 0;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        ByteWriter writer;
        writer.string("RPLY");
        writer.u8(2);
        writer.u8(1);
        writer.f32(static_cast<float>(macro.framerate));
        for (auto const& input : macro.inputs) {
            writer.u32(static_cast<uint32_t>(input.frame));
            writer.u8(static_cast<uint8_t>((input.down ? 1 : 0) | (input.player2 ? 2 : 0)));
        }
        return writer.release();
    }
};

class KdBotFormat : public Format {
public:
    std::string id() const override { return "kd"; }
    std::string name() const override { return "KD-Bot"; }
    std::vector<std::string> extensions() const override { return {".kd"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "KD-Bot";
        macro.framerate = reader.f32();

        while (reader.remaining() >= 6) {
            Input input;
            input.frame = static_cast<uint64_t>(reader.i32());
            input.down = reader.u8() == 1;
            input.player2 = reader.u8() == 1;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

class RushFormat : public Format {
public:
    std::string id() const override { return "rush"; }
    std::string name() const override { return "Rush"; }
    std::vector<std::string> extensions() const override { return {".rsh", ".rush"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "Rush";
        macro.framerate = reader.i16();

        while (reader.remaining() >= 5) {
            Input input;
            input.frame = static_cast<uint64_t>(reader.i32());
            auto state = reader.u8();
            input.down = (state & 1) != 0;
            input.player2 = (state >> 1) != 0;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

/// DDHOR frame replays ("DDHR" magic). Player 1 and player 2 inputs are stored
/// in two consecutive blocks, and 0 means "down".
class DdhorFormat : public Format {
public:
    std::string id() const override { return "ddhor"; }
    std::string name() const override { return "DDHOR"; }
    std::vector<std::string> extensions() const override { return {".ddhor"}; }

    bool matches(std::span<const uint8_t> data) const override {
        return data.size() >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'H' && data[3] == 'R';
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        uint8_t magic[4];
        reader.read(magic, 4);
        if (magic[0] != 'D' || magic[1] != 'D' || magic[2] != 'H' || magic[3] != 'R') {
            throw ParseError("DDHOR JSON replays are not supported, they do not store frames");
        }

        Macro macro;
        macro.botName = "DDHOR";
        macro.framerate = reader.i16();
        auto countPlayer1 = static_cast<uint32_t>(reader.i32());
        reader.i32(); // player 2 count

        for (uint32_t index = 0; reader.remaining() >= 5; index++) {
            Input input;
            input.frame = static_cast<uint64_t>(reader.f32());
            input.down = reader.u8() == 0;
            input.player2 = index >= countPlayer1;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

class YBotFrameFormat : public Format {
public:
    std::string id() const override { return "ybf"; }
    std::string name() const override { return "yBot Frame"; }
    std::vector<std::string> extensions() const override { return {".ybf"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "yBot";
        macro.framerate = reader.f32();
        auto count = reader.i32();

        for (int32_t i = 0; i < count && reader.remaining() >= 8; i++) {
            Input input;
            input.frame = reader.u32();
            auto state = reader.u32();
            input.down = (state & 0b10) != 0;
            input.player2 = (state & 0b01) != 0;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

/// GD Mega Overlay (pre Geode) binary macros.
class GdmoFormat : public Format {
public:
    std::string id() const override { return "gdmo"; }
    std::string name() const override { return "GD Mega Overlay"; }
    std::vector<std::string> extensions() const override { return {".macro"}; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        ByteReader reader(data);
        Macro macro;
        macro.botName = "GD Mega Overlay";
        macro.framerate = reader.f32();
        auto count = reader.u32();
        reader.u32(); // frame capture count

        for (uint32_t i = 0; i < count && reader.remaining() >= 24; i++) {
            Input input;
            input.down = reader.u8() != 0;
            input.player2 = reader.u8() != 0;
            reader.skip(2); // padding
            input.frame = reader.u32();
            input.yVelocity = reader.f64();
            input.x = reader.f32();
            input.y = reader.f32();
            input.button = 1;
            input.hasCorrection = true;
            macro.inputs.push_back(input);
        }
        return macro;
    }
};

}

void registerSimpleBinaryFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<ZBotFormat>());
    registry.add(std::make_unique<ReplayBotFormat>());
    registry.add(std::make_unique<KdBotFormat>());
    registry.add(std::make_unique<RushFormat>());
    registry.add(std::make_unique<DdhorFormat>());
    registry.add(std::make_unique<YBotFrameFormat>());
    registry.add(std::make_unique<GdmoFormat>());
}

}
