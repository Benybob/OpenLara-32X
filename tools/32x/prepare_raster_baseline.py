#!/usr/bin/env python3
"""Prepare a fresh upstream worktree for the same on-target raster test.

Changes only build support, the divider helper and guarded test entry points.
The upstream rasterizer and all other gameplay/rendering logic remain intact.
"""
import argparse
from pathlib import Path
import shutil
import subprocess

BASE = '3b268ca4bf986c380f44a26ffd9218bf6d376249'

def main():
    p = argparse.ArgumentParser()
    p.add_argument('upstream', help='Fresh, clean detached worktree at the pinned upstream commit')
    a = p.parse_args()
    dst = Path(a.upstream).resolve()
    source = Path(__file__).resolve().parents[2] / 'src/platform/32x'
    if subprocess.check_output(['git', '-C', str(dst), 'rev-parse', 'HEAD'], text=True).strip() != BASE:
        raise SystemExit('Baseline HEAD does not match the pinned upstream commit')
    if subprocess.check_output(['git', '-C', str(dst), 'status', '--porcelain'], text=True).strip():
        raise SystemExit('Baseline worktree must be clean')
    platform = dst / 'src/platform/32x'
    for name in ('Makefile', 'src-md/Makefile', '32x.h', 'worker.h', 'raster_tests.inc', '.gitignore'):
        shutil.copy2(source / name, platform / name)
    md_make = platform / 'src-md/Makefile'
    md_make.write_text(md_make.read_text().replace(' worker.o worker_blob.o worker_fast.o', ''))
    shutil.copytree(source / 'ldscripts', platform / 'ldscripts', dirs_exist_ok=True)
    tools = dst / 'tools/32x'
    tools.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Path(__file__).with_name('pack_sfx.py'), tools / 'pack_sfx.py')
    main_cpp = platform / 'main.cpp'
    text = main_cpp.read_text()
    anchor = '    gameInit();'
    if text.count(anchor) != 1: raise SystemExit('Unexpected baseline main.cpp')
    main_cpp.write_text(text.replace(anchor, anchor + '\n#ifdef MARS_RASTER_SELFTEST\n'
        '    extern void marsRasterSelfTest();\n    marsRasterSelfTest();\n    while (1);\n#endif'))
    render = platform / 'render.cpp'
    render.write_text(render.read_text() + '\n#ifdef MARS_RASTER_SELFTEST\n'
                      '#include "raster_tests.inc"\n#endif\n')
    print('Prepared test-only baseline:', dst)

if __name__ == '__main__': main()
