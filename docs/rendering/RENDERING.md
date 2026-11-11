# Rendering, projection and visibility

These notes describe `e8d678072a10e767f70549e967350203f007d10c`. Historical physical checks and current automated tests have separate scopes.

## DX11 and PowerVR

Depth-stencil and blend caches return references to retained COM state objects. This avoids a temporary owner on each cached selection. DXGI maximum frame latency is requested as one, with an existing warning path if the request fails. [Depth-state cache](../../core/rend/dx11/dx11_renderstate.h#L95) · [Blend-state cache](../../core/rend/dx11/dx11_renderstate.h#L187) · [Queue request](../../core/rend/dx11/dx11context.cpp#L133)

Stable depth sorting uses reusable scratch storage and radix passes for eligible lists. Small lists, sensitive floating-point inputs and allocation failure retain the comparison-sort path. Palette conversion retains the selected lookup base per conversion while reading current palette contents per texel. [Sort helper](../../core/hw/pvr/ta_triangle_sort.h#L27) · [Palette converter](../../core/rend/texconv.cpp#L279)

The [retained host fixtures](../../tests/voot-host/README.txt) cover sorting, palette conversion, dispatch and socket readiness. Historical timings are in the [performance notes](../performance/PERFORMANCE.md).

## Hor+ projection and clipping

Matrix calculation uses the current render-target dimensions. Super-widescreen derives width expansion from that target, while the established widescreen path uses its 16:9 expansion. Rotation, framebuffer emulation and render-to-texture have separate handling. [Matrix calculation](../../core/rend/transform_matrix.cpp#L88)

The tile-clip correction widens an eligible full-width horizontal clip to the base scissor. Its guards retain partial-width, inverse and render-to-texture cases and the original vertical limits. [Tile-clip guards](../../core/rend/transform_matrix.cpp#L216)

Original-aspect behavior is covered by the matrix fixture family. The differential corpus compares the clipping implementation with its retained reference calculation. [Regression corpus](../../tests/src/VootWidescreenClipTest.cpp#L353)

## VOOT horizontal visibility

Projection expansion alone cannot restore a model rejected by the game's original horizontal visibility test. The runtime publishes an extra horizontal extent from the eligible VOOT world viewport, with zero expansion when the source guards do not admit the correction. [Extent calculation](../../core/rend/transform_matrix.cpp#L107) · [Shared extent](../../core/rend/voot_visibility.h#L8)

The x64 compiler recognizes two identified bound loads, validates the title, load width, destination register and six-instruction signature, then adjusts the generated load result by that extent. The specialization is scoped to the matched code. [Guarded specialization](../../core/rec-x64/rec_x64.cpp#L262)

The retained R&D-02D record reports restored submitted geometry and short physical checks at original, 16:9 and ultrawide aspects. Wider stage, character, camera-angle and HUD coverage remains pending. The consolidated source's automated/startup scope is recorded in [Engineering](../ENGINEERING.md#validation-scope).

The [asset checklist](../assets/ASSET-CHECKLIST.md) plans matched captures and projection/visibility figures for separate scene, caption and rights review.
