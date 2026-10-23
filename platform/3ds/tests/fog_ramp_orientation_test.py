#!/usr/bin/env python3
"""Check production fog texels with PICA's bottom-origin V addressing.

Uses the actual Morton-layout initialization loop. Sampling is an ideal host
bilinear model, not a GPU quantization or physical-performance measurement.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess


def block(text, marker):
    start = text.index(marker)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def morton(x, y):
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)


def pixel(data, x, y):
    at = ((x // 8) * 64 + morton(x & 7, y)) * 4
    a, b, g, r = data[at:at + 4]
    return r, g, b, a


def sample(data, u, v, channel):
    # The UI's top=1/bottom=0 convention maps physical row 0 to V=1.
    x = min(255, max(0, u * 256 - 0.5))
    y = min(7, max(0, (1 - v) * 8 - 0.5))
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = x - x0, y - y0
    a = pixel(data, x0, y0)[channel] * (1 - fx) + pixel(data, min(255, x0 + 1), y0)[channel] * fx
    b = pixel(data, x0, min(7, y0 + 1))[channel] * (1 - fx) + pixel(data, min(255, x0 + 1), min(7, y0 + 1))[channel] * fx
    return a * (1 - fy) + b * fy


def reference_distance(coordinate):
    x = min(7, max(0, coordinate * 8 - 0.5))
    i = math.floor(x)
    return ((i * 255 + 3) // 7) * (1 - (x - i)) + ((min(7, i + 1) * 255 + 3) // 7) * (x - i)


def verify(data):
    assert len(data) == 256 * 8 * 4
    assert sample(data, 0.5, 0, 1) == 0, 'Near geometry is incorrectly covered by full distance fog'
    assert sample(data, 0.5, 1, 1) == 255
    count = 0
    for cutoff in (3375, 4500, 5000):
        previous = -1
        for step in range(-64, 577):
            distance = cutoff * step / 256
            coordinate = 3.5 / cutoff * distance - 2.5625
            actual = sample(data, 0.5, coordinate, 1)
            assert abs(actual - reference_distance(coordinate)) < 1e-10
            assert actual >= previous
            if distance <= cutoff * 0.75:
                assert actual == 0
            if distance >= cutoff:
                assert actual == 255
            previous = actual
            count += 1
    # Original fog samples R; distance off (including New High) does not read G.
    for factor in range(256):
        u = (factor + 0.5) / 256
        for v in (-2, 0, 0.0625, 0.25, 0.5, 0.9375, 1, 3):
            assert sample(data, u, v, 0) == factor
            assert sample(data, u, v, 2) == 0
            assert sample(data, u, v, 3) == 255
            count += 1
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--before-source', type=Path)
    args = parser.parse_args()
    port = Path(__file__).resolve().parents[1]
    source = (port / 'source/gfx_citro3d.cpp').read_text()
    out = args.output.resolve()
    if out == port.parents[1] or port.parents[1] in out.parents:
        parser.error('Output must be outside Source.')
    out.mkdir(parents=True, exist_ok=True)
    start = source.index('auto* fogPixels =')
    loop = block(source[start:], 'for (uint32_t y = 0; y < 8; ++y)')
    old_source = args.before_source.read_text() if args.before_source else source.replace('((7 - y) * 255 + 3)', '(y * 255 + 3)')
    old_loop = block(old_source[old_source.index('auto* fogPixels ='):], 'for (uint32_t y = 0; y < 8; ++y)')
    outputs = {}
    commands = []
    for name, body in [('corrected', loop), ('negative-old-V', old_loop)]:
        cpp, exe = out / (name + '.cpp'), out / name
        cpp.write_text('#include <cstdint>\n#include <cstdio>\n' + block(source, 'constexpr uint32_t MortonOffset8x8(') + '\nint main(){uint32_t pixels[256*8]{};auto* fogPixels=pixels;\n' + body + '\nreturn std::fwrite(pixels,1,sizeof(pixels),stdout)==sizeof(pixels)?0:1;}\n')
        command = ['clang++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(cpp), '-o', str(exe)]
        commands.extend([command, [str(exe)]])
        subprocess.run(command, check=True)
        result = subprocess.run([str(exe)], check=True, capture_output=True)
        (out / (name + '.rgba')).write_bytes(result.stdout)
        outputs[name] = result.stdout
    count = verify(outputs['corrected'])
    try:
        verify(outputs['negative-old-V'])
    except AssertionError as error:
        negative = str(error)
    else:
        raise AssertionError('The original inverted V ramp passed unexpectedly')
    for x in range(256):
        for y in range(8):
            before, after = pixel(outputs['negative-old-V'], x, y), pixel(outputs['corrected'], x, y)
            assert before[0] == after[0] and before[2:] == after[2:]
    record = {'passed': True, 'bilinear_cases': count, 'original_R_blue_alpha_bytes_unchanged': True, 'negative_control': negative, 'scope': 'Actual texel loop and ideal PICA-coordinate sampling; no physical GPU run.'}
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    (out / 'summary.json').write_text(json.dumps(record, indent=2) + '\n')
    print(f'PASS: {count} fog samples; near distance stays clear, cutoff fully fogged; original R and inactive-G paths unchanged.')
    print('PASS negative control:', negative)


if __name__ == '__main__':
    main()
