# Technical figure evidence and review

The figures document source behavior and retained measurements. VERIFIED means the illustrated claim was checked against that source/evidence; it does not establish complete physical gameplay qualification, approve image rights, or authorize publication. Maintainer review and merge approval remain pending.

## Asset inventory

| ID | Asset | Classification | Scope |
| --- | --- | --- | --- |
| VIS-01 | [Matched 3840×1080 visibility comparison](rendering/voot-visibility-comparison.png) | VERIFIED | New B2 source-controlled comparison; private integration authorized, public rights approval remains open |
| VIS-02 | [Hor+ and clipping](rendering/horplus-clipping.svg) | VERIFIED | Exact equal-height aspect ratios; separate submission and clipping responsibilities |
| VIS-03 | [Submitted polygon records](rendering/visibility-submission.svg) | VERIFIED | Historical numeric replay evidence; the new gameplay comparison is tracked as VIS-01 |
| VIS-04 | [Runtime interfaces](architecture/runtime-interfaces.svg) | VERIFIED | Public C++ runtime contracts; external host internals excluded |
| VIS-05 | [Parser](performance/rd03d-parser.svg) | VERIFIED | Historical mean and P99 wall time; original sample/workload scope |
| VIS-06 | [SH-4 dispatch](performance/rd03e-dispatch.svg) | VERIFIED | Historical mean cycles and render-start P99 regression |
| VIS-07 | [Cabinet receive waiting](performance/rd04a-link-wait.svg) | VERIFIED | Historical active cycles for both roles; separate labeled scales |
| VIS-08 | Hardware photos / compatibility captures | BLOCKED | No authenticated hardware photograph or revision-linked hardware compatibility record identified |

## VIS-01 matched B2 captures

