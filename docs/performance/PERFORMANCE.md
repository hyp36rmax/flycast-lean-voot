# Performance evidence

These are historical controlled studies. Consolidated `main` has source and regression validation; its own performance characterization remains pending.

The [measurement extract](measurements.json) contains retained values, workloads and SHA-256 identities for the maintainer's source records.

## Recorded results

| Study | Metric | Before | After | Measured scope |
| --- | --- | --- | --- | --- |
| R&D-03D | TA parser mean, µs/frame | 854.255 | 788.963 | Saved Solo battle, 3840×1080; 1,200 frames per variant |
| R&D-03E | Emulation mean, cycles/frame | 18,121,266 | 16,789,886 | Matched saved Solo battle, 3840×1080; 600 frames per variant |
| R&D-04A | SLAVE link mean, active cycles/sample | 11,144,486 | 67,436 | Linked original-aspect run; 600 samples per cabinet |

R&D-03D used A/B/B/A runs, 120 warmup frames per run and matching workload totals. Parser P99 was 1,302.100 → 1,263.400 µs. The per-conversion palette lookup base remains in the source. [Palette conversion](../../core/rend/texconv.cpp#L279)

R&D-03E emits the MMU-off lookup directly in the x64 dispatch path and retains the MMU-enabled helper. Process cycles/frame were 25,121,222 → 24,004,624, while render-start P99 increased from 23.9924 to 25.8561 ms. Both results belong in a summary of that pair. [MMU-aware dispatch](../../core/rec-x64/rec_x64.cpp#L743)

R&D-04A changed active receive polling to Windows readiness waiting. MASTER mean active cycles/sample were 46,605 → 47,724, with differing MASTER/SLAVE scene workloads. The later LAN worker transport has no matched benchmark in this study. [Readiness source](../../core/network/naomi_network.cpp#L34)

## Limits and rejected work

Thread cycles exclude descheduled waiting. Host wall time can include driver blocking. Nested conversion and parser stages must not be added together. These results establish neither whole-game FPS gains nor input-to-display latency.

The existing diagnostic instrumentation records bounded host stages and thread/process counters. Exact original CPU, GPU and Windows OS build linkage remains unverified. The [platform note](../hardware/VALIDATION-PLATFORM.md) separates the known build configuration from that hardware gap. [Cycle instrumentation](../../core/rend/dx11/dx11_renderer.cpp#L67)

The R&D-06A row-pointer study remains historical because its measured implementation is absent from the selected source. The R&D-06B load-result handoff, R&D-06C FPU change, R&D-06D FTRV change and R&D-06E local PAL8 unpacker were reverted or rejected. They contribute no claimed gain to consolidated `main`.

The [visual plan](../assets/ASSET-CHECKLIST.md) uses the recorded metrics with their units, sample counts, workload labels and counterexamples.
