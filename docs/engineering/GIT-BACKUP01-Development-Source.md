# Flycast Lean VOOT development source checkpoint

This private checkpoint carries the Flycast runtime source used by the
Project OT v0.5.0-dev UX04-R2 package. The active Flycast worktree was at
`7a2944477af130f4a8e03f732163b1c20ed89aa3`. The source is from the
hash-checked UX03-R4 chain with the variant-specific FIX01 and SEC-UI01
deltas. For LAN, the LANNET source overlay precedes those deltas.

The two consecutive backup commits preserve the Solo/Local and LAN trees.
Use the commit named 'Back up Flycast Lean VOOT v0.5.0-dev Solo and Local source'
for Solo and Local VS. Use the following LAN commit for LAN VS. These variants
differ in cabinet-link source, so switching CMake flags alone is insufficient.

Configure each checkout with Visual Studio 18 2026 Build Tools, Windows SDK
10.0.26100.0, and the retained x64 Release options.

```powershell
cmake -S . -B <output> -G "Visual Studio 18 2026" -A x64 -DOT_LEAN_RUNTIME=ON -DFLYCAST_HOSTED_STARTUP=ON -DUSE_DX11=ON -DUSE_DX9=OFF -DUSE_OPENGL=OFF -DUSE_VULKAN=OFF -DFLYCAST_VOOT_RENDER_DIAGNOSTICS=OFF -DFLYCAST_VOOT_GUEST_TRACE=OFF -DENABLE_LOG=OFF -DUSE_DISCORD=OFF -DUSE_BREAKPAD=ON -DACTIVATE_MULTITHREADED_COMPILATION=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build <output> --config Release --target flycast --parallel 1 -- /nr:false
```

The checked-in Git submodule links remain in place. Offline dependency
sources are available separately on the existing private
`backup/vendor-sources` ref at `e6ab144d80eb986b1eb1d49bdbaf46ea590d94fd`.
The generated version header and build artifacts are excluded. This checkpoint
does not certify executable reproduction or distribution readiness.
