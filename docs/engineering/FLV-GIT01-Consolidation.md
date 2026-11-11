# Flycast Lean VOOT development source

Flycast Lean VOOT is a dedicated Flycast-derived NAOMI runtime for
Virtual-On Oratorio Tangram. Its external host supplies session configuration,
input ownership, display placement and lifecycle management. This repository
contains the runtime and its integration interfaces. The external Experience
implementation is maintained separately.

The source foundation is the private November 10 runtime checkpoint
`1a00ec4723a1438857cf4df1797b8844c983fcb8`, including its Solo/Local parent
`d275fdabcad18ba3d236dcbcdb40885ceefaa0a7`. Official Flycast v2.7 is
`5aa091fde632fb332c8d8c34e280d62dc951954c`.

## Windows builds

Use CMake 4.4.3, Visual Studio 18 2026 x64 Build Tools, MSVC 19.51 and
Windows SDK 10.0.26100.0, matching the retained development recipe.

Restore the offline dependency source archive from
`backup/project-ot-golden:source-dependencies/` into a fresh checkout.
Concatenate its archive parts in their manifest order, verify the archive and
entry checksums, and extract using a tool with Windows long-path support.
The existing submodule links are retained. Their materialized source is in
that recovery archive.

```powershell
cmake -S . -B <new-output> -G "Visual Studio 18 2026" -A x64 -DOT_LEAN_RUNTIME=ON -DFLYCAST_HOSTED_STARTUP=ON -DFLYCAST_OT_LAN_TRANSPORT=OFF -DUSE_DX11=ON -DUSE_DX9=OFF -DUSE_OPENGL=OFF -DUSE_VULKAN=OFF -DFLYCAST_VOOT_RENDER_DIAGNOSTICS=OFF -DFLYCAST_VOOT_GUEST_TRACE=OFF -DENABLE_LOG=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=ON -DACTIVATE_MULTITHREADED_COMPILATION=ON "-DCMAKE_POLICY_VERSION_MINIMUM=3.5" -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build <new-output> --config Release --target flycast --parallel 1 -- /nr:false
```

`FLYCAST_OT_LAN_TRANSPORT=OFF` selects the preserved Solo/Local cabinet
implementation. `ON` selects the preserved LAN implementation and requires
Windows desktop hosted startup. LAN still requires the existing
`PROJECTOT_LAN08_SESSION`, `PROJECTOT_LAN08_LOCAL` and
`PROJECTOT_LAN08_PEER` launch contract. These build choices select the
existing implementations. Packet framing, port selection, timeout constants,
worker ownership and communication-board reset logic are retained.

`OT_LEAN_RUNTIME` excludes artwork and scraping. The current source still
contains standalone frontend code. Complete frontend removal remains a
separate architectural qualification.

## Regression fixtures

Build `tests/voot-host` separately with CMake and run CTest in its output
directory. It contains retained synthetic sort, palette, dispatch and socket
checks. Build `tests/lan-transport` separately and run CTest to exercise the
retained synthetic transport scenarios against the actual runtime header.
The main CMake test target supports `ENABLE_CTEST=ON` and the focused
`FLYCAST_VOOT_FOCUSED_TESTS=ON` rendering and hosted-aspect fixtures.

Physical original-aspect, Hor+, Solo, Local VS and cross-PC LAN gameplay
qualification remains pending for this consolidated source revision.

## Documentation import

PENDING IMPORT applies to the approved November 6 V1.0 document set.
The original upstream-derived README, license and existing engineering
documents are preserved. The approved README, ENGINEERING, rendering,
performance, hardware, CHANGELOG and DEVELOPMENT_HISTORY_V1.0 content requires
the verified export. This source and build note records the consolidation
work and leaves those editorial documents pending.

The runtime remains GPL-derived. Preserve existing notices and attribution.
The release compliance work retains open corresponding-source, component
notice, recipient-access and integration-boundary checks.
