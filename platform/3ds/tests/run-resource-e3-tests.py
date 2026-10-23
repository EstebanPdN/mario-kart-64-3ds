#!/usr/bin/env python3
"""Focused resource tests, optional private captured replay; no game execution."""
import argparse
from pathlib import Path
import json
import shlex
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--entries',type=Path)
p.add_argument('--replay',type=Path)
p.add_argument('--pack',type=Path)
p.add_argument('--archive',type=Path)
p.add_argument('--baseline',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[3];port=root/'platform/3ds';out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
assert root not in out.parents and out!=root
flags=['clang++','-std=c++20','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(port/'include')]
results=[]
with (out/'commands.log').open('w') as commands, (out/'results.log').open('w') as log:
 def run(command,cwd=out):
  command=[str(x) for x in command];commands.write(shlex.join(command)+'\n');commands.flush()
  result=subprocess.run(command,cwd=cwd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  log.write(result.stdout);log.flush();print(result.stdout,end='');result.check_returncode();return result.stdout
 def test(name,sources=(),extra=(),arguments=()):
  binary=out/name
  run([*flags,port/'tests'/f'{name}.cpp',*sources,*extra,'-o',binary])
  work=out/(name+'-work');work.mkdir(exist_ok=True)
  run([binary,*arguments],work);results.append(name)
 for name in ('improved_texture_pack_test','texture_retention_test','native_sprite_page_test','resources_cache_regression_test'):
  test(name,extra=['-lz'])
 upstream=root/'third_party/SpaghettiKart/libultraship'
 test('improved_resource_cache_test',sources=[port/'source/resource_manager_3ds.cpp',upstream/'src/fast/resource/type/Texture.cpp'],extra=['-DMK64_3DS_RESOURCE_MANAGER','-DMK64_3DS_GAME_TEXTURE_PACK','-I'+str(port/'probe_stubs'),'-I'+str(upstream/'include'),'-lz'])
 run([sys.executable, port/'tests/resource_admission_e3_test.py', '--output', out/'admission'])
 results.append('resource_admission_e3_test')
 if a.pack:
  assert a.entries and a.replay and a.archive
  test('resources_captured_replay_test',extra=['-lz'],arguments=[a.pack.resolve(),a.entries.resolve(),a.replay.resolve(),a.archive.resolve()])
  if a.baseline:
   binary=out/'resources_captured_replay_baseline'
   instrumented=out/'instrumented-baseline';instrumented.mkdir(exist_ok=True)
   for header in ('improved_texture_pack_3ds.hpp','native_sprite_page_3ds.hpp','read_ahead_file_3ds.hpp'):
    text=(a.baseline/header).read_text()
    if header=='improved_texture_pack_3ds.hpp':text=text.replace('std::fread','ResourceReplayFread')
    (instrumented/header).write_text(text)
   run([*flags[:6],'-I'+str(instrumented),*flags[6:],'-DE3_RESOURCE_BASELINE',port/'tests/resources_captured_replay_test.cpp','-lz','-o',binary])
   run([binary,a.pack.resolve(),a.entries.resolve(),a.replay.resolve(),a.archive.resolve()])
(out/'summary.json').write_text(json.dumps({'passed':results,'sanitizers':['address','undefined'],'game_or_emulator':False},indent=2)+'\n')
