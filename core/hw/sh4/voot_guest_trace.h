// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#ifdef FLYCAST_VOOT_GUEST_TRACE
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "cfg/option.h"
#include "ui/mainui.h"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>

namespace voot_guest_trace {

inline const char* prefix() { static const char* value = std::getenv("FLYCAST_VOOT_GUEST_TRACE"); return value; }
inline bool enabled() { return prefix() && *prefix(); }
inline void code(std::ofstream& out, u32 center)
{
    if ((center & 0x1c000000) != 0x0c000000 || (center & RAM_MASK) < 128) return;
    const u32 start = (center - 128) & ~1u;
    auto ptr = GetMemPtr(start, 512);
    if (!ptr) return;
    for (u32 i = 0; i < 512; i += 2) {
        u16 op; std::memcpy(&op, ptr + i, 2);
        char text[512]; OpDesc[op]->Disassemble(text, start + i, op);
        out << std::hex << std::setw(8) << std::setfill('0') << start + i << ' ' << std::setw(4) << op << ' ' << text << '\n';
    }
}
inline void visibility(Sh4Context* ctx, u32 pc)
{
    static unsigned samples = 0;
    if (!enabled() || samples >= 512 || MainFrameCount % 12 != 0 || !config::VootWidescreenClipping) return;
    std::ifstream arm(std::string(prefix()) + ".visibility.arm");
    if (!arm) return;
    std::ofstream out(std::string(prefix()) + ".visibility." + std::to_string(samples++) + ".txt");
    out.imbue(std::locale::classic());
    out << std::hex << "frame=" << std::dec << MainFrameCount << " pc=" << std::hex << pc << " pr=" << ctx->pr << " r0=" << ctx->r[0] << " r4=" << ctx->r[4] << " r14=" << ctx->r[14] << '\n';
    for (unsigned i=0;i<16;i++) out << "fr" << std::dec << i << '=' << ctx->fr[i] << " xf=" << ctx->xf[i] << '\n';
    const u32 pointer = ctx->r[14];
    if ((pointer & 0x1c000000) == 0x0c000000) {
        auto p = GetMemPtr(pointer, 64);
        if (p) for (u32 i=0;i<64;i+=4) {u32 bits; float value; std::memcpy(&bits,p+i,4); std::memcpy(&value,p+i,4); out << std::hex << pointer+i << ' ' << bits << ' ' << std::dec << value << '\n';}
    }
    if (samples == 1) {out << "VISIBILITY CODE\n"; code(out, 0x0c024740);}
}
}
#endif
