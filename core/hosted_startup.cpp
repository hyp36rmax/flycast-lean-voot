// SPDX-License-Identifier: GPL-2.0-or-later
// Optional Windows host presentation gates. No guest memory or clock changes.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
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
#include "hosted_aspect.h"
#include "hosted_input.h"
#include "input/gamepad.h"
#include "network/naomi_network.h"
#include <cstring>
#include "wsi/context.h"
#include "oslib/oslib.h"
#include "hw/sh4/sh4_sched.h"

// Implemented at the existing SDL keyboard/JVS adapter, on its window thread.
bool sdl_hosted_contacts(unsigned mask);

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
void note(const char *message);
HWND inputOwner{}, inputWindow{};
DWORD inputOwnerPid{};
// Process-local Solo control, executed only by the existing render/message thread.
std::atomic<int> guestPauseState{0}; // 0 unavailable, 1 running, 2 paused, 3 pending, 4 rejected
std::atomic<int> pauseRequest{0};
std::atomic<unsigned long long> pauseDeadline{0}, guestCycles{0};
bool hostPaused = false;
WNDPROC originalInputProcedure{};
std::atomic<unsigned> overlayFlags{0};
std::mutex overlayMutex;
std::string overlayText;
constexpr ULONG_PTR overlayTag = 0x4f544f10;
constexpr UINT contactsMessage = WM_APP + 0x51, keyMessage = WM_APP + 0x52,
    focusMessage = WM_APP + 0x53;
