# OpenLara for Sega 32X

Requires GNU make 4.3 or newer, Python 3, and a Sega cross toolchain containing
SH-2 and 68000 GCC, binutils,
Newlib and the size-optimized SH division library. This version was tested
with the Linux x86-64 archive from
[marsdev v1.0.0-rc1](https://github.com/andwn/marsdev/releases/tag/v1.0.0-rc1),
which contains GCC 13.1.0.

```sh
make clean ROOTDIR=/absolute/path/to/mars
make -j4 ROOTDIR=/absolute/path/to/mars
```

`GENDEV` is also accepted when `ROOTDIR` is not supplied. Linker scripts are
included locally; an existing installation's scripts can be selected with
`LDSCRIPTSDIR=/absolute/path/to/ldscripts`. The SH-2 script checks that static
data leaves room for the stacks. Header, assembly include, packed-level and
68000 binary dependencies are tracked for incremental and parallel builds.

The output is `OpenLara.32x`, padded to 4 MiB. It contains the existing upstream
packed data for Caves, City of Vilcabamba and Lara's Home. The title passport
offers the two levels; the Home icon selects the gym. This does not add the
rest of Tomb Raider's levels.

Use a six-button controller:

| Control | Action |
|---|---|
| D-pad | Move / turn |
| A | Action / confirm |
| B | Jump / cancel |
| C | Draw weapon |
| X | Walk |
| Y | Roll |
| Z | Look |
| Mode | Inventory |
| Start + Z | Save a RAM checkpoint during gameplay |
| Start + X | Restore the checkpoint during gameplay |
| Start + Y | Cycle Genesis worker → Genesis + CD auto → SH2 only |

Checkpoint feedback appears at the bottom of the screen. Checkpoints use the
engine's existing 8 KiB serialization buffer with a separate saved metadata
header. They survive repeated restores within the same level, and are discarded
when changing level, resetting, or powering off. They are **not battery-backed
SRAM saves**. The passport's save/load support uses the same session checkpoint.
Settings persistence remains unimplemented. Sound effects work on the standalone
32X through PWM; Sega CD is not required.

Performance changes preserve the existing 320×224 output, two-pixel horizontal
texture sampling, draw distance, and dual-CPU polygon scheduling. The textured
flat-textured scanline loop processes eight pixel pairs per iteration, with
eight pairs in the Gouraud-textured loop. Short spans and tails retain the pair
loop. A shared 16 KiB table stores duplicated shade bytes to remove per-pair expansion
in both loops. Encoded shades 1–32 are normalized to rows 0–31;
`table + 256` is aligned to 16 KiB for exact fixed-point Gouraud lookup; linker assertions enforce this and the reserved stacks.
`CPPFLAGS=-DMARS_TEXTURE_PAIRS=4` selects the smaller flat-textured unroll. Clipping uses the SH-2's signed
hardware divider, and sprites reuse their projection reciprocal. Sprite and
glyph submissions now consume two vertices instead of accidentally consuming
four. `CPPFLAGS=-DMARS_SOFTWARE_CLIP_DIV` selects the software clipping division
path for experiments; clean before changing build flags.

The audio backend mixes up to four 11,025 Hz mono effects into both PWM outputs.
The Genesis 68000 now decodes/resamples up to two predicted voice blocks while
the SH2s run gameplay/rendering. Its decoder instructions and lookup tables run
from Genesis work RAM. Finished blocks arrive through the 32X FIFO and master
SH2 DMA0; the SH2 mixes current volume and streams PWM through DMA1. Late or
stale predictions use the native SH2 decoder. A worker fault disables offloading
and leaves the SH2 audio path running. Controllers remain serviced by VBlank.

Genesis offloading is the default. Start + Y during gameplay cycles to optional
**Genesis + CD auto**, then **SH2 only**, then back to Genesis. In CD auto mode,
the Genesis can dispatch one voice to the Sega CD CPU while decoding another
itself. An absent/unsupported/unresponsive CD falls back to Genesis. The private
CD bootstrap calls no drive/BIOS functions and needs no disc; it uses only
program RAM, Word RAM and communication registers, never backup RAM. Sega CD
is not required. CD mode is experimental and opt-in: the decoder batch test is
faster, but the measured movement replay has no FPS gain. Reset restores the
default Genesis mode. Physical-console validation remains outstanding.

The voice-major mixer and hot audio IRQ routines use SH2 SDRAM. Samples,
pitch/volume behavior and 6 dB headroom retain the previous integer results.
The build repacks existing GBA effects into a deduplicated IMA ADPCM bank and
emits both CPUs' decoder tables automatically; it downloads no game assets.
Muting SFX stops active effects. Music playback remains unimplemented.

The clock uses VBlank-derived milliseconds so the periodic watchdog interrupt
does not compete with audio DMA on the tested PicoDrive core. Console reset
stops both DMA channels and PWM before reinitializing memory. A fresh CD boot
acknowledgement prevents reusing stale readiness state after reset.

Gouraud spans with a zero horizontal shade step now reuse one lightmap row
through a four-pair fast loop; other spans retain the gradient loop.
See [the constant-shade optimization](../../../docs/32x/constant-shading.md),
[the previous shading comparison](../../../docs/32x/shading-performance.md),
[the CPU worker report](../../../docs/32x/cpu-workers.md), the
[audio/performance comparison](../../../docs/32x/audio-performance.md), and the
[earlier renderer-only report](../../../docs/32x/report.md).

The headless runner uses a PicoDrive libretro core and Python 3 with Pillow:

```sh
python3 tools/32x/emulate.py \
  --core /absolute/path/to/picodrive_libretro.so \
  --rom src/platform/32x/OpenLara.32x \
  --elf src/platform/32x/OpenLara.elf \
  --nm /absolute/path/to/mars/sh-elf/bin/sh-elf-nm \
  --script /absolute/path/to/start.json --frames 2400 --audio --out /tmp/openlara-run
```

Run this command from the repository root. Replay JSON contains
`[start_video_frame, end_video_frame_exclusive, [libretro_button_ids]]` events.
For example, `[[120,132,[1]],[240,252,[1]],[360,372,[1]]]` enters Caves from the
title inventory. IDs are B=0, Y=1, Select=2, Start=3, Up=4, Down=5, Left=6,
Right=7, A=8, X=9, L=10, R=11. The Genesis mapping is A=Y, B=B, C=A, X=L,
Y=X, Z=R, Mode=Select. Each ROM must run in a separate process.

The runner reads PicoDrive's exported `Pico32xMem` and `Pico32x` symbols and
expects little-endian Linux. It is pinned and tested against core commit
`1890c2932234c9d30f4cd3851d02228baae8f09e`; a future core layout may require
updating these reads. Its JSON records page flips, guest FPS, level, player
position, ROM/core hashes, audio counters and settings. `--audio` also writes
`capture.wav`; `--reset-at 1800` exercises reset. Host execution time is diagnostic
only.

```sh
python3 tools/32x/compare_runs.py /tmp/original/run.json /tmp/improved/run.json
```

To run the on-target raster regression test, clean and rebuild with
`CPPFLAGS=-DMARS_RASTER_SELFTEST`, then run for 3600 video frames. The test stops
normal gameplay after initialization, executes 2,556 textured/Gouraud span cases
and 4,096 signed division comparisons, and leaves result symbols in SDRAM.
Expected FT hash: `1300929039`; GT hash: `1388686019`; done: `1`; math errors:
`0`. Another 5,112 boundary-shade cases (encoded shades 1/32, ascending/descending
gradients, all span widths and odd endpoints) must produce `raster_shades=3137317167`.
All 7,668 raster cases match the pinned upstream implementation.
The same build also decodes all 592,276 effect samples: expected audio
hash `1623845143`, audio done `1`. The hash matches an independent IMA decoder.
The new mixer also matches the previous sample-major mixer in 320 cases with
zero errors and hash `4267401245`.
Clean and rebuild without that flag to return to the playable ROM.

For a direct regression comparison with the original rasterizer, create a fresh
worktree at upstream commit `3b268ca4bf986c380f44a26ffd9218bf6d376249` and run
`python3 tools/32x/prepare_raster_baseline.py /absolute/path/to/upstream-worktree`.
This copies build support and inserts the same guarded test entry points, while
keeping the original rasterizer. Build that worktree with the same self-test
flag and toolchain, then compare the recorded hashes.

The asynchronous workers have their own on-target test. Clean and rebuild with:

```sh
make -C src/platform/32x -j4 ROOTDIR=/absolute/path/to/mars \
  CPPFLAGS='-DMARS_WORKER_SELFTEST -DMARS_WORKER_VERIFY -DMARS_WORKER_MODE=2'
```

Run that ROM for 3,000 frames with the headless runner, once normally and once
with `--cd-fixture`. Both should record `TestCases=273`, `Used=546`,
`TestErrors=0`, `VerifyErrors=0`, `TestDone=1`. The CD run should also record
`CDJobs=273`; without CD it should be zero. Repeat with `--interpreter` for the
SH2 interpreter. This covers all 91 effects, partial/end positions and increments
32–1024, checking each output sample and final decoder state against the SH2
reference. Timings are synthetic decoder throughput, not gameplay FPS.

`--cd-fixture` creates original signature-only firmware and a dummy media image
inside the output directory to attach PicoDrive's CD hardware through its normal
cartridge/media loader. It uses no commercial BIOS or game disc and does not
modify the emulator. It exercises the private bootstrap, sub-CPU, ownership
handshakes and actual decoder, but cannot establish retail hardware compatibility.

Fault injection is test-only: `MARS_CD_STALL_TEST` stalls the fourth CD job;
`MARS_MD_STALL_TEST` stalls the fourth Genesis job. The former should fall back
to Genesis; the latter should disable the worker and keep gameplay/SH2 audio
running. Neither define belongs in a playable release. Always clean before
changing build flags and rebuild without test flags for normal play.
