# Constant-shade span optimization

Gouraud spans whose integer horizontal shade step is zero now compute the
lightmap row once per span. A four-pair loop reuses that row. Other spans retain
the v4 gradient loop. Odd endpoints and short tails preserve the same UV steps,
word ordering and shade arithmetic. No resolution or draw-distance reduction is
introduced. The mask is formed with two instructions to avoid a literal-load
range limit as the assembly grows. No new RAM buffer is allocated.

| NTSC replay | v4 | v5 | Change |
|---|---:|---:|---:|
| Opening | 28.50 | 30.00 | +5.3% |
| Turning | 26.81 | 27.16 | +1.3% |
| Movement | 19.34 | 19.59 | +1.3% |

FPS counts page flips over 2,400 video frames, excluding the first 480; stock
clocks and no frameskip. PicoDrive core remains
`1890c2932234c9d30f4cd3851d02228baae8f09e`. Baseline is published v4 commit
`da153b760ad454c10ece59f5a0c9300bd79d77ac`. Identical controller replays can
follow slightly different trajectories because input is sampled at render
boundaries; the opening is the most directly comparable stationary workload.
These measurements do not guarantee a universal speedup or real-console FPS.

An eight-pair constant-span candidate produced the same three replay rates;
the smaller four-pair implementation was retained. All 2,556 original raster
cases and 5,112 shade-boundary cases match v4 in recompiler and interpreter:
FT `1300929039`, GT `1388686019`, boundary shades `3137317167`.
All 4,096 divider checks pass. ADPCM remains 592,276 samples with hash
`1623845143`; all 320 mixer cases pass with hash `4267401245`.

Sound and optional Sega CD workers remain supported. Physical hardware remains
untested. See [results-v5.json](results-v5.json), the raw emulator evidence in
the download, and [build/controls](../../src/platform/32x/README.md).