The [rendering document](../rendering/RENDERING.md#vis-01-matched-gameplay-comparison) integrates new BEFORE/AFTER captures from isolated R&D-02D-based builds. One BEFORE-only false guard suppresses the two identified visibility limit-load adjustments, leaving projection and renderer clipping enabled in both. [Sanitized provenance](rendering/vis01-provenance.json) records source, build and image identities without private system paths or raw game data.

All four captured geometry streams per role match historical counterparts; projection matrices agree. Newly measured sample-2 counts reproduce 0 → 576 in the identified far-mesh groups and 1,687 → 2,411 overall. The unmodified full-frame PNGs are 3840×1080; the comparison adds labels without resizing either panel. This is a matched rendered-state/capture-sequence result; no absolute guest simulation-frame ID was exported.

The maintainer has authorized integration into a private draft PR. That authorization does not establish public screenshot rights or permit public distribution. Broader scene/gameplay qualification and VIS-08 remain open.

## VIS-02 projection and clipping

Source checkpoint `e8d678072a10e767f70549e967350203f007d10c` supplies the runtime implementation; the documentation base also contains the FLV-DOC01 integration. [Matrix calculation](../../core/rend/transform_matrix.cpp#L88) computes horizontal expansion; [extent publication](../../core/rend/transform_matrix.cpp#L107) supplies the guarded visibility extra. [Tile clipping](../../core/rend/transform_matrix.cpp#L216) retains vertical, partial-width, inverse and render-to-texture guards. [x64 specialization](../../core/rec-x64/rec_x64.cpp#L262) matches the game, two load addresses, width/register and six opcodes. [Visibility extent](../../core/rend/voot_visibility.h#L8) is shared with generated code.

The rectangles have a common illustrative height of 144 units and widths 192, 256 and 512: precisely 4:3, 16:9 and 32:9. They illustrate framing, not guest pixel-coordinate clip rectangles or gameplay. The source uses actual target aspect in super-widescreen mode and the established 16:9 expansion in ordinary widescreen mode. No vertical field expansion is depicted. The dashed center indicates the original horizontal span; the outer blue sides show expanded framing and the eligible horizontal clip boundary.

The guest visibility correction is a separate requirement: a renderer cannot reconstruct geometry omitted by the guest. [Clip regression corpus](../../tests/src/VootWidescreenClipTest.cpp#L353), [actual viewport/fallback fixture](../../tests/src/VootWidescreenClipTest.cpp#L396) and [rendering notes](../rendering/RENDERING.md) retain the detailed guards. This editorial task did not rerun runtime tests or expand their qualification scope.

## VIS-03 submission evidence

R&D-02D reproduced the defect in a saved Solo scene, x64/DX11 diagnostics at 3840×1080. Before/after raw TA samples 0 and 1 were byte-identical. In sample 2, the three identified far-mesh material groups changed from 0/0/0 to 348/201/27 polygon records, totaling 576. Independently recounting the retained polygon CSVs reproduced each count. The whole sample changed from 1,687 to 2,411 polygons; 724 additional records include other stage geometry.

The [aggregate record](rendering/rd02d-submission.json) retains hashes of the engineering report, source manifest, comparison records, analysis script and both polygon CSVs. Full raw guest geometry, proprietary software, diagnostics and local archive paths are excluded from Git. Evidence identifiers refer to retained maintainer records, not downloadable files in this repository.

The investigation used an isolated RD02D source snapshot, not a new Git commit, against pinned upstream `5aa091fde632fb332c8d8c34e280d62dc951954c`. The source manifest identifies the snapshot. Its final Release executable identity does not independently identify each intermediate diagnostic trace build. The figure makes no final-binary performance claim. Short physical checks reported in the historical record do not replace full stage/camera/HUD or current consolidated-source qualification.

The groups are local material/mesh evidence, not formal guest object IDs. The bar uses a linear zero-based 0–600 polygon scale and represents counts, not geometric size. No gameplay frames were synthesized. The new [VIS-01 B2 comparison](../rendering/RENDERING.md#vis-01-matched-gameplay-comparison) supplies matched gameplay images for private review; public rights approval remains open.

## VIS-04 runtime interfaces

The foundation retains SH-4 execution, PowerVR rendering, AICA audio, JVS input and NAOMI cabinet communication. Lean changes are presented as modifications around that foundation, not a replacement emulator or an ordered processing pipeline.

Foundation references include [PowerVR/AICA initialization](../../core/emulator.cpp#L512), [AICA update scheduling](../../core/hw/aica/aica.cpp#L87), [JVS board state](../../core/hw/maple/maple_jvs.cpp#L146) and [NAOMI communication-board receive](../../core/hw/naomi/naomi_m3comm.cpp#L108). The SH-4 execution and modified dispatch path are linked in the engineering implementation map.

References: [runtime profiles and source map](../ENGINEERING.md#runtime-profiles), [build selector](../../CMakeLists.txt#L299), [lifecycle events](../../core/hosted_startup.cpp#L307), [versioned input](../../core/hosted_input.h#L15), [Solo-only aspect transaction](../../core/hosted_aspect.h#L6), [directory boundary](../../core/windows/winmain.cpp#L82), [LAN initialization](../../core/network/naomi_network.cpp#L73), and [LAN worker bounds](../../core/network/ot_lan_transport.h#L30).

Solo uses one runtime process. Local VS uses MASTER and SLAVE with `FLYCAST_OT_LAN_TRANSPORT=OFF`; the separate LAN implementation selects `ON` and requires hosted startup. The external box shows only public host responsibilities: settings/data configuration, input requests and process ownership. It contains no private Experience implementation.

## VIS-05 through VIS-07 historical performance

All numeric chart values come from the unchanged [measurement extract](../performance/measurements.json), cross-checked against [performance notes](../performance/PERFORMANCE.md). Values are rounded only for displayed precision: parser means to three decimals, parser P99 to one decimal, cycle means to whole cycles and render-start P99 to four milliseconds decimals. SVG bar widths use the unrounded values. Axes start at zero; no broken axes, log scales or 3D area encodings are used.

| Figure | Workload / samples | Counterexample or limitation |
| --- | --- | --- |
| RD03D | Saved Solo, 3840×1080, controls released; A/B/B/A; 120 warmup frames per run; 1,200 measured frames per variant | Parser P99 retained alongside mean; wall-time result is scoped to the scene |
| RD03E | Matched saved Solo, 3840×1080; 120 warmup and 600 measured frames per variant | Render-start P99 increases from 23.9924 to 25.8561 ms despite reduced emulation mean cycles |
| RD04A | Linked original-aspect run; 120 warmup and 600 measured samples per cabinet | MASTER mean rises from 46,605 to 47,724; role workloads differ; separately labeled panel scales |

The figures identify historical studies, not benchmarks of consolidated `main`. Original CPU/GPU/Windows build linkage remains unverified. Active thread cycles exclude descheduled waiting; cycles are not converted to wall time. The original receive-wait study does not benchmark the later LAN worker. No direct gameplay FPS, GPU execution time or input-latency benefit is claimed. Original measurement-record hashes remain in the extract.

## Capture and hardware review

The maintainer archive contains original 1440×1080 (4:3) and 3840×1080 (32:9) RD03D DX11 readbacks from saved Solo sample 2, with same-aspect baseline/candidate pixel equality and source/binary manifests. Those pairs demonstrate palette-study equivalence within each aspect; matching sample numbers alone does not approve a cross-aspect gameplay comparison.

1920×1080 image candidates also exist in later display/effect investigations. Dimensions alone do not establish Hor+ flags, scene suitability or accepted source revision. Some captures belong to explicitly rejected experiments and must not illustrate accepted rendering behavior. Those older candidates do not replace the new source-controlled B2 comparison. VIS-01 now uses the newly verified pair; other gameplay captures still require their own provenance and rights review.

Known build tools are documented in [validation platform](../hardware/VALIDATION-PLATFORM.md). Runtime device names or monitor logs are not a hardware compatibility test. No suitable authenticated hardware photograph was identified in the scoped archive search; bundled upstream artwork was excluded. Exact models, drivers, OS, binary/source linkage, test scope, image rights and privacy review are still required for VIS-08. Only the three verified VIS-01 B2 gameplay PNGs are integrated privately. No hardware photograph is added.

## Maintenance and visual validation

Run `python docs/assets/generate_visuals.py` from any directory using Python 3. It uses only the standard library and the retained aggregate JSON files. The editable SVGs contain native shapes and text, a fixed white canvas with contrasting labels/borders for both light and dark GitHub surroundings, and system font fallbacks. No embedded fonts, scripts, external resources, proprietary imagery or private system identifiers are used.

Every SVG was rasterized and visually inspected. Browser checks cover 720-pixel desktop figures and 358-pixel figure space in a 390-pixel mobile viewport, each against light and dark surroundings. No text extends outside a viewBox; detailed SVGs remain zoomable. Relative Markdown image targets resolve. The SVG/image subset uses standard GitHub-compatible elements; the draft PR remains the maintainer's final GitHub presentation review surface.

The generator, aggregate records and technical figures are part of the repository source documentation and retain the repository's applicable license. Maintainer editorial approval, screenshot/photograph rights and the separate distribution licensing gates remain open. No merge, release or public distribution is authorized by figure verification.
