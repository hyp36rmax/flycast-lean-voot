# Development history V1.0

Source-history review draft. The approved V1.0 narrative is [pending import](docs/DOCUMENTATION-STATUS.md).

| Milestone | Commit | Source progression |
| --- | --- | --- |
| Official Flycast v2.7 | `5aa091fde632` | Original source/history on `upstream` |
| Lean selection | `ac67f7646e34` | Optional artwork/scraping exclusion |
| Hor+ and visibility | `24121fbbb6e8` | Guarded viewport/visibility integration |
| M08 integration | `7a2944477af1` | Lean and hosted lineage |
| PLAY04-R3 preservation | `f2a8b707613e` | Accepted runtime snapshot recovery |
| GOLD01 preservation | `943ce300d4b1` | Subsequent runtime/source recovery |
| Solo/Local source | `d275fdabcad18` | Current preserved variant |
| LAN source | `1a00ec4723a1` | Cabinet-link additions over Solo/Local |
| Consolidated selection | `e8d678072a10` | Explicit selection of both implementations |

Rendering/performance studies also used isolated snapshots. Their manifests distinguish the measured studies from the later Git checkpoints. The [performance record](docs/performance/PERFORMANCE.md) identifies which implementations remain present.

## Historical recovery

The `backup/project-ot-golden` branch retains the selected runtime source, divergent history and offline dependencies. Its [manifest](https://github.com/hyp36rmax/flycast-lean-voot/blob/backup/project-ot-golden/recovery/manifest.json) records original identities and separately retained in-progress runtime work.

The five earlier preservation branches remain available. Their retirement requires maintainer approval. Existing technical records describe their own historical checkpoints.

## Approved narrative import

The full frozen Development History V1.0 draft remains unavailable. The verified source index and recovery text above are retained. This checklist reserves the missing narrative for import from that original draft.

| Narrative section | Existing verified material | Import status |
| --- | --- | --- |
| Why the project started and why Flycast was chosen | [Approved README introduction](README.md#about-the-project) and upstream source identity | Pending approved historical narrative |
| Establishing the Lean runtime | Lean source checkpoint in the index | Pending approved historical narrative |
| PowerVR projection and clipping investigation | [Rendering record](docs/rendering/RENDERING.md#hor-projection-and-clipping) | Pending approved historical narrative |
| VOOT visibility and recovered submitted geometry | [Visibility record](docs/rendering/RENDERING.md#voot-horizontal-visibility) | Pending approved historical narrative |
| SH-4 execution and rendering performance | [Retained studies](docs/performance/PERFORMANCE.md) | Pending approved historical narrative |
| NAOMI cabinet communication | [Communication record](docs/ENGINEERING.md#cabinet-communication) | Pending approved historical narrative |
| Solo, Local VS and LAN preservation | Variant checkpoints and recovery manifest above | Pending approved historical narrative |
| Unsuccessful experiments and lessons | [Recorded rejected work](docs/performance/PERFORMANCE.md#limits-and-rejected-work) | Pending approved historical narrative |
| Consolidation, source recovery and reproducibility | Consolidation checkpoint and recovery manifest above; frozen-binary equivalence remains unresolved | Pending approved historical narrative |
