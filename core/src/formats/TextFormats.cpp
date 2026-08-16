#include <umacro/Formats.hpp>

#include <charconv>
#include <sstream>

namespace umacro {

namespace {

std::string asString(std::span<const uint8_t> data) {
    return std::string(reinterpret_cast<char const*>(data.data()), data.size());
}

bool isPrintable(std::span<const uint8_t> data, size_t limit = 64) {
    auto count = std::min(limit, data.size());
    if (count == 0) return false;
    for (size_t i = 0; i < count; i++) {
        auto c = data[i];
        if (c != '\n' && c != '\r' && c != '\t' && (c < 0x20 || c > 0x7e)) return false;
    }
    return true;
}

std::vector<std::string> split(std::string const& line, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, delimiter)) parts.push_back(part);
    return parts;
}

double parseNumber(std::string const& text, char const* what) {
    try {
        return std::stod(text);
    } catch (std::exception const&) {
        throw ParseError(std::string("failed to parse ") + what + " ('" + text + "')");
    }
}

/// xdBot text macros: `frame|holding|button|player1|pos_only|x|y|...`,
/// optionally preceded by a single line holding the framerate.
class XdBotFormat : public Format {
public:
    std::string id() const override { return "xdbot"; }
    std::string name() const override { return "xdBot"; }
    std::vector<std::string> extensions() const override { return {".xd"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        if (!isPrintable(data)) return false;
        return asString(data.subspan(0, std::min<size_t>(data.size(), 512))).find('|') != std::string::npos;
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        Macro macro;
        macro.botName = "xdBot";

        std::stringstream stream(asString(data));
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            auto parts = split(line, '|');
            if (parts.size() == 1) {
                macro.framerate = parseNumber(parts[0], "framerate");
                continue;
            }
            if (parts.size() < 4) continue;

            Input input;
            input.frame = static_cast<uint64_t>(parseNumber(parts[0], "frame"));
            input.down = parseNumber(parts[1], "holding") == 1;
            input.button = static_cast<uint8_t>(parseNumber(parts[2], "button"));
            if (input.button == 0) input.button = 1;
            input.player2 = parseNumber(parts[3], "player1") != 1;
            if (parts.size() >= 7) {
                input.x = static_cast<float>(parseNumber(parts[5], "x"));
                input.y = static_cast<float>(parseNumber(parts[6], "y"));
                input.hasCorrection = true;
            }
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        std::string out = std::to_string(macro.framerate) + "\n";
        for (auto const& input : macro.inputs) {
            out += std::to_string(input.frame) + "|" + (input.down ? "1" : "0") + "|" +
                std::to_string(input.button) + "|" + (input.player2 ? "0" : "1") + "|0|" +
                std::to_string(input.x) + "|" + std::to_string(input.y) + "\n";
        }
        return Bytes(out.begin(), out.end());
    }
};

/// xBot frame macros: framerate, the literal line "frames", then `state frame`.
class XBotFormat : public Format {
public:
    std::string id() const override { return "xbot"; }
    std::string name() const override { return "xBot Frame"; }
    std::vector<std::string> extensions() const override { return {".xbot"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override {
        if (!isPrintable(data)) return false;
        return asString(data.subspan(0, std::min<size_t>(data.size(), 64))).find("frames") != std::string::npos;
    }

    Macro importMacro(std::span<const uint8_t> data) const override {
        std::stringstream stream(asString(data));
        std::string line;

        if (!std::getline(stream, line)) throw ParseError("empty file");
        Macro macro;
        macro.botName = "xBot";
        macro.framerate = parseNumber(line, "framerate");

        if (!std::getline(stream, line)) throw ParseError("missing mode line");
        if (line.find("frames") == std::string::npos) {
            throw ParseError("only xBot frame replays are supported");
        }

        while (std::getline(stream, line)) {
            auto parts = split(line, ' ');
            if (parts.size() < 2) continue;

            auto state = static_cast<int>(parseNumber(parts[0], "state"));
            Input input;
            input.frame = static_cast<uint64_t>(parseNumber(parts[1], "frame"));
            input.player2 = state > 1;
            input.down = state % 2 == 1;
            input.button = 1;
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        std::string out = std::to_string(static_cast<int64_t>(macro.framerate)) + "\nframes\n";
        for (auto const& input : macro.inputs) {
            int state = (input.player2 ? 2 : 0) + (input.down ? 1 : 0);
            out += std::to_string(state) + " " + std::to_string(input.frame) + "\n";
        }
        return Bytes(out.begin(), out.end());
    }
};

/// Plain text: framerate on the first line, then `frame down button player1`.
class PlainTextFormat : public Format {
public:
    std::string id() const override { return "txt"; }
    std::string name() const override { return "Plain Text"; }
    std::vector<std::string> extensions() const override { return {".txt"}; }
    bool canExport() const override { return true; }

    bool matches(std::span<const uint8_t> data) const override { return isPrintable(data); }

    Macro importMacro(std::span<const uint8_t> data) const override {
        std::stringstream stream(asString(data));
        std::string line;
        if (!std::getline(stream, line)) throw ParseError("empty file");

        Macro macro;
        macro.framerate = parseNumber(line, "framerate");

        while (std::getline(stream, line)) {
            auto parts = split(line, ' ');
            if (parts.size() < 4) continue;

            Input input;
            input.frame = static_cast<uint64_t>(parseNumber(parts[0], "frame"));
            input.down = parseNumber(parts[1], "down") == 1;
            input.button = static_cast<uint8_t>(parseNumber(parts[2], "button"));
            if (input.button == 0) input.button = 1;
            input.player2 = parseNumber(parts[3], "player1") == 0;
            macro.inputs.push_back(input);
        }
        return macro;
    }

    Bytes exportMacro(Macro const& macro) const override {
        std::string out = std::to_string(macro.framerate) + "\n";
        for (auto const& input : macro.inputs) {
            out += std::to_string(input.frame) + " " + (input.down ? "1" : "0") + " " +
                std::to_string(input.button) + " " + (input.player2 ? "0" : "1") + "\n";
        }
        return Bytes(out.begin(), out.end());
    }
};

}

void registerTextFormats(FormatRegistry& registry) {
    registry.add(std::make_unique<XdBotFormat>());
    registry.add(std::make_unique<XBotFormat>());
    registry.add(std::make_unique<PlainTextFormat>());
}

}
