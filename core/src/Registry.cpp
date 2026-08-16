#include <umacro/Format.hpp>
#include <umacro/Formats.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace umacro {

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

std::string extensionOf(std::string const& filename) {
    auto slash = filename.find_last_of("/\\");
    auto name = slash == std::string::npos ? filename : filename.substr(slash + 1);
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos) return "";

    // keep double extensions like `.mhr.json` intact
    auto prevDot = name.find_last_of('.', dot - 1);
    if (prevDot != std::string::npos && dot - prevDot <= 5) {
        return toLower(name.substr(prevDot));
    }
    return toLower(name.substr(dot));
}

std::vector<uint8_t> readFile(std::string const& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw ParseError("Failed to open " + path);

    auto size = file.tellg();
    if (size < 0) throw ParseError("Failed to read " + path);
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!data.empty()) file.read(reinterpret_cast<char*>(data.data()), size);
    if (!file) throw ParseError("Failed to read " + path);
    return data;
}

void writeFile(std::string const& path, std::span<const uint8_t> data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw ParseError("Failed to open " + path + " for writing");
    file.write(reinterpret_cast<char const*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) throw ParseError("Failed to write " + path);
}

FormatRegistry& FormatRegistry::get() {
    static FormatRegistry instance = [] {
        FormatRegistry registry;
        registerBuiltinFormats(registry);
        return registry;
    }();
    return instance;
}

void FormatRegistry::add(std::unique_ptr<Format> format) {
    m_formats.push_back(std::move(format));
}

std::vector<Format*> FormatRegistry::formats() const {
    std::vector<Format*> out;
    out.reserve(m_formats.size());
    for (auto const& format : m_formats) out.push_back(format.get());
    return out;
}

std::vector<Format*> FormatRegistry::importFormats() const {
    std::vector<Format*> out;
    for (auto const& format : m_formats) {
        if (format->canImport()) out.push_back(format.get());
    }
    return out;
}

std::vector<Format*> FormatRegistry::exportFormats() const {
    std::vector<Format*> out;
    for (auto const& format : m_formats) {
        if (format->canExport()) out.push_back(format.get());
    }
    return out;
}

Format* FormatRegistry::byId(std::string const& id) const {
    for (auto const& format : m_formats) {
        if (format->id() == id) return format.get();
    }
    return nullptr;
}

Format* FormatRegistry::detect(std::string const& filename, std::span<const uint8_t> data) const {
    auto extension = extensionOf(filename);

    std::vector<Format*> candidates;
    for (auto const& format : m_formats) {
        if (!format->canImport()) continue;
        auto exts = format->extensions();
        if (std::find(exts.begin(), exts.end(), extension) != exts.end()) {
            candidates.push_back(format.get());
        }
    }
    // Content signatures win over extensions. This handles files that were
    // renamed or use a shared extension such as .json and .replay.
    for (auto const& format : m_formats) {
        if (format->canImport() && format->matches(data)) return format.get();
    }
    if (!candidates.empty()) return candidates.front();
    return nullptr;
}

Result<Macro> FormatRegistry::import(std::string const& filename, std::span<const uint8_t> data) const {
    auto* format = detect(filename, data);
    if (!format) return Result<Macro>::err("Unrecognized macro format for " + filename);

    try {
        auto macro = format->importMacro(data);
        macro.sortInputs();
        return Result<Macro>::ok(std::move(macro));
    } catch (std::exception const& e) {
        return Result<Macro>::err(format->name() + ": " + e.what());
    }
}

Result<Bytes> FormatRegistry::exportAs(std::string const& formatId, Macro const& macro) const {
    auto* format = byId(formatId);
    if (!format) return Result<Bytes>::err("Unknown format " + formatId);
    if (!format->canExport()) return Result<Bytes>::err(format->name() + " cannot be exported to");

    try {
        return Result<Bytes>::ok(format->exportMacro(macro));
    } catch (std::exception const& e) {
        return Result<Bytes>::err(format->name() + ": " + e.what());
    }
}

}
