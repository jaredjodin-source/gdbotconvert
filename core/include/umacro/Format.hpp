#pragma once

#include <umacro/Bytes.hpp>
#include <umacro/Macro.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace umacro {

/// Minimal result type so the core library stays independent from Geode.
template <class T>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value), {}); }
    static Result err(std::string message) { return Result(std::nullopt, std::move(message)); }

    [[nodiscard]] bool isOk() const { return m_value.has_value(); }
    [[nodiscard]] std::string const& error() const { return m_error; }
    T& unwrap() { return *m_value; }
    T const& unwrap() const { return *m_value; }

private:
    Result(std::optional<T> value, std::string error)
        : m_value(std::move(value)), m_error(std::move(error)) {}

    std::optional<T> m_value;
    std::string m_error;
};

class Format {
public:
    virtual ~Format() = default;

    /// Stable identifier used in settings and in the UI.
    [[nodiscard]] virtual std::string id() const = 0;
    /// Human readable name, e.g. "Mega Hack Replay (JSON)".
    [[nodiscard]] virtual std::string name() const = 0;
    /// Extensions this format claims, lowercase and with the leading dot.
    [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;

    [[nodiscard]] virtual bool canImport() const { return true; }
    [[nodiscard]] virtual bool canExport() const { return false; }

    /// Content based detection, used when the extension is ambiguous
    /// (.json and .replay are claimed by several bots).
    [[nodiscard]] virtual bool matches(std::span<const uint8_t>) const { return false; }

    /// Throws ParseError on malformed data.
    [[nodiscard]] virtual Macro importMacro(std::span<const uint8_t> data) const = 0;
    [[nodiscard]] virtual Bytes exportMacro(Macro const&) const {
        throw ParseError(name() + " does not support exporting");
    }
};

class FormatRegistry {
public:
    static FormatRegistry& get();

    [[nodiscard]] std::vector<Format*> formats() const;
    [[nodiscard]] std::vector<Format*> importFormats() const;
    [[nodiscard]] std::vector<Format*> exportFormats() const;
    [[nodiscard]] Format* byId(std::string const& id) const;

    /// Picks a format from the file name and the file contents. Content
    /// detection wins over the extension, since bots frequently reuse both
    /// `.json` and `.replay`.
    [[nodiscard]] Format* detect(std::string const& filename, std::span<const uint8_t> data) const;

    /// Convenience wrappers that translate ParseError into an error result.
    [[nodiscard]] Result<Macro> import(std::string const& filename, std::span<const uint8_t> data) const;
    [[nodiscard]] Result<Bytes> exportAs(std::string const& formatId, Macro const& macro) const;

    void add(std::unique_ptr<Format> format);

private:
    std::vector<std::unique_ptr<Format>> m_formats;
};

/// Registers every built in format. Called automatically by FormatRegistry::get.
void registerBuiltinFormats(FormatRegistry& registry);

/// Lowercases a string, used for extension comparisons.
std::string toLower(std::string str);
/// Returns the (lowercase) extension of a path, including the leading dot.
/// Double extensions such as `.mhr.json` are returned in full.
std::string extensionOf(std::string const& filename);

}
