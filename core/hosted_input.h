// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
namespace hosted_input {
struct Packet {
    int32_t version, enabled, role; uint32_t mask;
    int64_t sequence, acknowledged, heartbeat;
    uint8_t generation[16];
    int32_t error; uint32_t applied;
    int64_t sampledSequence; uint32_t sampledKcode, runtimePid, hostPid, appliedEnabled;
    int64_t samples;
    int64_t publishedAt, sampledAt, observedSequence; int32_t observedMask; uint32_t jvsInputs; int32_t link; int32_t reserved; int64_t padding;
};
static_assert(sizeof(Packet)==144 && offsetof(Packet,generation)==40 && offsetof(Packet,samples)==88,"InputV1 ABI");
}
