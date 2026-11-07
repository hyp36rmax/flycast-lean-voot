# GOLD01 runtime source and recovery

Source checkpoint: `943ce300d4b12154774b4843549f0e2c92ba5cac`. It descends from existing Flycast history and the earlier Project OT source backup. The corresponding-source archive identity is `06D49D2DAA5CA8A8E2BAE5FEEFAB051A38DE63724F57AC9131589457AC66B519`.

Accepted changes preserved here: opt-in loopback-only NAOMI sockets with no broadcast/UPnP/external destinations; retained LAN socket MTU and payload-free diagnostics; deferred renderer-presentation admission for hosted F6 aspect transactions; accurate runtime identity resources and approved icon. Existing Lean, clipping, projection, visibility and hosted input modifications remain in ancestry/source. No unrelated runtime R&D branch is merged; no new networking algorithm is introduced by preservation.

The six retained base/overlay archive layers reconstruct the qualified source. `core/version.h` is generated, not a source revision authority. Existing submodule gitlinks and licenses remain unchanged. The private offline source branch `backup/vendor-sources` at `e6ab144d80eb986b1eb1d49bdbaf46ea590d94fd` contains archive parts and file hashes; all 15,116 dependency source entries match the qualified archive. Concatenate parts as binary data, verify archive SHA, then extract into a fresh clone. Optional upstream prebuilt packages/test keys excluded by that archive are not required by the Win64 target.

Configure in a fresh output directory:

```text
cmake -S <clone> -B <new-build> -G "Visual Studio 18 2026" -A x64 -DOT_LEAN_RUNTIME=ON -DFLYCAST_HOSTED_STARTUP=ON -DUSE_DX11=ON -DUSE_DX9=OFF -DUSE_OPENGL=OFF -DUSE_VULKAN=OFF -DUSE_DISCORD=OFF -DENABLE_CTEST=OFF "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
cmake --build <new-build> --config Release --target flycast --parallel 4 -- /nodeReuse:false
```

Toolchain: CMake 4.4.3, VS 18 2026 x64 BuildTools, Windows SDK 10.0.26100.0. Quote the dotted CMake policy value in PowerShell. Retain GPL license, attribution, vendor notices and applicable corresponding source with separately approved distribution. This is a private engineering backup, not a release. No source from the separate private host implementation is included here.

Binary-for-binary reproducibility is not claimed. Preserved source commits and verified corresponding-source identities establish source recovery; accepted binary identities remain separate. No generated executable, BIOS, commercial game archive, save or credential is included in this commit.
