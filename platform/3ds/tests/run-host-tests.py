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
        run([sys.executable, port / 'tests/title_credit_pipeline_test.py', '--output', output / 'title-credit-pipeline'])
        run([compile_test('audio_dma_range_cache_test')], cwd=output)
        run([compile_test('audio_ndsp_notification_test', [
            '-I', port / 'tests/audio_ndsp_sdk_stub', port / 'source/audio_ndsp_3ds.cpp'])], cwd=output)
        run([sys.executable, port / 'tests/resource_loading_test.py', '--output', output / 'course-loading'])
        for fixture in ('autodump_lab_test.py', 'laboratory_boundary_test.py', 'performance_trace_test.py'):
            run([sys.executable, port / 'tests' / fixture, output / Path(fixture).stem])
        run([sys.executable, port / 'tests/run-resource-e3-tests.py', '--output', output / 'e3-resources'])
        run([sys.executable, port / 'tests/resource_recovery_e3_test.py', '--output', output / 'e3-recovery'])
        run([compile_test('improved_texture_pack_test', ['-lz'])],cwd=output)
        for name in ('native_sprite_test', 'options_layout_test', 'runtime_policies_test', 'file_replace_test', 'texture_lease_test', 'texture_retention_test', 'texture_mode_test'):
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
            assert 'ui_textures=improved\n' in config.read_text()
            config.write_text('ui_textures=menu\n')
            for _ in range(2):
                run([settings, 'menu'], cwd=work, env=env)
            assert 'ui_textures=menu\n' in config.read_text()
            config.write_text('ui_textures=unknown\n')
            run([settings, 'invalid-textures'], cwd=work, env=env)
            # Missing main config recovers from the last complete FAT backup.
            config.unlink()
            backup=config.with_name(config.name+'.bak')
            backup.write_text('ui_textures=scaled\n')
            run([settings, 'scaled'], cwd=work, env=env)
            assert config.exists() and 'ui_textures=improved\n' in config.read_text()
            assert not backup.exists()
            fixtures={
                'frame-off':'frame_limit=off\nframe_limiter=on\n',
                'frame-off-reversed':'frame_limiter=on\nframe_limit=off\n',
                'frame-legacy':'frame_limiter=off\n',
                'legacy':'top_hud=on\nrender_scale=85\nturbo_speed=1\noverlay=on\nzr_debug=on\nlogging=off\n',
                'conflict':'select_for_dump=on\nselect_for_overlay=on\n',
                'mismatch':'profile=quality\nrender_scale=75\n',
            }
            for mode,text in fixtures.items():
                config.write_text(text)
                run([settings,mode],cwd=work,env=env)
        run([sys.executable, port / 'tests/options_integration_test.py',
             '--output', output / 'ui-integration'])
        run([sys.executable, port / 'tests/standing_rotation_test.py',
             '--output', output / 'standing-rotation'])
    if args.suite in ('all', 'renderer'):
        run([compile_test('renderer_e3_static_test')], cwd=output)
        for fixture in ('fog_ramp_orientation_test.py', 'renderer_e3_interpreter_test.py', 'renderer_e3_texture_test.py',
                        'renderer_e3_fog_backend_test.py', 'renderer_e3_fog_integration_test.py', 'renderer_e3_material_test.py',
                        'renderer_e3_packing_test.py'):
            run([sys.executable, port / 'tests' / fixture, '--output', output / Path(fixture).stem])
        run([compile_test('static_runs_test')],cwd=output)
        run([sys.executable, port / 'tests/static_run_backend_test.py', '--output', output / 'static-backend'])
        run([sys.executable, port / 'tests/texture_image_cache_test.py', '--output', output / 'texture-image'])
        run([compile_test('native_fast_page_test')],cwd=output)
        run([compile_test('native_visibility_test')],cwd=output)
        run([compile_test('native_scene_submission_test', [
            '-I',port / 'tests/native_scene_sdk_stub',
            '-include',port / 'tests/native_scene_sdk_stub/3ds.h',
            port / 'source/native_gpu_scene_3ds.cpp',
            port / 'source/native_mesh_draw_3ds.cpp',
            port / 'source/native_material_3ds.cpp'])],cwd=output)
        for fixture in ('resource_name_cache_test.py', 'native_sprite_publication_test.py', 'native_projection_owner_test.py', 'loading_audio_scope_test.py'):
            run([sys.executable, port / 'tests' / fixture, '--output', output / Path(fixture).stem])
        upstream = root / 'third_party/SpaghettiKart/libultraship'
        run([compile_test('improved_resource_cache_test', [
            '-DMK64_3DS_RESOURCE_MANAGER', '-DMK64_3DS_GAME_TEXTURE_PACK',
            '-I', port / 'probe_stubs', '-I', upstream / 'include',
            port / 'source/resource_manager_3ds.cpp',
            upstream / 'src/fast/resource/type/Texture.cpp', '-lz'])], cwd=output)
        run([sys.executable, port / 'tests/texture_owner_release_test.py',
             '--output', output / 'owner-release'])
        run([sys.executable, port / 'tests/resource_data_lease_test.py',
             '--output', output / 'data-lease'])
        run([sys.executable, port / 'tests/native_working_set_test.py',
             '--output', output / 'native-working-set'])
        run([sys.executable, port / 'tests/texture_cache_lifecycle_test.py',
             '--output', output / 'cache-lifecycle'])
        for name in ('native_geometry_test', 'native_lighting_test', 'native_mesh_format_test', 'native_scene_format_test', 'native_camera_test', 'native_exit_state_test', 'native_course_contract_test', 'native_frame_queue_test', 'native_preload_test', 'native_sprite_pack_test', 'native_original_tiles_test', 'native_wheel_palettes_test', 'presentation_fps_test', 'native_anchor_queue_test', 'native_sprite_batch_test', 'native_draw_reference_test', 'native_vegetation_exit_test', 'native_projectile_contract_test', 'alpha_policy_test', 'palette_copy_test', 'native_dynamic_texture_test', 'native_batch_comparison_test', 'native_particle_contract_test', 'native_particle_pack_test', 'native_comparison_span_test', 'native_frame_comparison_test', 'native_texture_storage_test'):
            run([compile_test(name)], cwd=output)
        run([compile_test('native_sprite_page_test', ['-lz'])],cwd=output)
        run([sys.executable, port / 'tests/native_old_scheduling_test.py', '--output', output / 'old-scheduling'])
        run([sys.executable, port / 'tests/native_program_geometry_test.py', '--output', output / 'native-program'])
        run([compile_test('gpu_command_publication_test', [
            '-I',port / 'tests/gpu_command_sdk_stub',
            port / 'source/gpu_command_publication_3ds.cpp'])],cwd=output)
        run([compile_test('frame_begin_timing_test', [
            '-I',port / 'tests/frame_begin_sdk_stub'])],cwd=output)
        run([compile_test('native_mesh_layout_test', [
            '-I',port / 'tests/native_mesh_sdk_stub',
            port / 'source/native_mesh_draw_3ds.cpp'])],cwd=output)
        run([compile_test('native_material_test', [
            '-I',port / 'tests/native_material_sdk_stub',
            port / 'source/native_material_3ds.cpp'])],cwd=output)
        run([sys.executable, port / 'tests/texture_bind_compat_test.py', '--output', output / 'texture-bind'])
        run([sys.executable, port / 'tests/run-reuse-tests.py', '--output', output / 'reuse'])
    print('PASS: host regression suite ' + args.suite + '; no emulator or hardware result implied.')


if __name__ == '__main__':
    main()
