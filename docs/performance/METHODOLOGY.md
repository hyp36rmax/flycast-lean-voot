# Performance measurement methodology

Verified evidence summary for review. The original approved November 6 V1.0 wording is still pending import. Existing [performance studies](PERFORMANCE.md) retain their measurements, rejected results and workload limits; this page does not create a new benchmark result.

## What is measured

The retained investigations use Windows thread and process cycle counters (`QueryThreadCycleTime`, `QueryProcessCycleTime`), thread/process CPU-time APIs (`GetThreadTimes`, `GetProcessTimes`), and elapsed-time sources including `std::chrono::steady_clock` and .NET `Stopwatch`.

Cycle counts measure counted host execution work. They are not elapsed time, emulated clock speed, GPU time or input latency. CPU-time units also do not imply that every measurement has that resolution. Render-start intervals describe observed host cadence, not automatically the rate of unique presented gameplay frames.

## Matched comparisons

I compare the same game state, aspect, render configuration and input conditions, separate warmup from measured samples, and keep the study's actual baseline and candidate identities. The retained studies document their own sample counts; they are not pooled into a whole-runtime speedup.

The recorded dispatch and palette studies used saved Solo workloads. The cabinet-wait study measured MASTER and SLAVE separately. Different scene workloads or run lengths remain explicit limitations. Alternating baseline/candidate runs and repeated comparisons help distinguish a local change from run-to-run variation.

## Interpreting results

Local-path timing is reported separately from emulation-thread and total-process work. A subsystem reduction is not a general FPS increase. Negative results and P99 regressions remain alongside mean reductions. Upload bytes record submitted data, not a direct measurement of memory bandwidth or GPU work.

The current consolidated source and the older frozen executables retain their own identities and validation scope. Historical counter experiments do not prove the final binary includes every measured experiment. Instrumented study builds are distinguished from diagnostics-off Release builds.

## Limits

The verified records do not establish a general gameplay FPS gain, GPU-time reduction or input-to-display latency improvement. Exact historical CPU/GPU and Windows-build linkage is incomplete for some studies. No PIX or WPA result is claimed. New performance claims require new matched evidence rather than a reinterpretation of these measurements.
