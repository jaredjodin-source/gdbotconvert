#include <umacro/Bytes.hpp>
#include <umacro/Format.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {

void usage(std::ostream& out) {
    out << "Usage:\n"
        << "  umacro formats\n"
        << "  umacro inspect <input>\n"
        << "  umacro convert <input> <output> --to <format-id>\n";
}

int listFormats() {
    for (auto* format : umacro::FormatRegistry::get().formats()) {
        std::cout << format->id() << '\t' << format->name() << '\t';
        auto extensions = format->extensions();
        for (size_t i = 0; i < extensions.size(); i++) {
            if (i != 0) std::cout << ',';
            std::cout << extensions[i];
        }
        std::cout << '\t' << (format->canExport() ? "import/export" : "import") << '\n';
    }
    return 0;
}

int inspect(std::string const& inputPath) {
    auto data = umacro::readFile(inputPath);
    auto& registry = umacro::FormatRegistry::get();
    auto* format = registry.detect(inputPath, data);
    if (!format) {
        std::cerr << "Unrecognized macro format\n";
        return 2;
    }

    auto result = registry.import(inputPath, data);
    if (!result.isOk()) {
        std::cerr << result.error() << '\n';
        return 2;
    }

    auto const& macro = result.unwrap();
    std::cout << "format: " << format->id() << " (" << format->name() << ")\n"
        << "bot: " << macro.botName << '\n'
        << "author: " << macro.author << '\n'
        << "level: " << macro.levelName << " (" << macro.levelId << ")\n"
        << "framerate: " << macro.framerate << '\n'
        << "inputs: " << macro.inputs.size() << '\n'
        << "last-frame: " << macro.lastFrame() << '\n';
    return 0;
}

int convert(std::string const& inputPath, std::string const& outputPath, std::string const& formatId) {
    auto& registry = umacro::FormatRegistry::get();
    auto input = umacro::readFile(inputPath);
    auto imported = registry.import(inputPath, input);
    if (!imported.isOk()) {
        std::cerr << imported.error() << '\n';
        return 2;
    }

    auto exported = registry.exportAs(formatId, imported.unwrap());
    if (!exported.isOk()) {
        std::cerr << exported.error() << '\n';
        return 2;
    }

    umacro::writeFile(outputPath, exported.unwrap());
    std::cout << "Converted " << imported.unwrap().inputs.size() << " inputs to " << formatId << '\n';
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "formats") return listFormats();
        if (argc == 3 && std::string_view(argv[1]) == "inspect") return inspect(argv[2]);
        if (argc == 6 && std::string_view(argv[1]) == "convert" && std::string_view(argv[4]) == "--to") {
            return convert(argv[2], argv[3], argv[5]);
        }

        usage(std::cerr);
        return 1;
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
