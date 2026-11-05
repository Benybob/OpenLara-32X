# Clipping and renderer timing

This pass retains a small, exact clipping optimization and adds opt-in timing
diagnostics. More aggressive polygon-balancing experiments were rejected after
they slowed down the replay tests. The polygon assignment and bucket barriers
remain unchanged; screen-band rendering has not been implemented.

| NTSC replay | v5 FPS | v6 FPS | Change |
|---|---:|---:|---:|
| Opening | 30.00000 | 30.00000 | 0.00% |
| Turning | 27.15625 | 27.25000 | +0.35% |
| Movement | 19.59375 | 19.62500 | +0.16% |

These are small gains: three additional page flips while turning and one during
movement over the measured 32 seconds. They are not evidence of a major speedup.
Each replay runs for 2,400 video frames, excluding the first 480, with stock
clocks, no frameskip, and PicoDrive commit
`1890c2932234c9d30f4cd3851d02228baae8f09e`. Baseline is published v5 commit
`17ca9df452e12d6c3122748d2c3d4ccc03699864`. Inputs are sampled at render boundaries,
so moving replays can follow different trajectories. Exact results, ROM hashes,
positions and validation summaries are in [results-v6.json](results-v6.json).

## Retained change

`drawPoly` first checks whether the input vertices cross either screen axis.
It runs only the necessary clipping passes, retaining the existing integer
intersection arithmetic and vertex order. A polygon already inside an axis
does not need to be copied through that pass. Horizontal clipping cannot move
an interpolated vertical coordinate outside the input's vertical range.
The two temporary arrays retain their previous size; no new release buffer is
allocated. Static RAM still ends at `0x0603C648`; the ROM remains 4 MiB.

## What the diagnostics found

In the final movement diagnostic, the master spends 33.9% and the slave 38.5%
of measured flush-segment time waiting at bucket barriers. This is not overall
CPU utilization, and the percentages must not be converted directly into an
expected FPS gain. Geometry preparation, gameplay and waits outside these
segments are not measured. The diagnostic's own overhead changes timings.

The optional `MARS_RENDER_PROFILE` build reads the interval timer with /8192
scaling, interrupt priority zero and no watchdog-reset mode. Each CPU publishes
work ticks, barrier ticks, bucket count and flush count through an uncached
telemetry row. The runner reads those values; `profile_runs.py` subtracts the
warmup snapshot and summarizes each CPU. Normal builds omit this instrumentation.
The 8-bit timer wraps after roughly 90 ms at NTSC stock clocks; an individual
segment exceeding that interval aliases. Measurements are coarse and validated
only in the pinned emulator. Do not reset during a measurement interval.

Two runtime screen-area scheduling estimates were tried before the workspace
restore; both lost performance and were discarded. Stepping directly between
assigned faces was effectively unchanged and was also discarded. Their raw
trial files did not survive the restore; neither their code nor their numerical
results are used to support the released optimization. The retained ROM was
reconstructed byte-for-byte and all validation below was rerun afterward.

## Validation

- 900 new clipped-polygon cases match v5 across complete cleared framebuffers:
  hash `2151182815`. These include FT/GT triangles and quads crossing each edge,
  both axes, fully inside, fully outside, and exactly on boundaries.
- Both SH2 recompiler and interpreter pass the new cases and all 7,668 existing
  raster cases. Existing FT/GT/shade hashes remain `1300929039`, `1388686019`
  and `3137317167`.
- All 4,096 divider checks pass; 592,276 ADPCM samples retain hash `1623845143`;
  all 320 mixer cases pass with hash `4267401245`.
- PAL, interpreter gameplay, repeated checkpoint/reset, and optional CD mode
  complete with zero worker faults and clipped audio samples in the runs.
  CD validation uses the existing synthetic fixture, not a retail BIOS or disc.
- The reconstructed release ROM SHA256 is
  `e6991404fac291adbf44f2ce71d7bf0a019c64270c92f80d339943d9b17a90e3`.

Physical hardware remains untested. Sound effects and optional Sega CD support
remain available; the CD is not required. Build, controls and diagnostic commands
are in the [platform README](../../src/platform/32x/README.md).
