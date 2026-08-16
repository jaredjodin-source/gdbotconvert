#include <umacro/Format.hpp>
#include <umacro/Formats.hpp>

#include <cmath>
#include <cstdio>
#include <string>

using namespace umacro;

namespace {

int g_failures = 0;

void check(bool condition, std::string const& what) {
    if (condition) return;
    g_failures++;
    std::printf("  FAIL %s\n", what.c_str());
}

Macro sampleMacro() {
    Macro macro;
    macro.author = "devin";
    macro.description = "test macro";
    macro.framerate = 240.0;
    macro.levelId = 128;
    macro.levelName = "Test Level";

    for (uint64_t frame = 10; frame < 100; frame += 10) {
        Input input;
        input.frame = frame;
        input.down = (frame / 10) % 2 == 1;
        input.button = 1;
        input.player2 = frame >= 60;
        macro.inputs.push_back(input);
    }
    return macro;
}

/// Exports the macro and reads it back with the same format, then checks that
/// the frames, buttons, players and hold states survived the round trip.
void testRoundTrip(Format* format) {
    std::printf("- %s (%s)\n", format->name().c_str(), format->id().c_str());
    auto macro = sampleMacro();

    Bytes bytes;
    try {
        bytes = format->exportMacro(macro);
    }
    catch (std::exception const& error) {
        check(false, std::string("export threw: ") + error.what());
        return;
    }

    Macro parsed;
    try {
        parsed = format->importMacro(bytes);
    }
    catch (std::exception const& error) {
        check(false, std::string("import threw: ") + error.what());
        return;
    }

    check(parsed.inputs.size() == macro.inputs.size(), "input count differs");
    if (parsed.inputs.size() != macro.inputs.size()) return;
    check(std::abs(parsed.framerate - macro.framerate) < 0.5, "framerate differs");

    for (size_t i = 0; i < macro.inputs.size(); i++) {
        auto const& expected = macro.inputs[i];
        auto const& actual = parsed.inputs[i];
        auto at = " at input " + std::to_string(i);
        check(expected.frame == actual.frame, "frame differs" + at);
        check(expected.down == actual.down, "hold differs" + at);
        check(expected.player2 == actual.player2, "player differs" + at);
        check(expected.button == actual.button, "button differs" + at);
    }

    // exported data must be routed back to the same format by detection
    auto& registry = FormatRegistry::get();
    auto filename = "macro" + format->extensions().front();
    auto detected = registry.detect(filename, bytes);
    check(detected != nullptr, "detection returned nothing");
    if (detected) check(detected->id() == format->id(), "detected " + detected->id());
}

/// Cross format conversion: every exportable format must be readable back into
/// a macro that can be exported as GDR2, the universal target.
void testConvertToGdr2(Format* format) {
    auto& registry = FormatRegistry::get();
    auto bytes = format->exportMacro(sampleMacro());
    auto imported = registry.import("macro" + format->extensions().front(), bytes);
    check(imported.isOk(), format->id() + ": import failed: " + imported.error());
    if (!imported.isOk()) return;

    auto converted = registry.exportAs("gdr2", imported.unwrap());
    check(converted.isOk(), format->id() + ": gdr2 export failed: " + converted.error());
    if (!converted.isOk()) return;

    auto reimported = registry.import("macro.gdr2", converted.unwrap());
    check(reimported.isOk(), format->id() + ": gdr2 reimport failed: " + reimported.error());
    if (!reimported.isOk()) return;
    check(
        reimported.unwrap().inputs.size() == sampleMacro().inputs.size(),
        format->id() + ": input count changed through gdr2"
    );
}

void testDetection() {
    std::printf("- detection\n");
    auto& registry = FormatRegistry::get();

    Bytes replaybot = {'R', 'P', 'L', 'Y', 2, 1, 0, 0, 0x70, 0x43};
    auto detected = registry.detect("replay.replay", replaybot);
    check(detected && detected->id() == "replaybot", "RPLY magic not detected");

    // a .json file claimed by several bots must be routed by its contents
    std::string tasbot = R"({"fps": 240.0, "macro": []})";
    Bytes tasbotBytes(tasbot.begin(), tasbot.end());
    detected = registry.detect("macro.json", tasbotBytes);
    check(detected && detected->id() == "tasbot", "TASbot json not detected");

    std::string mhr = R"({"meta": {"fps": 240.0}, "events": []})";
    Bytes mhrBytes(mhr.begin(), mhr.end());
    detected = registry.detect("macro.json", mhrBytes);
    check(detected && detected->id() == "mhrjson", "MHR json not detected");
    detected = registry.detect("renamed.data", mhrBytes);
    check(detected && detected->id() == "mhrjson", "MHR content not detected without extension");

    auto gdr2 = registry.exportAs("gdr2", sampleMacro());
    check(gdr2.isOk(), "could not produce GDR2 fixture");
    if (gdr2.isOk()) {
        detected = registry.detect("renamed.gdr", gdr2.unwrap());
        check(detected && detected->id() == "gdr2", "GDR2 magic did not override .gdr extension");
    }

    check(extensionOf("a/b/macro.mhr.json") == ".mhr.json", "double extension");
    check(extensionOf("MACRO.GDR2") == ".gdr2", "extension case");
}

void testMalformedInput() {
    std::printf("- malformed input\n");
    auto& registry = FormatRegistry::get();

    Bytes truncatedGdr2 = {'G', 'D', 'R'};
    auto result = registry.import("macro.gdr2", truncatedGdr2);
    check(!result.isOk(), "truncated GDR2 was accepted");

    Bytes invalidJson = {'{', 'x', '}'};
    result = registry.import("macro.json", invalidJson);
    check(!result.isOk(), "invalid JSON was accepted");

    Bytes unknown = {0, 1, 2, 3};
    result = registry.import("macro.unknown", unknown);
    check(!result.isOk(), "unknown binary was accepted");
}

}

int main() {
    auto& registry = FormatRegistry::get();
    std::printf("%zu formats registered\n", registry.formats().size());

    testDetection();
    testMalformedInput();
    for (auto* format : registry.exportFormats()) {
        testRoundTrip(format);
        testConvertToGdr2(format);
    }

    if (g_failures > 0) {
        std::printf("\n%d checks failed\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
