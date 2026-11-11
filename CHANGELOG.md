# Changelog

## Unreleased — v0.5.0-dev

The current runtime checkpoint is [e8d678072a10](https://github.com/hyp36rmax/flycast-lean-voot/commit/e8d678072a10e767f70549e967350203f007d10c). This is a private development baseline for documentation review.

- Added explicit selection for the preserved Solo/Local and LAN cabinet implementations.
- Retained Lean selection, DX11/PowerVR work, Hor+ clipping, guarded VOOT visibility correction and MMU-aware x64 dispatch.
- Retained hosted lifecycle, versioned input/aspect interfaces and identity resources.
- Added the retained synthetic LAN transport regression fixture.

The source checkpoint passed the scoped [engineering validation](docs/ENGINEERING.md#validation-scope). Physical gameplay and sustained cross-PC qualification remain pending.

## Preserved checkpoints

| Milestone | Source identity | Scope |
| --- | --- | --- |
| Flycast v2.7 | `5aa091fde632` | Untouched upstream reference |
| PLAY04-R3 | `f2a8b707613e` | Earlier preserved runtime source |
| GOLD01 | `943ce300d4b1` | Golden source recovery |
| Solo/Local development | `d275fdabcad18` | Preserved November 10 variant |
| LAN development | `1a00ec4723a1` | Preserved November 10 variant |

Approved V1.0 changelog wording is [pending import](docs/DOCUMENTATION-STATUS.md). This draft records verified source milestones.
