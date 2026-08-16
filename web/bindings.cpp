#include <umacro/Format.hpp>

#include <emscripten/emscripten.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <string>

namespace {

umacro::Bytes output;
std::string textResult;
std::string lastError;

void setError(std::string message) {
    lastError = std::move(message);
    output.clear();
    textResult.clear();
}

}

extern "C" {

EMSCRIPTEN_KEEPALIVE
char const* umacro_formats() {
    try {
        nlohmann::json formats = nlohmann::json::array();
        for (auto* format : umacro::FormatRegistry::get().formats()) {
            formats.push_back({
                {"id", format->id()},
                {"name", format->name()},
                {"extensions", format->extensions()},
                {"exportable", format->canExport()},
            });
        }
        textResult = formats.dump();
        lastError.clear();
        return textResult.c_str();
    } catch (std::exception const& error) {
        setError(error.what());
        return nullptr;
    }
}

EMSCRIPTEN_KEEPALIVE
char const* umacro_inspect(uint8_t const* data, size_t size, char const* filename) {
    try {
        auto bytes = std::span<const uint8_t>(data, size);
        auto& registry = umacro::FormatRegistry::get();
        auto* format = registry.detect(filename, bytes);
        if (!format) {
            setError("Format de macro non reconnu");
            return nullptr;
        }

        auto imported = registry.import(filename, bytes);
        if (!imported.isOk()) {
            setError(imported.error());
            return nullptr;
        }

        auto const& macro = imported.unwrap();
        nlohmann::json result = {
            {"formatId", format->id()},
            {"formatName", format->name()},
            {"bot", macro.botName},
            {"author", macro.author},
            {"level", macro.levelName},
            {"levelId", macro.levelId},
            {"framerate", macro.framerate},
            {"inputs", macro.inputs.size()},
            {"lastFrame", macro.lastFrame()},
            {"duration", macro.framerate > 0 ? macro.lastFrame() / macro.framerate : 0},
        };
        textResult = result.dump();
        lastError.clear();
        return textResult.c_str();
    } catch (std::exception const& error) {
        setError(error.what());
        return nullptr;
    }
}

EMSCRIPTEN_KEEPALIVE
int umacro_convert(uint8_t const* data, size_t size, char const* filename, char const* formatId) {
    try {
        auto& registry = umacro::FormatRegistry::get();
        auto imported = registry.import(filename, std::span<const uint8_t>(data, size));
        if (!imported.isOk()) {
            setError(imported.error());
            return 0;
        }

        auto exported = registry.exportAs(formatId, imported.unwrap());
        if (!exported.isOk()) {
            setError(exported.error());
            return 0;
        }

        output = std::move(exported.unwrap());
        lastError.clear();
        return 1;
    } catch (std::exception const& error) {
        setError(error.what());
        return 0;
    }
}

EMSCRIPTEN_KEEPALIVE
uintptr_t umacro_output_data() {
    return reinterpret_cast<uintptr_t>(output.data());
}

EMSCRIPTEN_KEEPALIVE
size_t umacro_output_size() {
    return output.size();
}

EMSCRIPTEN_KEEPALIVE
char const* umacro_error() {
    return lastError.c_str();
}

}
