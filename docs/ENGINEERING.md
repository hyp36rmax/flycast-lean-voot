# Flycast Lean VOOT engineering

Flycast Lean VOOT is a Flycast-derived runtime focused on Virtual-On: Oratorio Tangram M.S.B.S. Ver. 5.66. It executes the game and exposes interfaces for an external application to configure and manage runtime processes.

This technical review draft describes source checkpoint `e8d678072a10e767f70549e967350203f007d10c`. The approved V1.0 editorial set is [pending import](DOCUMENTATION-STATUS.md).

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

The validated Windows x64 Release configuration enables DX11, Lean mode and hosted startup. The [retained recipe](engineering/FLV-GIT01-Consolidation.md#windows-builds) gives its complete toolchain and CMake flags.

| Mode | `FLYCAST_OT_LAN_TRANSPORT` | Runtime configuration |
| --- | --- | --- |
| Solo | `OFF` | Game profile and owned process lifecycle |
| Local VS | `OFF` | Separate MASTER/SLAVE profiles using the original cabinet transport |
| LAN VS | `ON` | Hosted startup and the existing reservation/endpoint contract |

The LAN selector guards the preserved variant additions and requires hosted startup. Its transport logic and board-reset call remain in the selected LAN path. [Selector and configuration gate](../CMakeLists.txt#L299) · [Board reset](../core/hw/naomi/naomi_m3comm.cpp#L98)

Lean mode excludes artwork and scraping sources. Other frontend code remains compiled. Complete frontend exclusion is an open architectural qualification. [Compiled UI source list](../core/ui/CMakeLists.txt#L2)

## External integration

The host supplies absolute configuration and writable-data directories through `FLYCAST_HOST_CONFIG_DIR` and `FLYCAST_HOST_DATA_DIR`. The runtime owns emulation, rendering and audio execution. [Directory boundary](../core/windows/winmain.cpp#L82)

The hosted lifecycle uses named events for creation, first guest-frame submission, activation and stop. Input and aspect packets have versioned layouts. These contracts support independently managed runtime processes. [Lifecycle events](../core/hosted_startup.cpp#L307) · [Input layout](../core/hosted_input.h#L15) · [Aspect layout](../core/hosted_aspect.h#L17)

LAN initialization reads `PROJECTOT_LAN08_SESSION`, `PROJECTOT_LAN08_LOCAL` and `PROJECTOT_LAN08_PEER`, then validates the title, role port and endpoint configuration. Its receive worker has bounded assembly/receive queues and explicit peer-loss and stop handling. [LAN initialization](../core/network/naomi_network.cpp#L73) · [Transport bounds](../core/network/ot_lan_transport.h#L30) · [Receive worker](../core/network/ot_lan_transport.h#L376)

The original Local VS transport retains its Windows readiness wait within the board loop's existing deadline. Opt-in loopback-only configuration limits it to same-PC destinations. The [historical network scope note](OT-Local-Network-Scope.md) describes that configuration. [Readiness method](../core/network/naomi_network.cpp#L34)

## Validation scope

FLV-GIT01 built both Release profiles from a clean checkout with verified offline dependencies. It recorded 25 passing focused rendering/aspect tests, four host fixtures, four transport scenarios and four actual runtime startup/shutdown scenarios. One inherited microbenchmark remained disabled. Eight compiler comparisons covered both variants across four affected translation units, normalizing source paths, diagnostic line numbers and formatting.

These results apply to the source checkpoint above. Physical gameplay, controller/display checks and sustained cross-PC LAN sessions remain pending for this revision. Historical performance studies have their own [workload limits](performance/PERFORMANCE.md).

## Documentation map

- [Rendering and visibility](rendering/RENDERING.md)
- [Performance evidence](performance/PERFORMANCE.md)
- [Hardware and toolchain scope](hardware/VALIDATION-PLATFORM.md)
- [Attribution and licensing](LICENSING.md)
- [Visual asset checklist](assets/ASSET-CHECKLIST.md)
