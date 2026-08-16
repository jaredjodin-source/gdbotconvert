#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace umacro {

using Bytes = std::vector<uint8_t>;

/// Thrown by parsers on malformed data. Caught at the registry boundary and
/// turned into an error string.
class ParseError : public std::runtime_error {
public:
    explicit ParseError(std::string const& what) : std::runtime_error(what) {}
};

class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> data) : m_data(data) {}

    [[nodiscard]] size_t position() const { return m_pos; }
    [[nodiscard]] size_t size() const { return m_data.size(); }
    [[nodiscard]] size_t remaining() const { return m_data.size() - m_pos; }
    [[nodiscard]] bool eof() const { return m_pos >= m_data.size(); }

    void seek(size_t pos) {
        if (pos > m_data.size()) throw ParseError("seek out of bounds");
        m_pos = pos;
    }

    void skip(size_t count) {
        if (count > remaining()) throw ParseError("unexpected end of data");
        m_pos += count;
    }

    void read(void* out, size_t count) {
        if (count > remaining()) throw ParseError("unexpected end of data");
        std::memcpy(out, m_data.data() + m_pos, count);
        m_pos += count;
    }

    template <class T>
    T get() {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        read(&value, sizeof(T));
        return value;
    }

    uint8_t u8() { return get<uint8_t>(); }
    bool boolean() { return get<uint8_t>() != 0; }
    int16_t i16() { return get<int16_t>(); }
    uint32_t u32() { return get<uint32_t>(); }
    int32_t i32() { return get<int32_t>(); }
    uint64_t u64() { return get<uint64_t>(); }
    float f32() { return get<float>(); }
    double f64() { return get<double>(); }

    uint32_t u32be() {
        uint8_t b[4];
        read(b, 4);
        return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
    }

    [[nodiscard]] std::span<const uint8_t> data() const { return m_data; }

private:
    std::span<const uint8_t> m_data;
    size_t m_pos = 0;
};

class ByteWriter {
public:
    void write(void const* data, size_t count) {
        auto const* bytes = static_cast<uint8_t const*>(data);
        m_data.insert(m_data.end(), bytes, bytes + count);
    }

    template <class T>
    void put(T value) {
        static_assert(std::is_trivially_copyable_v<T>);
        write(&value, sizeof(T));
    }

    void u8(uint8_t v) { put(v); }
    void boolean(bool v) { put<uint8_t>(v ? 1 : 0); }
    void i16(int16_t v) { put(v); }
    void u32(uint32_t v) { put(v); }
    void i32(int32_t v) { put(v); }
    void f32(float v) { put(v); }
    void f64(double v) { put(v); }

    void string(std::string const& v) { write(v.data(), v.size()); }

    [[nodiscard]] std::vector<uint8_t> const& data() const { return m_data; }
    [[nodiscard]] std::vector<uint8_t> release() { return std::move(m_data); }

private:
    std::vector<uint8_t> m_data;
};

/// Reads a whole file into memory. Throws ParseError when it cannot be read.
std::vector<uint8_t> readFile(std::string const& path);
/// Writes a buffer to disk. Throws ParseError on failure.
void writeFile(std::string const& path, std::span<const uint8_t> data);

}
