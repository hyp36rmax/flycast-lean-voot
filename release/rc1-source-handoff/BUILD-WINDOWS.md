# Windows x64 builds for the preserved runtime variants

These instructions describe the retained source-build recipe. No new runtime was built for RELEASE-DAY02, and the qualified executables remain unchanged.

## Toolchain and prerequisites

- Windows x64.
- CMake 4.4.3.
- Visual Studio 18 2026 x64 C++ Build Tools, v145; retained records identify MSVC 19.51.36260 and Windows SDK 10.0.26100.0.
- PowerShell for the optional build wrapper.
- The complete offline source/dependency delivery described in [README-SOURCE.md](README-SOURCE.md).

Use a fresh source tree for each variant. A source archive alone does not materialize Git submodules; `Restore-Source.py` supplies the verified dependency archive without network access.

## Configure and build

```powershell
cmake -S C:\FlycastSource\SoloLocal -B C:\FlycastBuild\SoloLocal -G "Visual Studio 18 2026" -A x64 -DOT_LEAN_RUNTIME=ON -DFLYCAST_HOSTED_STARTUP=ON -DUSE_DX11=ON -DUSE_DX9=OFF -DUSE_OPENGL=OFF -DUSE_VULKAN=OFF -DFLYCAST_VOOT_RENDER_DIAGNOSTICS=OFF -DFLYCAST_VOOT_GUEST_TRACE=OFF -DENABLE_LOG=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=ON -DACTIVATE_MULTITHREADED_COMPILATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build C:\FlycastBuild\SoloLocal --config Release --target flycast --parallel 1 -- /nr:false
```

For LAN, use the separately restored `LAN` source and a new `LAN` output directory with the same flags. Its source already contains the LAN implementation. These older variant commits do not use the later combined-tree `FLYCAST_OT_LAN_TRANSPORT` selector.

The optional wrapper uses the same arguments and checks the restore receipt's variant:

```powershell
.\Build-Windows.ps1 -Variant Solo-Local -SourceDirectory C:\FlycastSource\SoloLocal -BuildDirectory C:\FlycastBuild\SoloLocal
.\Build-Windows.ps1 -Variant LAN -SourceDirectory C:\FlycastSource\LAN -BuildDirectory C:\FlycastBuild\LAN
```

The source's CMake files generate the version header and embedded resources. Keep the tracked Windows resource/icon inputs, hosted interface headers and original copyright notices. The archive/dependency manifests identify the source bytes; the original build receipts retain historical settings for maintainer comparison.

## Output and installation

The Release target produces `Release/flycast.exe`. Project OT's qualified installation uses the unchanged executable bytes under the filename `ProjectOT-Runtime.exe`, separately in `runtime/Solo-Local/` and `runtime/LAN/`.

A recipient-built executable has its own identity and may have different bytes. Keep it in a separate development directory, record its SHA-256 and use an appropriate development/standalone invocation. The supplied wrapper never installs it into Project OT, changes an integrity manifest or replaces a frozen executable. Project OT's production host checks the qualified runtime hashes before activation. The scope of any additional installation/interface information required by the applicable license remains part of the integration review.

The supported compiler/runtime toolchain and Microsoft's x64 C++ runtime are external prerequisites. Do not copy app-local redistributable DLLs into the qualified Project OT package. The private Experience uses the separately installed .NET 10 Runtime; compiling this C++ runtime does not require the private Experience source or produce `ProjectOT.exe`.

## Validation boundary

Earlier clean builds from the reconstruction chain passed. This milestone verifies source restoration, mapping, scripts and documentary settings without rerunning those builds or gameplay. Script syntax validation is not a new compiler/build PASS. Neither matching source nor a successful build alone qualifies physical gameplay or establishes a legal integration boundary.
