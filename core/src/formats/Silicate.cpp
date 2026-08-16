#include <umacro/Formats.hpp>

namespace umacro {

namespace {

/// Silicate 1: every input is packed into a single 32 bit word.
/// [ frame (28 bits) | player2 | button (2 bits) | down ]
class SilicateFormat : public Format {
public:
    std::string id() const override { return "slc"; }
    std::string name() const override { return "Silicate"; }
    std::vector<std::string> extensions() const override { return {".slc"}; }
    bool canExport() const override { return true; }

    Macro importMacro(std::span<const uint8_t> data) const override {
        if (data.size() >= 4) {
            std::string_view header(reinterpret_cast<char const*>(data.data()), 4);
            if (header == "SILL") throw ParseError("Silicate 2 replays are not supported yet");
            if (header == "SLC3") throw ParseError("Silicate 3 replays are not supported yet");
        }

        ByteReader reader(data);
        Macro macro;
        macro.botName = "Silicate";
        macro.framerate = reader.f64();
        auto count = reader.u32();

        for (uint32_t i = 0; i < count; i++) {
            auto packed = reader.u32();
            Input input;
            input.frame = packed >> 4;
            input.player2 = (packed & 0b1000) != 0;
            input.down = (packed & 0b0001) != 0;
            auto button = (packed & 0b0110) >> 1;
            input.button = button == 0 ? 1 : static_cast<uint8_t>(button);
            macro.inputs.push_back(input);
        }

        if (reader.remaining() >= 8) macro.seed = static_cast<int>(reader.u64());
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        ByteWriter writer;
        writer.f64(macro.framerate);
        writer.u32(static_cast<uint32_t>(macro.inputs.size()));

        for (auto const& input : macro.inputs) {
            if (input.frame > 0x0FFFFFFF) throw ParseError("frame does not fit in 28 bits");
            uint32_t packed = static_cast<uint32_t>(input.frame) << 4;
            packed |= input.player2 ? 0b1000 : 0;
            packed |= static_cast<uint32_t>(input.button & 0b11) << 1;
            packed |= input.down ? 1 : 0;
            writer.u32(packed);
        }
        writer.put<uint64_t>(static_cast<uint64_t>(macro.seed));
        return writer.release();
    }
};

}

void registerSilicateFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<SilicateFormat>());
}

}
