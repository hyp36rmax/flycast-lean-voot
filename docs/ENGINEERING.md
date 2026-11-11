# Flycast Lean VOOT engineering

Our starting point was a working Flycast runtime for the original NAOMI game. Keeping that foundation let us investigate display, execution and cabinet-link problems without replacing the emulator around them.

Flycast Lean VOOT remains a C++ source project that builds a native executable, `flycast.exe`. It runs the guest software, renders the game and produces audio. Host integration interfaces let a separate application supply configuration, route input and manage session processes.

Source reference `e8d678072a10e767f70549e967350203f007d10c` identifies the consolidated runtime described here.

## Implementation map

| Area | Source |
| --- | --- |
| Lean build | [Build option](../CMakeLists.txt#L79) and [UI source selection](../core/ui/CMakeLists.txt#L28) |
| PowerVR, DX11 and Hor+ | [Rendering notes](rendering/RENDERING.md) |
| SH-4 execution | [MMU-aware x64 dispatch](../core/rec-x64/rec_x64.cpp#L743) |
| NAOMI communication | [Windows readiness wait](../core/network/naomi_network.cpp#L34) and [Board wait loop](../core/hw/naomi/naomi_m3comm.cpp#L310) |
| Hosted execution | [Startup and signal handling](../core/hosted_startup.cpp#L272) |
| Runtime identity | [Windows resources](../shell/windows/flycast.rc#L22) |

## Runtime profiles

Solo uses one runtime process. Local VS uses separate MASTER and SLAVE cabinets, each with its own configuration and state. LAN adds a different receive-worker transport, so preserving the two implementations required more than changing endpoint addresses.

During consolidation, we kept both implementations behind an explicit build selector. That gives developers a single source tree while retaining the original Solo/Local path and the separate LAN additions. [Selector and configuration gate](../CMakeLists.txt#L299), [Board reset](../core/hw/naomi/naomi_m3comm.cpp#L98)

| Mode | `FLYCAST_OT_LAN_TRANSPORT` | Runtime configuration |
| --- | --- | --- |
| Solo | `OFF` | Game profile and owned process lifecycle |
| Local VS | `OFF` | Separate MASTER/SLAVE profiles using the original cabinet transport |
| LAN VS | `ON` | Hosted startup and the existing reservation/endpoint contract |

The validated Windows x64 Release configuration enables DX11, Lean mode and hosted startup. The [retained recipe](engineering/FLV-GIT01-Consolidation.md#windows-builds) gives its complete toolchain and CMake flags. LAN selection requires hosted startup.

Lean mode excludes artwork and scraping sources. Other frontend code remains compiled. Complete frontend exclusion is still an open architectural qualification. [Compiled UI source list](../core/ui/CMakeLists.txt#L2)

## External integration

An externally managed executable needs clear ownership of its settings and writable state. The host supplies absolute directories through `FLYCAST_HOST_CONFIG_DIR` and `FLYCAST_HOST_DATA_DIR`; the runtime then uses those directories for configuration and data. [Directory boundary](../core/windows/winmain.cpp#L82)

Hosted startup exposes named events for creation, first guest-frame submission, activation and stop. These let the host distinguish a process that has started from a runtime that has submitted a frame and is ready for activation. [Lifecycle events](../core/hosted_startup.cpp#L307)

Input and aspect control use versioned packet layouts. Their size checks preserve the interface expected by a compatible host while the runtime continues to own emulation, rendering and audio execution. [Input layout](../core/hosted_input.h#L15), [Aspect layout](../core/hosted_aspect.h#L17)

LAN initialization reads `PROJECTOT_LAN08_SESSION`, `PROJECTOT_LAN08_LOCAL` and `PROJECTOT_LAN08_PEER`. Before creating the transport, it validates the title, role port and endpoint configuration. [LAN initialization](../core/network/naomi_network.cpp#L73)

The receive worker handles bounded fragment assembly and receive queues, peer loss and stopping. Its transport bounds and board-reset path remain explicit in the selected LAN implementation. [Transport bounds](../core/network/ot_lan_transport.h#L30), [Receive worker](../core/network/ot_lan_transport.h#L376)

## Cabinet communication

Waiting for cabinet data was a host CPU cost worth investigating. The original Local VS path now uses Windows readiness waiting inside the board loop's existing deadline. Opt-in loopback-only configuration limits the original transport to same-PC destinations. The [historical network scope note](OT-Local-Network-Scope.md) records that configuration. [Readiness method](../core/network/naomi_network.cpp#L34)

The later LAN transport has its own worker and lifecycle. Its behavior and measurements need to be assessed in that context; the older Local VS timing study does not qualify LAN performance. [Performance evidence](performance/PERFORMANCE.md#naomi-receive-waiting)

## Validation scope

FLV-GIT01 built both Release profiles from a clean checkout with verified offline dependencies. It recorded 25 passing focused rendering/aspect tests, four host fixtures, four transport scenarios and four actual runtime startup/shutdown scenarios. One inherited microbenchmark remained disabled. Eight compiler comparisons covered both variants across four affected translation units, normalizing source paths, diagnostic line numbers and formatting.

Those results apply to the source checkpoint above. Physical gameplay, controller/display checks and sustained cross-PC LAN sessions remain pending for this revision. Historical performance studies retain their own [workload limits](performance/PERFORMANCE.md).

## Documentation map

- [Rendering and visibility](rendering/RENDERING.md)
- [Performance evidence](performance/PERFORMANCE.md)
- [Hardware and toolchain scope](hardware/VALIDATION-PLATFORM.md)
- [Attribution and licensing](LICENSING.md)

Maintainer planning and import tracking are kept in the [documentation review record](DOCUMENTATION-STATUS.md) and [visual asset checklist](assets/ASSET-CHECKLIST.md).
