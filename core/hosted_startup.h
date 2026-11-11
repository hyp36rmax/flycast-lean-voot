// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>

namespace hosted_startup {
#ifdef FLYCAST_HOSTED_STARTUP
bool initialize();
bool enabled();
bool muted();
bool accepts_input();
bool guest_paused();
bool window_input_enabled();
unsigned diagnostics_overlay(std::string& text);
void window_created(void *window);
void window_destroying(void *window);
void frame_submitted();
void input_sampled(unsigned kcode,unsigned jvsInputs=0);
unsigned cabinet_input(unsigned originalKcode);
void poll(void *window);
void aspect_rendered(int width, int height);
void aspect_presented(int outputWidth, int outputHeight);
#else
inline bool initialize() { return true; }
inline bool enabled() { return false; }
inline bool muted() { return false; }
inline bool accepts_input() { return true; }
inline bool guest_paused() { return false; }
inline bool window_input_enabled() { return false; }
inline unsigned diagnostics_overlay(std::string&) { return 0; }
inline void window_created(void *) {}
inline void window_destroying(void *) {}
inline void frame_submitted() {}
inline void input_sampled(unsigned,unsigned=0) {}
inline unsigned cabinet_input(unsigned originalKcode) { return originalKcode; }
inline void poll(void *) {}
inline void aspect_rendered(int, int) {}
inline void aspect_presented(int, int) {}
#endif
}
