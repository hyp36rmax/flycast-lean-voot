// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include "hosted_aspect.h"

TEST(HostedAspect, VersionedFixedSizePacket) { EXPECT_EQ(sizeof(hosted_aspect::Packet), 64u); }
TEST(HostedAspect, AcceptsFittedQualifiedAspects) {
    for (auto size : {std::pair{1920,1440}, {2560,1440}, {3413,1440}, {5120,1440}}) {
        hosted_aspect::Packet p{}; p.version = 1; p.sequence = 1; p.aspect = size.first == 3413 ? 2 : size.first == 5120 ? 3 : 0;
        p.width = size.first; p.height = size.second; EXPECT_TRUE(hosted_aspect::valid(p));
    }
}
TEST(HostedAspect, RejectsUnknownVersion) { hosted_aspect::Packet p{};p.version=2;p.sequence=1;p.width=1920;p.height=1080;EXPECT_FALSE(hosted_aspect::valid(p)); }
TEST(HostedAspect, RejectsUnknownAspect) { hosted_aspect::Packet p{};p.version=1;p.sequence=1;p.width=1920;p.height=1080;p.aspect=4;EXPECT_FALSE(hosted_aspect::valid(p)); }
TEST(HostedAspect, RejectsInvalidDimensions) {
    hosted_aspect::Packet p{};p.version=1;p.sequence=1;p.width=0;p.height=1080;EXPECT_FALSE(hosted_aspect::valid(p));
    p.width=20000;EXPECT_FALSE(hosted_aspect::valid(p));p.width=1920;p.height=0;EXPECT_FALSE(hosted_aspect::valid(p));
}
TEST(HostedAspect, RejectsUnfittedUltrawideCamera) {
    hosted_aspect::Packet p{};p.version=1;p.sequence=1;p.width=2560;p.height=1440;p.aspect=3;EXPECT_FALSE(hosted_aspect::valid(p));
    p.aspect=2;EXPECT_FALSE(hosted_aspect::valid(p));
}
TEST(HostedAspect, RequiresPositiveSequence) { hosted_aspect::Packet p{};p.version=1;p.width=1920;p.height=1080;EXPECT_FALSE(hosted_aspect::valid(p)); }
