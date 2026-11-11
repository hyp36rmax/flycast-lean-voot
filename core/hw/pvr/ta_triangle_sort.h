// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace pvr {
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
struct DepthSortStats {std::uint64_t ns=0,triangles=0,standardCalls=0,radixCalls=0,scratchGrowths=0;};
inline DepthSortStats depthSortStats;
#endif
struct TriangleSortEntry {
    TriangleSortEntry() = default;
    TriangleSortEntry(std::uint32_t polygon, std::uint32_t a, std::uint32_t b, std::uint32_t c)
        : vid{a,b,c}, pid(polygon), z(0) {}
    std::uint32_t vid[3];
    std::uint32_t pid;
    float z;
    bool operator<(const TriangleSortEntry& other) const { return z < other.z; }
};

// Stable byte passes reuse distinct caller-owned scratch and retain equal-depth order.
inline bool stableDepthSort(std::vector<TriangleSortEntry>& list,
        std::vector<TriangleSortEntry>& scratch) {
    static_assert(sizeof(float) == sizeof(std::uint32_t) && std::numeric_limits<float>::is_iec559);
    auto key = [](float z) {
        std::uint32_t bits;
        std::memcpy(&bits, &z, sizeof(bits));
        if ((bits & 0x7fffffff) == 0) return std::uint32_t{0x80000000};
        return (bits & 0x80000000) ? ~bits : bits ^ 0x80000000;
    };
    if (list.size() < 128) {
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
        ++depthSortStats.standardCalls;
#endif
        std::stable_sort(list.begin(), list.end());
        return false;
    }
    for (const auto& triangle : list) {
        std::uint32_t bits;
        std::memcpy(&bits, &triangle.z, sizeof(bits));
        const auto exponent = bits & 0x7f800000;
        const auto fraction = bits & 0x007fffff;
        // Preserve the existing comparator path for NaNs and DAZ-sensitive values.
        if ((exponent == 0x7f800000 || exponent == 0) && fraction != 0) {
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
            ++depthSortStats.standardCalls;
#endif
            std::stable_sort(list.begin(), list.end());
            return false;
        }
    }
    const auto previousCapacity = scratch.capacity();
    try {
        if (scratch.size() < list.size()) scratch.resize(list.size());
    } catch (const std::bad_alloc&) {
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
        ++depthSortStats.standardCalls;
#endif
        std::stable_sort(list.begin(), list.end());
        return false;
    }
#ifdef FLYCAST_VOOT_RENDER_DIAGNOSTICS
    ++depthSortStats.radixCalls;
    if (scratch.capacity() != previousCapacity) ++depthSortStats.scratchGrowths;
#else
    (void)previousCapacity;
#endif
    auto* input = list.data();
    auto* output = scratch.data();
    for (unsigned shift = 0; shift < 32; shift += 8) {
        std::array<std::size_t,256> offsets{};
        for (std::size_t i=0;i<list.size();++i) ++offsets[(key(input[i].z) >> shift) & 255];
        if (offsets[(key(input[0].z) >> shift) & 255] == list.size()) continue;
        std::size_t total = 0;
        for (auto& offset : offsets) {const auto count=offset;offset=total;total+=count;}
        for (std::size_t i=0;i<list.size();++i) output[offsets[(key(input[i].z) >> shift) & 255]++] = input[i];
        std::swap(input,output);
    }
    if (input != list.data()) std::copy_n(input,list.size(),list.data());
    return true;
}
}
