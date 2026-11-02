// SPDX-License-Identifier: GPL-2.0-or-later
// Optional Windows host presentation gates. No guest memory or clock changes.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "types.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include "hosted_startup.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "ui/gui.h"
#include "emulator.h"
#include "sdl/sdl.h"

namespace hosted_startup {
namespace {
struct Signals {
    HANDLE created{}, frame{}, activate{}, activated{}, stop{}, fastOn{}, fastOff{}, pause{}, resume{};
    ~Signals() { for (HANDLE h : {created, frame, activate, activated, stop, fastOn, fastOff, pause, resume}) if (h) CloseHandle(h); }
} signals;
std::atomic<bool> requested{false};
std::mutex initialization;
bool initialized = false, initializationOk = false;
std::atomic<bool> active{false}, submitted{false};
bool allowFastForward = false;
std::string windowProperty;
const auto epoch = std::chrono::steady_clock::now();
void note(const char *message) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - epoch).count();
    NOTICE_LOG(COMMON, "Hosted startup: %s; monotonic_ms=%lld", message, (long long)ms);
}
bool signaled(HANDLE h) { return h && WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
}
bool initialize() {
    std::lock_guard<std::mutex> lock(initialization);
    if (initialized) return initializationOk;
    initialized = true;
    const std::string token = config::loadStr("host", "Session", "");
    if (token.empty()) { initializationOk = true; return true; }
    requested = true;
    if (token.size() != 36 || token.find_first_not_of("0123456789abcdefABCDEF-") != std::string::npos) return false;
    const std::string prefix = "Local\\FlycastHost-" + token + "-";
    windowProperty = prefix + "Created";
    auto open = [&](const char *name) { return OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, (prefix + name).c_str()); };
    signals.created = open("Created"); signals.frame = open("GuestFrameSubmitted");
    signals.activate = open("Activate"); signals.activated = open("Activated"); signals.stop = open("Stop");
    signals.fastOn = open("FastOn"); signals.fastOff = open("FastOff");
    signals.pause = open("Pause"); signals.resume = open("Resume");
    if (!signals.created || !signals.frame || !signals.activate || !signals.activated || !signals.stop || !signals.fastOn || !signals.fastOff || !signals.pause || !signals.resume) {
        ERROR_LOG(COMMON, "Hosted startup: required host signals unavailable; refusing window creation"); return false;
    }
    allowFastForward = config::loadBool("host", "AllowSoloFastForward", false) && !config::NetworkEnable;
    initializationOk = true; note("initialized hidden and audio-gated"); return true;
}
bool enabled() { return requested; }
bool muted() { return requested && !active.load(std::memory_order_acquire); }
bool accepts_input() { return !requested || active.load(std::memory_order_acquire); }
void window_created(void *ownedWindow) {
    if (!requested) return;
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(static_cast<SDL_Window *>(ownedWindow), &info)
        || !SetPropA(info.info.win.window, windowProperty.c_str(), reinterpret_cast<HANDLE>(1))) {
        ERROR_LOG(COMMON, "Hosted startup: cannot identify owned presentation window"); return;
    }
    SetEvent(signals.created); note("window created hidden; exact presentation HWND tagged");
}
void window_destroying(void *ownedWindow) {
    if (!requested || !ownedWindow) return;
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window *>(ownedWindow), &info))
        RemovePropA(info.info.win.window, windowProperty.c_str());
}
void frame_submitted() {
    if (requested && !submitted.exchange(true)) { SetEvent(signals.frame); note("guest frame submitted (not scene or scanout readiness)"); }
}
void poll(void *ownedWindow) {
    if (!requested) return;
    if (signaled(signals.stop)) { active = false; settings.input.fastForwardMode = false; dc_exit(); return; }
    if (active && signaled(signals.pause)) { ResetEvent(signals.pause); if (!config::NetworkEnable && !settings.network.online && !settings.naomi.multiboard && !gui_is_open()) gui_togglePause(); note("host pause requested"); }
    if (active && signaled(signals.resume)) { ResetEvent(signals.resume); if (gui_is_open()) gui_togglePause(); note("host resume requested"); }
    if (signaled(signals.fastOff)) { settings.input.fastForwardMode = false; ResetEvent(signals.fastOn); ResetEvent(signals.fastOff); note("fast forward OFF"); }
    if (signaled(signals.fastOn) && !gui_is_open() && !active) {
        ResetEvent(signals.fastOn);
        settings.input.fastForwardMode = allowFastForward && !config::NetworkEnable && !settings.network.online && !settings.naomi.multiboard;
        note(settings.input.fastForwardMode ? "solo diagnostic fast forward ON" : "fast forward refused");
    }
    if (!active && submitted && signaled(signals.activate) && !gui_is_open()) {
        settings.input.fastForwardMode = false;
        SDL_Window *window = static_cast<SDL_Window *>(ownedWindow);
        if (!window) return;
        SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info)) return;
        // The host placed this owned HWND while hidden; showing it must not seize focus.
        ShowWindow(info.info.win.window, SW_SHOWNOACTIVATE);
        active.store(true, std::memory_order_release); SetEvent(signals.activated);
        note("activated; normal speed; startup audio gate released");
    }
}
}
