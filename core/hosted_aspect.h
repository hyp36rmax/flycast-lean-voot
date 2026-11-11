// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cmath>

// V1 is a Solo-only, leased preview/commit transaction. The SDL/render thread
// owns all configuration and window mutations; the host only writes requests.
namespace hosted_aspect {
enum State { Idle, Requested, Applying, Presented, Commit, Committed, Cancel, RolledBack, Rejected, Restoring };
struct alignas(8) Packet {
    int32_t version, state, sequence, aspect;
    int32_t x, y, width, height;
    uint64_t deadline;
    int32_t error, outputWidth, outputHeight, internalWidth, internalHeight;
    int32_t reserved;
};
static_assert(sizeof(Packet) == 64, "Host ABI changed");
inline bool valid(const Packet& p) {
    if (p.version != 1 || p.aspect < 0 || p.aspect > 3 || p.width < 320 || p.height < 240
            || p.width > 16384 || p.height > 8640 || p.sequence <= 0) return false;
    // 4:3/16:9 use a full output surface with renderer pillarboxing. Ultrawide
    // cameras derive their aspect from the fitted native client dimensions.
    const double ratio = double(p.width) / p.height;
    return p.aspect < 2 || std::abs(ratio - (p.aspect == 2 ? 64. / 27 : 32. / 9)) < .005;
}
}
