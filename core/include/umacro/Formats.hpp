#pragma once

#include <umacro/Format.hpp>

namespace umacro {

void registerGdrFormats(FormatRegistry& registry);
void registerMegaHackFormats(FormatRegistry& registry);
void registerTasbotFormats(FormatRegistry& registry);
void registerEchoFormats(FormatRegistry& registry);
void registerTextFormats(FormatRegistry& registry);
void registerSimpleBinaryFormats(FormatRegistry& registry);
void registerReplayEngineFormats(FormatRegistry& registry);
void registerSilicateFormats(FormatRegistry& registry);

}
