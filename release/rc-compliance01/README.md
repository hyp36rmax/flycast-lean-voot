# Corrected Project OT RC runtime source

This delivery matches the two native runtimes inside the public Project OT
v0.5.0-rc.1 ZIP, SHA-256
346DF63DA48A4579C1CF7DB4106DBDD305D337BD38DD0645630EF9F63B55D7CC.
It supplies the corrected RC-UNBLOCK01 builds, not the older pre-CDI-exclusion
source configuration. SOURCE-DELIVERY-MANIFEST.json maps each source archive,
base commit plus modification overlay and released executable hash.

| Variant | Released ProjectOT-Runtime.exe SHA-256 |
| --- | --- |
| Solo / Local VS | 43DEE4CF0CBEC6AA731D33C765C06F8B1C29AB32BB6E75B06225E1A26FE4A022 |
| LAN VS | 6AE1C948213BB0A501A6192CE2082FED7804DCB198886B264E24768A47EDA713 |

## Obtain and build

Download and extract the complete source-delivery ZIP, preserving Dependencies/
and protocol/. Verify SHA256SUMS.txt and Source-File-Inventory.json supplied
beside it. Restore each variant with Restore-Source.py, then follow
BUILD-WINDOWS.md / Build-Windows.ps1 in a new build directory. The existing
wrapper supplies the actual specialized flags: USE_CDI=OFF,
OT_VOOT_RESOURCE_SET=ON, OT_LEAN_RUNTIME=ON, FLYCAST_HOSTED_STARTUP=ON and DX11.
The pinned vendor source archive supplies dependencies without private GitHub
authentication or a private Experience library. Microsoft's compiler/SDK and
VC++ runtime are external prerequisites.

Corresponding-Source-Notices.zip is required co-delivery and contains the
recovered full license/copyright corpus. Preserve the original per-file
notices, GPLv2 and GPLv3 texts, dependency terms and modification records.
Flycast-derived GPL-2.0-or-later code and compiled xBRZ use the compatible GPLv3
route. CDI is not linked in these builds; its separately retained source keeps
its own original grant. This delivery does not relabel every dependency.

## Interfaces and modifications

Both archives include core/hosted_startup.cpp/.h, hosted_input.h and
hosted_aspect.h, their runtime consumers and the CDI/resource build corrections.
protocol/Hosted-Runtime-Protocol.txt documents startup, named events, InputV3,
AspectV1, window/input messages, offsets, state transitions and compatibility.
Runtime build and interface source is supplied, rather than replaced by prose.

The runtime supports the source-inspected standalone path without host:Session;
a compatible independent host can implement the documented interface. The
unchanged official Project OT host pins qualified runtime hashes and rejects
modified replacement binaries. Rebuilding the runtime alone does not bypass
that check. No online activation, Project OT secret key or private C# library
is required to build the native runtime. Do not overwrite a qualified install.

## Provenance and limits

Both corrected Windows x64 Release builds passed in RC-UNBLOCK01 and the retained
output bytes match the public RC. Their base revisions plus isolated source
overlay are explicit in the manifest. A second independent build proving
bit-for-bit reproducibility has not been performed. No new measured machine-code
discrepancy is asserted for these corrected builds; earlier pre-correction
reproduction findings do not identify these binaries. Compiler/linker versions,
generated metadata and embedded build paths must be accounted for in a future
strict reproduction check. No player or runtime rebuild is part of publication.

## Remaining Experience boundary

No private C# Experience implementation or private repository history is included.
Whether the bespoke host/runtime relationship requires additional host interface,
build or installation material remains unresolved. The exact question is whether
the named .NET input/aspect/lifecycle/window/integrity bridges are independent
interoperability, a limited covered integration, or part of a combined covered
work; any applicable GPLv3 installation-information requirement must also be
assessed for the actual transaction and hash-pinned host. Publishing this native
runtime handoff does not clear that question or the full binary distribution.
The protocol documents do not replace any legally required source.

Game ROMs, NAOMI BIOS archives, player saves, private diagnostics and unrelated
Experience engineering material are not part of this source delivery.


## Downloads

Source release: https://github.com/hyp36rmax/flycast-lean-voot/releases/tag/source-v0.5.0-rc.1-corrected

Use the explicitly attached Corrected-Source.zip and its checksum/inventory.
GitHub automatic repository source archives at this delivery-index commit are
not the two corrected variant snapshots. The attached archive is authoritative.
