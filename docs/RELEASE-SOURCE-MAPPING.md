# Source for the frozen Project OT runtimes

The release uses the retained executables. The source-delivery review maps them to the preserved individual source variants, rather than replacing them with a later consolidated build.

| Runtime | Source | Frozen executable SHA-256 |
| --- | --- | --- |
| Solo / Local VS | `d275fdabcad18ba3d236dcbcdb40885ceefaa0a7` | `B542A02195031A98C7F6B0108D09CA5F07B09D716C234A265E9D704650F50B5D` |
| LAN VS | `1a00ec4723a1438857cf4df1797b8844c983fcb8` | `B75509A793E31E0EB58AC9870C08E1635281A1A16EE0A78FE2C7FF983D8D6811` |

The exact Git-exported source and verified dependency archive are staged with an offline restore utility and portable Windows build instructions. Known source inputs match the retained reconstruction chain; observed tracked-file differences are line endings and backup documentation. `core/version.h` has its existing template and CMake generation rule. No missing handwritten runtime C/C++ input was identified.

Git export applies the repository's line-ending attributes: 81 Solo/Local and 82 LAN exported files differ from their raw Git blob bytes only by line endings. The archive hashes identify the supplied exports; a Git blob ID is not presented as a ZIP checksum.

The later consolidated source checkpoint, `e8d678072a10`, selects both implementations through `FLYCAST_OT_LAN_TRANSPORT`. The earlier preserved variant trees do not use that selector. Their source-delivery build recipe uses a separate tree for each variant. Existing consolidated-source validation is not substituted for the original binary's provenance.

Source completeness is assessed using source/build inputs, not successful gameplay or matching executable hashes. Byte-identical rebuilds are not required by this engineering gate. Historical original-library/LAN-graph attribution limits remain documented; no machine-code equivalence is claimed.

## Source availability

The intended release arrangement supplies actual source archives and build/restore instructions alongside the matching binaries. The private staging contains those files now and restores without GitHub credentials or network access. Public download access is not represented as already published. Final recipient access, source scope and notices must be approved before release.

The original Flycast notices remain. The final runtime notice set must account for xBRZ GPLv3, other component terms and the integration assessment; a GPLv2 text by itself is not a completed licensing review.

## Practical questions

**Does compiling this source produce ProjectOT.exe?** No. It produces the native Flycast runtime. The separate Experience manages the player-facing application.

**Do I need private Experience code to compile the runtime?** The supplied C++ source, dependency sources and standard Windows toolchain are the recorded runtime build inputs. The custom runtime-side interface definitions are included.

**Will my build have the frozen SHA-256?** Not necessarily. Generated version data, paths and build settings can change bytes. Keep recipient-built outputs separate and record their own identity.

**Is the original game or NAOMI BIOS included?** No commercial game or NAOMI BIOS archive is supplied. Generated font-table assets and their licensing are tracked separately.

**Is the whole Experience proven independent for licensing?** No. The technical separation and current custom interfaces are documented, but the final scope determination remains open.

**Are new performance results claimed here?** No. The historical studies keep their original workloads, negative results and limitations. The retained methodology is an evidence import, not a new benchmark or a reconstruction of missing approved V1.0 wording.
