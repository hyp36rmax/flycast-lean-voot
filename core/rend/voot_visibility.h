// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>

namespace voot_visibility
{
// Guest screen-space culling must cover the expanded world viewport.
inline std::atomic<float> horizontalExtra{0.f};
static_assert(sizeof(horizontalExtra) == sizeof(float));
static_assert(std::atomic<float>::is_always_lock_free);
}
