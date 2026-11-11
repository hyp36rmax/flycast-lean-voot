# Visual asset review checklist

This is an asset plan. Final graphics and gameplay images await separate maintainer review.

| ID | Proposed asset | Recommended path | Available evidence | Review needed |
| --- | --- | --- | --- | --- |
| VIS-01 | Original 4:3 and expanded captures | `docs/assets/rendering/voot-4x3.png` and `voot-32x9.png` | R&D-03D captures/comparisons within each aspect | Match scene, camera and frame; verify captions, identity and rights |
| VIS-02 | Hor+ projection/clipping diagram | `docs/assets/rendering/horplus-clipping.svg` | Matrix guards and focused fixtures | Verify dimensions, vertical limits, RTT exclusions and labels |
| VIS-03 | Visibility comparison | `docs/assets/rendering/visibility-comparison.png` | R&D-02D submission/geometry evidence and guarded source | Select matched gameplay images; identify source revision and correction scope |
| VIS-04 | Runtime architecture | `docs/assets/architecture/runtime-interfaces.svg` | Public lifecycle, input/aspect and transport interfaces | Show the runtime's external contract and process ownership |
| VIS-05 | Palette/parser historical timing | `docs/assets/performance/rd03d-parser.svg` | R&D-03D extract | Retain units, workload, sample count, tail values and historical scope |
| VIS-06 | SH-4 cycles with cadence context | `docs/assets/performance/rd03e-dispatch.svg` | R&D-03E extract | Show cycle reduction and increased render-start P99 in separate panels |
| VIS-07 | NAOMI receive-wait active cycles | `docs/assets/performance/rd04a-link-wait.svg` | R&D-04A SLAVE and MASTER values | Label roles/workloads and active cycles; distinguish later LAN transport |
| VIS-08 | Hardware compatibility material | `docs/assets/hardware/` | Known toolchain; incomplete historical hardware linkage | Verify models, drivers and OS; review image rights and private identifiers |

The image candidates remain in the maintainer's evidence archive. R&D-03D comparisons identify before/after frames within each aspect. Selecting a cross-aspect presentation pair still requires scene review.

The [rendering notes](../rendering/RENDERING.md) give source anchors for projection/visibility. The [measurement extract](../performance/measurements.json) supplies chart values and evidence identities. The [hardware note](../hardware/VALIDATION-PLATFORM.md) states the current gap.

## Approval checklist

- [ ] Verify source revision, capture identity and the exact illustrated claim.
- [ ] Confirm screenshot and hardware-image rights.
- [ ] Select matched scenes and restrained captions without private identifiers.
- [ ] Keep chart units, workload, sample counts and counterexamples visible.
- [ ] Check diagrams against runtime interfaces and source guards.
- [ ] Approve final files and alt text before adding them to the repository.
