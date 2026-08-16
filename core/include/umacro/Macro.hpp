#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace umacro {

enum class Button : uint8_t {
    Jump = 1,
    Left = 2,
    Right = 3,
};

/// A single input of a macro. Mirrors the GDR2 input layout, plus optional
/// physics correction data used by bots that store player state per input.
struct Input {
    uint64_t frame = 0;
    uint8_t button = 1;
    bool player2 = false;
    bool down = false;

    bool hasCorrection = false;
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;
    double yVelocity = 0.0;
};

/// Bot/replay agnostic representation of a macro. Every supported format is
/// converted to and from this structure.
struct Macro {
    std::string author;
    std::string description;
    std::string botName = "UniversalMacro";
    int botVersion = 1;

    double framerate = 240.0;
    float duration = 0.f;
    int gameVersion = 22074;
    int seed = 0;
    int coins = 0;
    bool ldm = false;
    bool platformer = false;

    uint32_t levelId = 0;
    std::string levelName;

    std::vector<Input> inputs;
    std::vector<uint64_t> deaths;

    void sortInputs() {
        std::stable_sort(inputs.begin(), inputs.end(), [](Input const& a, Input const& b) {
            return a.frame < b.frame;
        });
    }

    [[nodiscard]] uint64_t lastFrame() const {
        return inputs.empty() ? 0 : inputs.back().frame;
    }
};

}
