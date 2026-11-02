// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
#include <chrono>
#include <cstdint>
#include <cstdlib>
namespace pvr {
inline bool parserProfileEnabled = std::getenv("FLYCAST_DX11_PERF_OUTPUT") != nullptr;
struct ParserProfile {
    std::uint64_t resetNs=0,decodeNs=0,postNs=0,regionNs=0;
    std::uint64_t paletteConvertNs=0,directConvertNs=0,convertedPixels=0,updateNs=0,setupNs=0,convertNs=0,protectNs=0,uploadNs=0,textureNs=0,textureCalls=0,textureSamples=0,textureUpdates=0,commandBytes=0;
};
inline ParserProfile parserProfile;
inline unsigned textureSamplePhase=0;
inline std::uint64_t parserTicks() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
#endif
