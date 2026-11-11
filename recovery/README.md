# Runtime source recovery

The runtime source tree in this archival branch is the preserved November 10
LAN baseline. Its Solo/Local parent and the PLAY04-R3 and GOLD01 source
checkpoints remain available by their original commit identities.

The archive commit deliberately retains multiple historical parents.
It combines recovery reachability without merging their implementations into
the source tree. recovery/manifest.json lists every retained branch and tag
identity and the minimal archive-parent set. Restore any recorded commit with
git checkout --detach <sha> in a separate clone. Every recorded commit is an
ancestor of this archive commit.

source-dependencies/ retains the existing verified vendor-source archive parts,
notices and restoration manifest. Concatenate the parts in manifest order as
binary data, verify the archive checksum, then extract into the selected
runtime checkout. The archive contains 15,116 source entries. Upstream unused
binary packages and dependency test keys remain excluded.

recovery/Uncommitted-Runtime-20261110.zip separately preserves the 17 modified
or untracked files in the original active runtime worktree, its patches and
file checksums. Those in-progress sources have not been merged into main.

The public V1.0 editorial document set is PENDING IMPORT. Existing README,
license and engineering notes remain in history. This recovery archive has
no authority to promote a new physical Golden runtime or release candidate.