bool validInputOwner() {
    DWORD ownerPid = 0;
    GetWindowThreadProcessId(inputOwner, &ownerPid);
    return inputOwner && IsWindow(inputOwner) && ownerPid == inputOwnerPid && ownerPid != GetCurrentProcessId();
}
bool applyContacts(unsigned mask) {
    if (!sdl_hosted_contacts(mask)) return false;
    if (inputWindow) SetPropA(inputWindow,"ProjectOT.InputMask.v1",reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(mask)+1));
    return true;
}
struct CabinetInput {
    HANDLE mapping{}, mutex{};
    hosted_input::Packet *packet{};
    unsigned char generation[16]{};
    int role{}; long long sequence{}, heartbeat{}; unsigned mask{}; int enabled{}; unsigned long long lastRead{};
    bool requested{};
    ~CabinetInput(){if(packet)UnmapViewOfFile(packet);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);}
} cabinetInput;
// The opt-in normalized source is independent of SDL key edges and GUI capture.
// Publication and sample provenance share this short process-local lock; the emu
// thread never waits for the cross-process mailbox mutex.
struct InputPublication {
    std::mutex mutex;
    unsigned mask{}; bool enabled{}; long long sequence{}, heartbeat{}, publishedAt{};
    long long matchedSequence{}, samples{}, sampledAt{}, observedSequence{}; unsigned sampledKcode{}, observedMask{}, jvsInputs{};
} inputPublication;
struct InputProvenance { long long sequence{}; unsigned mask{}; bool valid{}; };
thread_local InputProvenance inputProvenance;
void publishInput(unsigned mask,bool enabled,long long sequence,long long heartbeat){
    std::lock_guard<std::mutex> lock(inputPublication.mutex);
    inputPublication.mask=mask;inputPublication.enabled=enabled;inputPublication.sequence=sequence;
    inputPublication.heartbeat=heartbeat;inputPublication.publishedAt=GetTickCount64();
}
void exportInputObservation(hosted_input::Packet& p){
    std::lock_guard<std::mutex> lock(inputPublication.mutex);
    p.sampledSequence=inputPublication.matchedSequence;p.samples=inputPublication.samples;p.sampledKcode=inputPublication.sampledKcode;
    p.publishedAt=inputPublication.publishedAt;p.sampledAt=inputPublication.sampledAt;
    p.observedSequence=inputPublication.observedSequence;p.observedMask=inputPublication.observedMask;p.jvsInputs=inputPublication.jvsInputs;
    p.link=settings.network.online ? 1|(naomiNetwork.getSlotId()<<8)|(naomiNetwork.getSlotCount()<<16):0;
}
bool inputLock(){
    if(!cabinetInput.mutex)return false;
    auto result=WaitForSingleObject(cabinetInput.mutex,0);
    if(result==WAIT_ABANDONED){publishInput(0,false,cabinetInput.sequence,0);ReleaseMutex(cabinetInput.mutex);note("abandoned cabinet input writer; neutralized");return false;}
    return result==WAIT_OBJECT_0;
}
void clearCabinetInput(){publishInput(0,false,cabinetInput.sequence,0);if(cabinetInput.packet){cabinetInput.packet->applied=0;cabinetInput.packet->appliedEnabled=0;}}
void pollCabinetInput(){
    auto& c=cabinetInput;if(!c.requested)return;
    if(!inputLock()){if(GetTickCount64()-c.lastRead>250)publishInput(0,false,c.sequence,0);return;}
    c.lastRead=GetTickCount64();
    auto& p=*c.packet;const auto now=GetTickCount64();int error=0;
    if(!validInputOwner() || p.hostPid!=inputOwnerPid)error=1;
    else if(p.role!=c.role || memcmp(p.generation,c.generation,16)!=0)error=2;
    else if(p.sequence<c.sequence || p.sequence<0 || (p.sequence==c.sequence && (p.mask!=c.mask || p.enabled!=c.enabled)))error=3;
    else if((p.mask & ~0x1fffu)!=0 || (p.enabled!=0 && p.enabled!=1))error=4;
    else if(p.heartbeat<c.heartbeat || p.heartbeat<0 || static_cast<unsigned long long>(p.heartbeat)>now || now-p.heartbeat>250)error=5;
    p.version=3;p.runtimePid=GetCurrentProcessId();
    if(error){if(p.error!=error)note("cabinet input rejected/expired; neutralized");p.error=error;clearCabinetInput();}
    else {
        p.error=0;c.heartbeat=p.heartbeat;
        if(p.sequence>c.sequence){
            c.sequence=p.sequence;c.mask=p.mask;c.enabled=p.enabled;
            publishInput(p.mask,p.enabled && active && !hostPaused,p.sequence,p.heartbeat);
            p.applied=p.enabled && active && !hostPaused?p.mask:0;p.appliedEnabled=p.enabled && active && !hostPaused;
            p.acknowledged=p.sequence;
        }
        if(!p.enabled || !active || hostPaused)clearCabinetInput();
        else {std::lock_guard<std::mutex> lock(inputPublication.mutex);inputPublication.heartbeat=p.heartbeat;}
    }
    exportInputObservation(p);
    ReleaseMutex(c.mutex);
}
bool aspectControlPending();
LRESULT CALLBACK inputProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_COPYDATA && lparam) {
        const auto& packet = *reinterpret_cast<const COPYDATASTRUCT *>(lparam);
        if ((packet.dwData & ~ULONG_PTR(3)) == overlayTag) {
            if (reinterpret_cast<HWND>(wparam) != inputOwner || !validInputOwner() || !active
                || !packet.lpData || packet.cbData < 1 || packet.cbData > 512)
                return 0;
            const auto *text = static_cast<const char *>(packet.lpData);
            if (text[packet.cbData - 1] != 0 || std::memchr(text, 0, packet.cbData - 1))
                return 0;
            std::lock_guard<std::mutex> lock(overlayMutex);
            overlayText.assign(text, packet.cbData - 1);
            overlayFlags.store(packet.dwData & 3, std::memory_order_release);
            return 1;
        }
    }
    if (message == WM_APP + 0x55 || message == WM_APP + 0x56 || message == WM_APP + 0x57) {
        if (reinterpret_cast<HWND>(wparam) != inputOwner || !validInputOwner()) return 0;
        if (message == WM_APP + 0x56) return static_cast<DWORD>(guestCycles.load());
        if (message == WM_APP + 0x57) return static_cast<DWORD>(guestCycles.load() >> 32);
        if (lparam == 0) return guestPauseState.load();
        if ((lparam != 1 && lparam != 2) || !active || config::NetworkEnable || settings.network.online || settings.naomi.multiboard || aspectControlPending()) return 0;
        if (pauseRequest.load() != 0) return 0;
        pauseDeadline = GetTickCount64() + 2000;
        guestPauseState = 3; pauseRequest = static_cast<int>(lparam);
        return 3;
    }
    if(message == WM_APP + 0x54) {
        if(reinterpret_cast<HWND>(wparam)!=inputOwner || !validInputOwner() || GetForegroundWindow()!=window)return 0;
        DWORD ownerPid=0;GetWindowThreadProcessId(inputOwner,&ownerPid);
        applyContacts(0);
        return AllowSetForegroundWindow(ownerPid)?1:0;
    }
    if (message == contactsMessage) {
        if(cabinetInput.requested)return 0; // exclusive negotiated channel; never two input writers
        if (reinterpret_cast<HWND>(wparam) != inputOwner || !validInputOwner()
            || (static_cast<UINT_PTR>(lparam) & ~0x7fffu)) return 0;
        const unsigned mask = static_cast<unsigned>(lparam);
        if (mask && (hostPaused || !active.load(std::memory_order_acquire) || GetForegroundWindow() != window)) {
            applyContacts(0); return 2; // suspended, not delivered
        }
        return applyContacts(mask) ? 1 : 0;
    }
    if (message == WM_KILLFOCUS) {
        if(!cabinetInput.requested)applyContacts(0);
        if (validInputOwner()) PostMessage(inputOwner, focusMessage, 0, reinterpret_cast<LPARAM>(window));
    }
    if (message == WM_SETFOCUS && validInputOwner())
        PostMessage(inputOwner, focusMessage, 1, reinterpret_cast<LPARAM>(window));
    if (message == WM_KEYDOWN || message == WM_KEYUP) {
        // System chords keep ordinary Windows ownership; never capture another window.
        const bool chord = (GetKeyState(VK_MENU) & 0x8000) || (GetKeyState(VK_CONTROL) & 0x8000)
            || (GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000);
        if (wparam == VK_F11 && message == WM_KEYDOWN && !(lparam & (1LL << 30))
            && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)
            && validInputOwner() && GetForegroundWindow() == window)
            PostMessage(inputOwner, keyMessage, 1024, reinterpret_cast<LPARAM>(window));
        if (chord) {
            applyContacts(0);
            if (validInputOwner()) PostMessage(inputOwner, focusMessage, 0, reinterpret_cast<LPARAM>(window));
        }
        else if (validInputOwner() && GetForegroundWindow() == window && active.load(std::memory_order_acquire))
            if(message == WM_KEYUP || !(lparam & (1LL << 30)))
                PostMessage(inputOwner, keyMessage, (wparam & 255) | (message == WM_KEYDOWN ? 256 : 0), reinterpret_cast<LPARAM>(window));
        return 0; // host is the sole physical keyboard translator
    }
    return CallWindowProc(originalInputProcedure, window, message, wparam, lparam);
}
struct AspectControl {
    HANDLE mapping{};
    hosted_aspect::Packet *packet{};
    SDL_Window *window{};
    bool pending{}, rendered{}, restoring{};
    std::atomic<bool> presentationCheckpoint{false};
    int deferredSequence{};
    bool wide{}, super{}, clipping{};
    int x{}, y{}, width{}, height{};
    ~AspectControl() { if (packet) UnmapViewOfFile(packet); if (mapping) CloseHandle(mapping); }
} aspectControl;
bool aspectControlPending() { return aspectControl.pending; }
void aspectState(int state) {
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&aspectControl.packet->state), state);
}
void restoreAspect() {
    auto& a = aspectControl;
    if (!a.pending) return;
    a.restoring = true; a.rendered = false;
    aspectState(hosted_aspect::Restoring);
    config::Widescreen = a.wide; config::SuperWidescreen = a.super; config::VootWidescreenClipping = a.clipping;
    SDL_SetWindowPosition(a.window, a.x, a.y); SDL_SetWindowSize(a.window, a.width, a.height);
    sdl_update_display_metrics(a.window, SDL_GetWindowFlags(a.window));
    GraphicsContext::Instance()->resize();
}
void pollAspect(SDL_Window *window) {
    auto& a = aspectControl;
    if (!a.packet) return;
    auto& p = *a.packet;
    const bool checkpoint=a.presentationCheckpoint.exchange(false, std::memory_order_acq_rel);
    const auto state = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&p.state), 0, 0);
    if (!a.pending && state == hosted_aspect::Cancel) { aspectState(hosted_aspect::RolledBack); return; }
    if (a.pending && !a.restoring && (state == hosted_aspect::Cancel || GetTickCount64() >= p.deadline || !active || gui_is_open())) {
        restoreAspect(); note("aspect rollback awaiting native presentation"); return;
    }
    if (state != hosted_aspect::Requested) return;
    if (!checkpoint) {
        if (a.deferredSequence != p.sequence) { a.deferredSequence = p.sequence; note("aspect deferred until native renderer presentation checkpoint"); }
        return;
    }
    // A leased transaction starts only after the renderer can present, not when the host queued it.
    p.deadline = GetTickCount64() + 4000;
    if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&p.state), hosted_aspect::Applying, hosted_aspect::Requested) != hosted_aspect::Requested) return;
    p.error = 0;
    if (!window || !active || gui_is_open() || a.pending || !hosted_aspect::valid(p)
            || p.deadline <= GetTickCount64() || p.deadline > GetTickCount64() + 10000
            || config::NetworkEnable || settings.network.online || settings.naomi.multiboard
            || config::RendererType != RenderType::DirectX11 || config::Rotate90 || config::EmulateFramebuffer
            || config::ScreenStretching != 100 || settings.content.gameId != "VIRTUAL-ON ORATORIO TANGRAM") {
        p.error = 1; aspectState(hosted_aspect::Rejected); os_notify("Aspect change unavailable", 1500); return;
    }
    a.window = window; a.wide = config::Widescreen; a.super = config::SuperWidescreen; a.clipping = config::VootWidescreenClipping;
    SDL_GetWindowPosition(window, &a.x, &a.y); SDL_GetWindowSize(window, &a.width, &a.height);
    a.pending = true; a.rendered = a.restoring = false;
    try {
        config::Widescreen = p.aspect != 0; config::SuperWidescreen = p.aspect >= 2; config::VootWidescreenClipping = p.aspect != 0;
        SDL_SetWindowPosition(window, p.x, p.y); SDL_SetWindowSize(window, p.width, p.height);
        sdl_update_display_metrics(window, SDL_GetWindowFlags(window));
        GraphicsContext::Instance()->resize();
        if (settings.display.width != p.width || settings.display.height != p.height) {
            p.error = 2; restoreAspect();
        }
    } catch (...) { p.error = 3; restoreAspect(); }
}
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
    const int inputVersion=config::loadInt("host","InputChannel",0);
    if(inputVersion!=0 && inputVersion!=3)return false;
    cabinetInput.requested=inputVersion==3;
    if(cabinetInput.requested){
        auto& c=cabinetInput;c.role=config::loadInt("host","InputRole",-1);
        const auto generation=config::loadStr("host","InputGeneration","");
        if(c.role<0 || c.role>2 || generation.size()!=32 || generation.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)return false;
        for(int i=0;i<16;i++)c.generation[i]=static_cast<unsigned char>(strtoul(generation.substr(i*2,2).c_str(),nullptr,16));
        c.mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,(prefix+"InputV3").c_str());
        c.mutex=OpenMutexA(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(prefix+"InputLockV3").c_str());
        if(c.mapping)c.packet=static_cast<hosted_input::Packet*>(MapViewOfFile(c.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(hosted_input::Packet)));
        if(!c.packet || !c.mutex)return false;
        note("cabinet input V3 mailbox opened; legacy contacts disabled");
    }
    // Optional control map: old hosts remain compatible; old runtimes leave
    // version zero so a new host cannot advertise live switching accidentally.
    aspectControl.mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, (prefix + "AspectV1").c_str());
    if (aspectControl.mapping) {
        aspectControl.packet = static_cast<hosted_aspect::Packet*>(MapViewOfFile(aspectControl.mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(hosted_aspect::Packet)));
        if (aspectControl.packet) {
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&aspectControl.packet->reserved), 2);
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&aspectControl.packet->version), 1);
        }
    }
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
bool window_input_enabled() { return inputWindow != nullptr; }
void window_created(void *ownedWindow) {
    if (!requested) return;
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(static_cast<SDL_Window *>(ownedWindow), &info)
        || !SetPropA(info.info.win.window, windowProperty.c_str(), reinterpret_cast<HANDLE>(1))) {
        ERROR_LOG(COMMON, "Hosted startup: cannot identify owned presentation window"); return;
    }
    // Optional native-host keyboard contract. No global hook or background keyboard registration.
    const char *ownerText = getenv("FLYCAST_HOST_INPUT_HWND");
    const char *pidText = getenv("FLYCAST_HOST_INPUT_PID");
    if (ownerText && pidText) {
        inputOwner = reinterpret_cast<HWND>(static_cast<UINT_PTR>(_strtoui64(ownerText, nullptr, 10)));
        inputOwnerPid = strtoul(pidText, nullptr, 10);
        DWORD ownerPid = 0; GetWindowThreadProcessId(inputOwner, &ownerPid);
        if (validInputOwner() && ownerPid == strtoul(pidText, nullptr, 10)) {
            inputWindow = info.info.win.window;
            originalInputProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtr(inputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(inputProcedure)));
            if (!originalInputProcedure || !SetPropA(inputWindow, "ProjectOT.WindowInput.v1", reinterpret_cast<HANDLE>(1))) {
                if (originalInputProcedure) SetWindowLongPtr(inputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalInputProcedure));
                inputWindow = nullptr; inputOwner = nullptr;
            } else {
                SetPropA(inputWindow, "ProjectOT.DiagnosticsOverlay.v1", reinterpret_cast<HANDLE>(1));
                note("owned-window input protocol 1 ready");
            }
        }
    }
    SetEvent(signals.created); note("window created hidden; exact presentation HWND tagged");
}
void window_destroying(void *ownedWindow) {
    if (!requested || !ownedWindow) return;
    if (inputWindow) {
        RemovePropA(inputWindow, "ProjectOT.DiagnosticsOverlay.v1");
        {
            std::lock_guard<std::mutex> lock(overlayMutex);
            overlayFlags.store(0, std::memory_order_release);
            overlayText.clear();
        }
        applyContacts(0); RemovePropA(inputWindow, "ProjectOT.WindowInput.v1");RemovePropA(inputWindow,"ProjectOT.InputMask.v1");
        SetWindowLongPtr(inputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalInputProcedure));
        inputWindow = nullptr; inputOwner = nullptr;inputOwnerPid=0; originalInputProcedure = nullptr;
    }
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window *>(ownedWindow), &info))
        RemovePropA(info.info.win.window, windowProperty.c_str());
}

