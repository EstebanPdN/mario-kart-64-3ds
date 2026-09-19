#!/usr/bin/env python3
"""Exercise cache ownership/math and the actual patched Fast3D dispatch tables."""
import argparse
import pathlib
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=pathlib.Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
root = pathlib.Path(__file__).resolve().parents[3]
port = root / "platform/3ds"
upstream = root / "third_party/SpaghettiKart/libultraship"
source = upstream / "src/fast/interpreter.cpp"

def run(command):
    subprocess.run([str(part) for part in command], check=True)

compile_args = ["clang++", "-std=c++20", "-O1", "-g", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror",
                "-I", port / "include", "-I", upstream / "include"]
run(compile_args + [port / "tests/fast3d_reuse_test.cpp", "-o", output / "reuse-test"])
run([output / "reuse-test"])

# The normal parent CMake adds this header before the reuse patch. Isolate only
# that prerequisite; the production reuse patch itself is executed unmodified.
driver = output / "apply-reuse.cmake"
driver.write_text(f'''file(READ "{source}" SPAGHETTIKART_3DS_INTERPRETER_TEXT)
string(REPLACE "#include \\"fast/types.h\\"" "#include \\"fast/types.h\\"\\n#include \\"fast3d_math_3ds.hpp\\""
  SPAGHETTIKART_3DS_INTERPRETER_TEXT "${{SPAGHETTIKART_3DS_INTERPRETER_TEXT}}")
include("{port / 'fast3d_reuse.cmake'}")
file(WRITE "{output / 'interpreter-reuse.cpp'}" "${{SPAGHETTIKART_3DS_INTERPRETER_TEXT}}")
''')
run(["cmake", "-P", driver])
patched = (output / "interpreter-reuse.cpp").read_text()
tables = patched[patched.index("class UcodeHandler {"):patched.index("const char* GfxGetOpcodeName")]
step = patched[patched.index("static void gfx_step() {"):patched.index("void Interpreter::SpReset()")]
handlers = sorted(set(re.findall(r'"[^"]+",\s*(\w+)\s*}', tables)))
stubs = "\n".join(f"bool {name}(F3DGfx** c) {{ return Invoke({i}, c); }}" for i, name in enumerate(handlers))
test = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include "fast/lus_gbi.h"
#include "fast/ucodehandlers.h"
using namespace Fast;
using GfxOpcodeHandlerFunc = bool (*)(F3DGfx**);
UcodeHandlers ucode_handler_index = ucode_f3dex2;
static int invoked = -1, behavior = 0;
static std::uintptr_t operand = 0;
static bool Invoke(int id, F3DGfx** cmd) {
    invoked = id;
    operand = (*cmd)->words.w1;
    if (behavior == 1) *cmd += 2; // live multiword command
    if (behavior == 2) { *cmd += 3; return true; } // handler-controlled branch
    return false;
}
STUBS
TABLES
static struct Stack { F3DGfx* command; F3DGfx*& currCmd() { return command; } } g_exec_stack;
static void gfx_set_ucode_handler(UcodeHandlers ucode) { ucode_handler_index = ucode; }
#define SPDLOG_CRITICAL(...) ((void)0)
STEP
static GfxOpcodeHandlerFunc Reference(int8_t opcode, unsigned ucode) {
    if (otrHandlers.contains(opcode)) return otrHandlers.at(opcode).second;
    if (rdpHandlers.contains(opcode)) return rdpHandlers.at(opcode).second;
    if (ucode < ucode_handlers.size() && ucode_handlers[ucode]->contains(opcode))
        return ucode_handlers[ucode]->at(opcode).second;
    return nullptr;
}
int main() {
    std::size_t cases = 0;
    for (unsigned ucode = 0; ucode <= ucode_max; ++ucode) {
        ucode_handler_index = static_cast<UcodeHandlers>(ucode);
        for (unsigned op = 0; op < 256; ++op) {
            auto opcode = static_cast<int8_t>(op);
            auto reference = Reference(opcode, ucode);
            assert(Mk64OpcodeHandler3DS(opcode) == reference);
            if (opcode == F3DEX2_G_LOAD_UCODE) continue;
            for (behavior = 0; behavior < 3; ++behavior) {
                F3DGfx commands[8]{};
                commands[0].words = {op << 24, 0x10000U + cases};
                invoked = -1;
                int expected = -1;
                if (reference) {
                    auto* test = commands;
                    reference(&test);
                    expected = invoked;
                }
                invoked = -1;
                g_exec_stack.command = commands;
                gfx_step();
                assert(invoked == expected);
                assert(g_exec_stack.command == commands + (reference && behavior ? 3 : 1));
                if (reference) assert(operand == commands[0].words.w1);
                ++cases;
            }
        }
    }
    for (auto op : {OTR_G_VTX_OTR_FILEPATH, OTR_G_SETTIMG_OTR_FILEPATH,
                   OTR_G_DL_OTR_FILEPATH, OTR_G_PUSHCD, OTR_G_MTX_OTR_FILEPATH}) {
        for (std::uintptr_t bad : {std::uintptr_t(0), std::uintptr_t(65535),
                                  std::numeric_limits<std::uintptr_t>::max()}) {
            F3DGfx commands[2]{};
            commands[0].words = {std::uint32_t(std::uint8_t(op)) << 24, bad};
            invoked = -1;
            g_exec_stack.command = commands;
            gfx_step();
            assert(invoked == -1 && g_exec_stack.command == commands + 1);
        }
    }
    for (unsigned ucode = 0; ucode < ucode_max; ++ucode) {
        F3DGfx commands[2]{};
        commands[0].words.w0 = (std::uint32_t(std::uint8_t(F3DEX2_G_LOAD_UCODE)) << 24) | ucode;
        g_exec_stack.command = commands;
        gfx_step();
        assert(static_cast<unsigned>(ucode_handler_index) == ucode);
        assert(g_exec_stack.command == commands + 1);
    }
    std::printf("PASS: %zu generated dispatch cases, precedence, all ucodes, live operands, branches, multiword guards\n", cases);
}
'''.replace("STUBS", stubs).replace("TABLES", tables).replace("STEP", step)
(output / "dispatch-test.cpp").write_text(test)
run(compile_args + ["-Wno-unused-variable", output / "dispatch-test.cpp", "-o", output / "dispatch-test"])
run([output / "dispatch-test"])
