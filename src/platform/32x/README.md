# OpenLara for Sega 32X

Build with a Sega cross toolchain containing SH-2 and 68000 GCC, binutils,
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

Checkpoint feedback appears at the bottom of the screen. Checkpoints use the
engine's existing 8 KiB serialization buffer with a separate saved metadata
header. They survive repeated restores within the same level, and are discarded
when changing level, resetting, or powering off. They are **not battery-backed
SRAM saves**. The passport's save/load support uses the same session checkpoint.
Settings persistence and audio remain unimplemented in this platform backend.

Performance changes preserve the existing 320×224 output, two-pixel horizontal
texture sampling, draw distance, and dual-CPU polygon scheduling. The textured
scanline loops now process four pixel pairs per iteration, with the original
pair loop handling short spans and tails. Clipping uses the SH-2's signed
hardware divider, and sprites reuse their projection reciprocal. Sprite and
glyph submissions now consume two vertices instead of accidentally consuming
four. `CPPFLAGS=-DMARS_SOFTWARE_CLIP_DIV` selects the software clipping division
path for experiments; clean before changing build flags.

See [the measured comparison](../../../docs/32x/report.md) for results and limits.

The headless runner uses a PicoDrive libretro core and Python 3 with Pillow:

```sh
python3 tools/32x/emulate.py \
  --core /absolute/path/to/picodrive_libretro.so \
  --rom src/platform/32x/OpenLara.32x \
  --elf src/platform/32x/OpenLara.elf \
  --nm /absolute/path/to/mars/sh-elf/bin/sh-elf-nm \
  --script /absolute/path/to/start.json --frames 2400 --out /tmp/openlara-run
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
position, ROM/core hashes and settings. Host execution time is diagnostic only.

```sh
python3 tools/32x/compare_runs.py /tmp/original/run.json /tmp/improved/run.json
```

To run the on-target raster regression test, clean and rebuild with
`CPPFLAGS=-DMARS_RASTER_SELFTEST`, then run for 1500 video frames. The test stops
normal gameplay after initialization, executes 2,556 textured/Gouraud span cases
and 4,096 signed division comparisons, and leaves result symbols in SDRAM.
Expected FT hash: `1300929039`; GT hash: `1388686019`; done: `1`; math errors:
`0`. Clean and rebuild without that flag to return to the playable ROM.

For a direct regression comparison with the original rasterizer, create a fresh
worktree at upstream commit `3b268ca4bf986c380f44a26ffd9218bf6d376249` and run
`python3 tools/32x/prepare_raster_baseline.py /absolute/path/to/upstream-worktree`.
This copies build support and inserts the same guarded test entry points, while
keeping the original rasterizer. Build that worktree with the same self-test
flag and toolchain, then compare the recorded hashes.
