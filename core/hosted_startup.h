// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace hosted_startup {
#ifdef FLYCAST_HOSTED_STARTUP
bool initialize();
bool enabled();
bool muted();
bool accepts_input();
void window_created(void *window);
void window_destroying(void *window);
void frame_submitted();
void poll(void *window);
#else
inline bool initialize() { return true; }
inline bool enabled() { return false; }
inline bool muted() { return false; }
inline bool accepts_input() { return true; }
inline void window_created(void *) {}
inline void window_destroying(void *) {}
inline void frame_submitted() {}
inline void poll(void *) {}
#endif
}
