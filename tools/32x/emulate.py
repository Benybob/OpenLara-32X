#!/usr/bin/env python3
"""Deterministic headless PicoDrive/libretro runner; no emulated overclock.

Uses the core's exported SDRAM pointer for read-only guest telemetry. Run each
ROM in a new process. Requires Pillow for screenshots; all timings are guest
video frames, never host execution speed.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import subprocess
import time

class Variable(C.Structure):
    _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]

class Game(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p),
                ('size', C.c_size_t), ('meta', C.c_char_p)]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--core', required=True)
    p.add_argument('--rom', required=True)
    p.add_argument('--elf', required=True)
    p.add_argument('--nm', required=True)
    p.add_argument('--out', required=True)
    p.add_argument('--frames', type=int, default=1800)
    p.add_argument('--script', help='JSON controller events: [start,end,[button IDs]]')
    p.add_argument('--region', choices=['US', 'Europe'], default='US')
    p.add_argument('--interpreter', action='store_true')
    p.add_argument('--capture-every', type=int, default=300)
    p.add_argument('--capture-frames', default='', help='Additional comma-separated frame numbers')
    p.add_argument('--reset-at', type=int, help='Reset the emulated console at this video frame')
    a = p.parse_args()
    out = Path(a.out); out.mkdir(parents=True, exist_ok=True)
    symbols = {}
    for line in subprocess.check_output([a.nm, '-n', a.elf], text=True).splitlines():
        bits = line.split()
        if len(bits) == 3:
            symbols[bits[2]] = int(bits[0], 16)
    events = json.loads(Path(a.script).read_text()) if a.script else []
    core = C.CDLL(str(Path(a.core).resolve()))
    defaults = {}
    overrides = {'picodrive_input1': b'6 button pad', 'picodrive_region': a.region.encode(),
                 'picodrive_drc': b'disabled' if a.interpreter else b'enabled',
                 'picodrive_frameskip': b'disabled', 'picodrive_overclk68k': b'disabled'}
    directory = str(out.resolve()).encode()
    pixel_format = 0
    frame = 0
    extra_captures = {int(x) for x in a.capture_frames.split(',') if x}
    last_image = None
    @C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
    def environment(cmd, data):
        nonlocal pixel_format
        if cmd in (9, 30, 31):
            C.cast(data, C.POINTER(C.c_char_p))[0] = directory
            return True
        if cmd == 10:
            pixel_format = C.cast(data, C.POINTER(C.c_int))[0]
            return pixel_format in (0, 1, 2)
        if cmd == 15:
            v = C.cast(data, C.POINTER(Variable)).contents
            v.value = overrides.get(v.key.decode(), defaults.get(v.key.decode()))
            return bool(v.value)
        if cmd == 16:
            array = C.cast(data, C.POINTER(Variable)); i = 0
            while array[i].key:
                text = array[i].value.decode()
                defaults[array[i].key.decode()] = text.split(';', 1)[1].strip().split('|')[0].encode()
                i += 1
            return True
        if cmd == 17:
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        if cmd == 3:
            C.cast(data, C.POINTER(C.c_bool))[0] = True
            return True
        if cmd in (18, 35):
            return True
        return False
    @C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
    def video(data, width, height, pitch):
        nonlocal last_image
        if not data:
            return
        if frame % a.capture_every != 0 and frame != a.frames - 1 and frame not in extra_captures:
            return
        from PIL import Image
        raw = C.string_at(data, pitch * height)
        rgb = bytearray(width * height * 3)
        for y in range(height):
            for x in range(width):
                i = y * pitch + x * (4 if pixel_format == 1 else 2)
                v = int.from_bytes(raw[i:i+(4 if pixel_format == 1 else 2)], 'little')
                if pixel_format == 1: r,g,b = (v>>16)&255,(v>>8)&255,v&255
                elif pixel_format == 2: r,g,b = ((v>>11)&31)*255//31,((v>>5)&63)*255//63,(v&31)*255//31
                else: r,g,b = ((v>>10)&31)*255//31,((v>>5)&31)*255//31,(v&31)*255//31
                j=(y*width+x)*3; rgb[j:j+3]=bytes((r,g,b))
        last_image=Image.frombytes('RGB',(width,height),bytes(rgb))
        last_image.save(out / f'frame-{frame:05d}.png')
    @C.CFUNCTYPE(None, C.c_int16, C.c_int16)
    def audio(left, right): pass
    @C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
    def audio_batch(data, frames): return frames
    @C.CFUNCTYPE(None)
    def poll(): pass
    @C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)
    def input_state(port, device, index, button):
        return int(port == 0 and any(start <= frame < end and button in buttons
                                    for start,end,buttons in events))
    for name, cb in [('environment',environment), ('video_refresh',video),
                     ('audio_sample',audio), ('audio_sample_batch',audio_batch),
                     ('input_poll',poll), ('input_state',input_state)]:
        fn=getattr(core,'retro_set_'+name); fn.argtypes=[type(cb)]; fn(cb)
    core.retro_init()
    core.retro_load_game.argtypes=[C.POINTER(Game)]; core.retro_load_game.restype=C.c_bool
    rom=Path(a.rom).resolve()
    if not core.retro_load_game(C.byref(Game(str(rom).encode(),None,0,None))):
        raise RuntimeError('ROM failed to load')
    core.retro_set_controller_port_device(0,1)
    def read_address(addr, size=4):
        if addr is None or not 0x06000000 <= addr < 0x06040000: return None
        base=C.c_void_p.in_dll(core,'Pico32xMem').value
        if base is None: return None
        offset=addr-0x06000000
        return int.from_bytes(bytes(C.c_ubyte.from_address(base+((offset+i)^1)).value
                                    for i in range(size)), 'big')
    def read_symbol(name, size=4):
        return read_address(symbols.get(name), size)
    regs=(C.c_ushort * 48).in_dll(core,'Pico32x')
    records=[]; flips=[]; last_page=None
    start=time.monotonic()
    for frame in range(a.frames):
        if frame == a.reset_at: core.retro_reset()
        core.retro_run()
        page=regs[32+5]&1
        if page != last_page:
            flips.append(frame); last_page=page
        player=read_symbol('_players')
        pos=[read_address(player+8+i*4) for i in range(3)] if player and 0x06000000<=player<0x06040000 else None
        if pos is not None: pos=[n if n < 0x80000000 else n-0x100000000 for n in pos]
        angle=read_address(player+22,2) if pos is not None else None
        records.append({'frame':frame,'page':page,'fps':read_symbol('_fps'),
                        'vblank':read_symbol('_gFrameIndex'),
                        'level':read_symbol('_gLevelID',1),
                        'vertices':read_symbol('_gVerticesBase'),
                        'faces':read_symbol('_gFacesBase'), 'player_pos':pos, 'player_yaw':angle,
                        'save_size':read_address(symbols['_gSaveGame']+4) if '_gSaveGame' in symbols else None,
                        'game_time':read_address(symbols['_gSaveGame']+12) if '_gSaveGame' in symbols else None,
                        'raster_cases':read_symbol('_gRasterTestCases'),
                        'raster_ft':read_symbol('_gRasterTestFT'),
                        'raster_gt':read_symbol('_gRasterTestGT'),
                        'raster_done':read_symbol('_gRasterTestDone'),
                        'math_errors':read_symbol('_gRasterTestMathErrors')})
    result={'rom':rom.name, 'rom_sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),
            'input_events':events,
            'region':a.region,'emulated_hz':50 if a.region=='Europe' else 60,
            'frames':a.frames,'core_sha256':hashlib.sha256(Path(a.core).read_bytes()).hexdigest(),
            'interpreter':a.interpreter,'overrides':{k:v.decode() for k,v in overrides.items()},
            'host_seconds':time.monotonic()-start,'page_flips':flips,'samples':records}
    (out/'run.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'rom':rom.name,'host_seconds':result['host_seconds'],
                      'flips':len(flips), 'last_sample':records[-1]}))
    core.retro_unload_game(); core.retro_deinit()
if __name__ == '__main__': main()
