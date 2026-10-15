#!/usr/bin/env python3
"""Run asset-free host regressions in an isolated output tree, without a ROM.

These fixtures validate portable policies and mocked UI routing, not GPU pixels,
3DS services or physical performance. Target builds remain a separate gate.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def run(command, **options):
    subprocess.run([str(value) for value in command], check=True, **options)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--suite', choices=('all', 'ui', 'renderer'), default='all')
    parser.add_argument('--cxx', default='clang++')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    port = root / 'platform/3ds'
    output = args.output.resolve()
    if output == root or root in output.parents:
        parser.error('Use a disposable output directory outside the source checkout.')
    output.mkdir(parents=True, exist_ok=True)
    flags = [args.cxx, '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
             '-fno-omit-frame-pointer', '-I', port / 'include', '-I', port / 'source']

    def compile_test(name, extra=()):
        binary = output / name
        run(flags + [port / 'tests' / (name + '.cpp'), *extra, '-o', binary])
        return binary

    if args.suite in ('all', 'ui'):
        for name in ('native_sprite_test', 'options_layout_test', 'runtime_policies_test', 'file_replace_test', 'texture_lease_test'):
            run([compile_test(name)], cwd=output)
        settings = compile_test('settings_profiles_test', [port / 'source/settings_3ds.cpp'])
        for model in ('old', 'new'):
            work = output / ('settings-' + model)
            config = work / 'sdmc:/3ds/MK64/mk64-3ds.cfg'
            config.parent.mkdir(parents=True, exist_ok=True)
            # This directory is explicitly disposable. No real SD data is used.
            if config.exists():
                config.unlink()
            env = dict(os.environ)
            env.pop('MK64_TEST_NEW_MODEL', None)
            if model == 'new':
                env['MK64_TEST_NEW_MODEL'] = '1'
            for mode in (None, 'reload'):
                run([settings] + ([mode] if mode else []), cwd=work, env=env)
            config.write_text(config.read_text().replace('ui_textures=original', 'ui_textures=scaled'))
            for _ in range(2):
                run([settings, 'scaled'], cwd=work, env=env)
            assert 'ui_textures=scaled\n' in config.read_text()
            config.write_text('ui_textures=unknown\n')
            run([settings, 'invalid-textures'], cwd=work, env=env)
            # Missing main config recovers from the last complete FAT backup.
            config.unlink()
            backup=config.with_name(config.name+'.bak')
            backup.write_text('ui_textures=scaled\n')
            run([settings, 'scaled'], cwd=work, env=env)
            assert config.exists() and 'ui_textures=scaled\n' in config.read_text()
            assert not backup.exists()
            fixtures={
                'legacy':'top_hud=on\nrender_scale=85\nturbo_speed=1\noverlay=on\nzr_debug=on\nlogging=off\n',
                'conflict':'select_for_dump=on\nselect_for_overlay=on\n',
                'mismatch':'profile=quality\nrender_scale=75\n',
            }
            for mode,text in fixtures.items():
                config.write_text(text)
                run([settings,mode],cwd=work,env=env)
        run([sys.executable, port / 'tests/options_integration_test.py',
             '--output', output / 'ui-integration'])
    if args.suite in ('all', 'renderer'):
        for name in ('native_geometry_test', 'native_lighting_test'):
            run([compile_test(name)], cwd=output)
        run([sys.executable, port / 'tests/run-reuse-tests.py', '--output', output / 'reuse'])
    print('PASS: host regression suite ' + args.suite + '; no emulator or hardware result implied.')


if __name__ == '__main__':
    main()
