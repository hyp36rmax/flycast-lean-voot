# Hardware and validation platform

## Recorded Windows build

| Component | FLV-GIT01 configuration |
| --- | --- |
| Target | Windows x64 Release |
| CMake | 4.4.3 |
| Generator | Visual Studio 18 2026 |
| Compiler | MSVC 19.51; toolset directory 14.51.36231 |
| SDK | Windows SDK 10.0.26100.0 |
| Graphics | DX11 |
| Runtime | Lean and hosted startup; explicit Solo/Local or LAN selection |

The [retained recipe](../engineering/FLV-GIT01-Consolidation.md#windows-builds) records the full flags. The top-level CMake file defines the options. [Build options](../../CMakeLists.txt#L79)

## Hardware evidence gaps

Exact original CPU, GPU, driver and Windows OS build linkage is pending for individual historical performance captures. The SDK version identifies the compilation SDK.

Physical controller models, display arrangements and cross-PC network hardware need an identified qualification record before a compatibility matrix can be approved. Future records should identify component/software versions, source and binary identities, and the exact test scope. Device serials, account details, local addresses and private overlays require removal from review assets.
