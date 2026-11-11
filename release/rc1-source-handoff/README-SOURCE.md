# Flycast Lean VOOT source delivery

Private recipient-handoff review for Project OT v0.5.0-rc.1. The source files are present in this folder; restoring them needs no GitHub account, private credentials or network connection. Licensing clearance and public delivery remain separate gates.

## Source variants

| Variant | Pinned source commit | Frozen executable SHA-256 |
| --- | --- | --- |
| Solo / Local VS | `d275fdabcad18ba3d236dcbcdb40885ceefaa0a7` | `B542A02195031A98C7F6B0108D09CA5F07B09D716C234A265E9D704650F50B5D` |
| LAN VS | `1a00ec4723a1438857cf4df1797b8844c983fcb8` | `B75509A793E31E0EB58AC9870C08E1635281A1A16EE0A78FE2C7FF983D8D6811` |

The native runtime executes the original NAOMI software. It includes the retained Project OT rendering, execution, cabinet-link and hosted-interface changes. It does not contain the private Experience menus, controller service or Session Coordinator. Source-level separation describes the architecture; it does not settle the integration's licensing scope.

These commits are the preserved individual implementations, before the later combined build selector. Use separate restored trees. Do not apply `FLYCAST_OT_LAN_TRANSPORT` instructions from the later `e8d678072a10` consolidated tree to these earlier snapshots.

## Files supplied now

- `Solo-Local-d275fdabcad1.zip` — exact tracked Solo/Local source.
- `LAN-1a00ec4723a1.zip` — exact tracked LAN source.
- `Dependencies/Flycast-vendor-source.zip` — verified offline dependency source, including the material for the 22 submodule references.
- `SOURCE-DELIVERY-MANIFEST.json` — archive hashes, commits and frozen executable mapping.
- `Restore-Source.py` — offline restore with archive integrity and path checks.
- `Build-Windows.ps1` — separate source-build recipe; never run as part of player startup.
- `BUILD-WINDOWS.md` — toolchain, build configuration and installation scope.

Raw historical build receipts in this private review folder are maintainer evidence. The final recipient-file list must be reviewed explicitly; do not publish the whole research/evidence workspace.

## Restore without Git

Install Python 3 for this source-restoration utility, then use a new destination:

```powershell
python Restore-Source.py --variant Solo-Local --destination C:\FlycastSource\SoloLocal
python Restore-Source.py --variant LAN --destination C:\FlycastSource\LAN
```

The utility checks the archive digests, rejects overlapping or unsafe paths, and writes a receipt for every restored file. It never overwrites an existing source directory and does not run the game, compiler or a network service.

The restored source includes `core/version.h.in` and the existing CMake generation rule for `core/version.h`. That generated header is not a missing handwritten source file. Build dates, source paths and generated version strings can differ from historical executables. Byte-identical rebuilding is not the source-completeness criterion.

## What the comparison establishes

The pinned source files match the retained reconstruction inputs, apart from line endings and three added backup-documentation files per variant. The dependency comparison found no content differences in available retained files. The few archive files absent from the old extracted trees are non-Windows sample/tool paths, and are present in this delivery. The only retained C/C++ input absent from the archives is the CMake-generated `core/version.h`.

This establishes the available source-input chain. It does not turn prior clean builds into proof of machine-code equivalence, erase the missing original LAN object graph, or prove that every original static-library member was historically built from the same input. Those limits remain in the source mapping report.

## Licensing and recipient access

Preserve Flycast attribution, each component's notices, source and selected license obligations. In particular, xBRZ carries GPLv3 terms, libusb carries LGPL terms, libjuice carries MPL terms, and FreeType offers alternative terms. A GPLv2 text alone is not the complete notice set for this runtime. The review collection and component matrix accompany the private release evidence.

The intended delivery arrangement is actual co-delivery of these source archives and instructions with the corresponding binary release, not a promise to supply them later or a private GitHub link recipients cannot open. The offline restore has been exercised locally. Third-party/public download access has not been published or verified; that requires final release authorization.

No SEGA game or NAOMI BIOS archive is supplied. The upstream `fonts/biosfont.bin.zip` is a generated font-table asset with its generator and font-source comments; its name is not evidence of a dumped NAOMI BIOS. Font licensing and regeneration inputs are inventoried separately and are not silently assumed cleared.
