#include <umacro/Formats.hpp>

namespace umacro {

void registerBuiltinFormats(FormatRegistry& registry) {
    registerGdrFormats(registry);
    registerMegaHackFormats(registry);
    registerTasbotFormats(registry);
    registerEchoFormats(registry);
    registerTextFormats(registry);
    registerSimpleBinaryFormats(registry);
    registerReplayEngineFormats(registry);
    registerSilicateFormats(registry);
}

}