unsigned diagnostics_overlay(std::string& text) {
    if (overlayFlags.load(std::memory_order_acquire) == 0 || !active)
        return 0;
    std::lock_guard<std::mutex> lock(overlayMutex);
    text = overlayText;
    return overlayFlags.load(std::memory_order_relaxed);
}
// Called at the original JVS command read, immediately before its existing mapping.
unsigned cabinet_input(unsigned originalKcode){
    inputProvenance={};if(!cabinetInput.requested)return originalKcode;
    pollCabinetInput(); // consume at the original guest input boundary, independent of Present
    static const unsigned bits[]={DC_DPAD_UP,DC_DPAD_DOWN,DC_DPAD_LEFT,DC_DPAD_RIGHT,DC_BTN_Y,DC_BTN_Z,DC_DPAD2_LEFT,DC_DPAD2_RIGHT,DC_BTN_A,DC_BTN_C,DC_BTN_B,DC_BTN_X,DC_BTN_START};
    unsigned mask=0;long long sequence=0;
    {std::lock_guard<std::mutex> lock(inputPublication.mutex);
        sequence=inputPublication.sequence;
        if(inputPublication.enabled && active && GetTickCount64()>=static_cast<unsigned long long>(inputPublication.heartbeat)
            && GetTickCount64()-inputPublication.heartbeat<=250)mask=inputPublication.mask;}
    unsigned controlled=0,pressed=0;for(unsigned i=0;i<13;i++){controlled|=bits[i];if(mask&(1u<<i))pressed|=bits[i];}
    inputProvenance={sequence,mask,true};
    return (originalKcode|controlled)&~pressed;
}
// The provenance belongs to this emu thread's exact Maple snapshot, not the
// mailbox's newest request. Original JVS conversion/serialization stays intact.
void input_sampled(unsigned kcode,unsigned jvsInputs){
    if(!cabinetInput.requested || !inputProvenance.valid)return;
    static const unsigned bits[]={DC_DPAD_UP,DC_DPAD_DOWN,DC_DPAD_LEFT,DC_DPAD_RIGHT,DC_BTN_Y,DC_BTN_Z,DC_DPAD2_LEFT,DC_DPAD2_RIGHT,DC_BTN_A,DC_BTN_C,DC_BTN_B,DC_BTN_X,DC_BTN_START};
    unsigned mask=0;for(unsigned i=0;i<13;i++)if(!(kcode&bits[i]))mask|=1u<<i;
    {std::lock_guard<std::mutex> lock(inputPublication.mutex);
        ++inputPublication.samples;inputPublication.sampledKcode=kcode;inputPublication.sampledAt=GetTickCount64();
        inputPublication.observedSequence=inputProvenance.sequence;inputPublication.observedMask=mask;inputPublication.jvsInputs=jvsInputs;
        if(mask==inputProvenance.mask)inputPublication.matchedSequence=inputProvenance.sequence;}
    if(inputLock()){exportInputObservation(*cabinetInput.packet);ReleaseMutex(cabinetInput.mutex);}
}
bool guest_paused() { return hostPaused; }
void frame_submitted() {
    if (requested && !submitted.exchange(true)) { SetEvent(signals.frame); note("guest frame submitted (not scene or scanout readiness)"); }
}
void poll(void *ownedWindow) {
    if (!requested) return;
    if (signaled(signals.stop)) { if(cabinetInput.requested)clearCabinetInput();applyContacts(0); restoreAspect(); active = false; settings.input.fastForwardMode = false; dc_exit(); return; }
    const int request = pauseRequest.exchange(0);
    if (request != 0) {
        try {
            if (GetTickCount64() >= pauseDeadline || !active || config::NetworkEnable || settings.network.online || settings.naomi.multiboard || aspectControl.pending)
                guestPauseState = 4;
            else {
                applyContacts(0);
                if (request == 1 && !hostPaused) { emu.stop(); hostPaused = true; }
                if (request == 2 && hostPaused) { emu.start(); hostPaused = false; }
                guestCycles = sh4_sched_now64();
                guestPauseState = hostPaused && !emu.running() ? 2 : !hostPaused && emu.running() ? 1 : 4;
                note(hostPaused ? "guest pause acknowledged after emulator stop" : "guest resume acknowledged after emulator start");
            }
        } catch (...) { guestPauseState = 4; note("guest pause/resume failed"); }
    }
    guestCycles = sh4_sched_now64();
    if (!hostPaused) pollAspect(static_cast<SDL_Window*>(ownedWindow));
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
        active.store(true, std::memory_order_release); guestPauseState = 1;
        SetPropA(info.info.win.window,"ProjectOT.GuestPause.v1",reinterpret_cast<HANDLE>(1)); SetEvent(signals.activated);
        note("activated; normal speed; startup audio gate released");
    }
}
void aspect_rendered(int width, int height) {
    auto& a = aspectControl;
    if (!a.pending) return;
    a.packet->internalWidth = width; a.packet->internalHeight = height;
    a.packet->outputWidth = settings.display.width; a.packet->outputHeight = settings.display.height;
    a.rendered = true;
}
void aspect_presented(int outputWidth, int outputHeight) {
    auto& a = aspectControl;
    if (outputWidth > 0 && outputHeight > 0) a.presentationCheckpoint.store(true, std::memory_order_release);
    if (!a.pending || !a.rendered) return;
    if (outputWidth != (a.restoring ? a.width : a.packet->width)
            || outputHeight != (a.restoring ? a.height : a.packet->height)) return;
    if (a.restoring) {
        a.pending = a.rendered = a.restoring = false;
        aspectState(hosted_aspect::RolledBack);
        os_notify("Aspect change unavailable; previous aspect restored", 1500);
        return;
    }
    const auto state = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&a.packet->state), 0, 0);
    if (state == hosted_aspect::Applying) aspectState(hosted_aspect::Presented);
    else if (state == hosted_aspect::Commit && GetTickCount64() < a.packet->deadline) {
        const int selectedAspect = a.packet->aspect;
        if (InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(&a.packet->state), hosted_aspect::Committed, hosted_aspect::Commit) != hosted_aspect::Commit) return;
        a.pending = a.rendered = false;
        const char *labels[] = { "Aspect 4:3", "Aspect 16:9", "Aspect 21:9", "Aspect 32:9" };
        os_notify(labels[selectedAspect], 1500);
        note("aspect committed after DX11 render and presentation");
    }
}
}
