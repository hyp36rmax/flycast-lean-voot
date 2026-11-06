// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdint>
namespace frame_delivery_trace {
inline const char* output = [] {
    const char* value = std::getenv("FLYCAST_DX11_DELIVERY_OUTPUT");
    return value && *value ? value : "delivery.csv";
}();
inline std::atomic<bool> armed{std::getenv("FLYCAST_DX11_DELIVERY_OUTPUT") != nullptr};
inline bool enabled() { return armed.load(std::memory_order_relaxed); }
inline uint64_t ticks() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::atomic<uint64_t> producerWait{0}, renderWait{0}, frameWait{0}, renderQueuedAt{0}, renderAge{0}, presentAge{0};
inline std::atomic<uint64_t> linkWaitNs{0},linkWaitCycles{0},linkPolls{0},linkTimeouts{0};
inline std::atomic<unsigned> emulationThreadId{0};
inline std::atomic<unsigned> maxDepth{0}, droppedPresent{0}, skippedRender{0}, renders{0}, presents{0};
}
#endif
