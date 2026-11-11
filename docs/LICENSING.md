# Attribution and licensing review

Flycast Lean VOOT retains Flycast history and source attribution. The upstream README identifies Flycast's reicast lineage. Runtime headers state the GNU General Public License, version 2 or later. The original GPL text and per-file notices are retained. [Upstream reference](upstream/README.md), [GPL text](../LICENSE), [representative source notice](../core/network/naomi_network.cpp#L1)

Third-party dependencies retain their own source and licenses. The [submodule manifest](../.gitmodules) identifies their repositories. Current runtime profiles are described in [Engineering](ENGINEERING.md#runtime-profiles). Offline source recovery is described in the [retained build recipe](engineering/FLV-GIT01-Consolidation.md#windows-builds).

## Review before distribution

The release review still needs to establish corresponding source for the exact offered binaries, component-specific notices, the static libusb relinking route, recipient-ready build/install material and source access.

The external integration boundary also requires review. These documents describe the runtime side of that boundary. Game, BIOS, screenshots, icons and other assets need a separate rights and final-package inventory review.

This proposal contains source references, historical measurement extracts and an asset plan. Publication readiness remains a separate gate.
