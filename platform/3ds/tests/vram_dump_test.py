#!/usr/bin/env python3
"""Exercise the actual VRAM capture and AUTO request gates without a ROM."""
from pathlib import Path
import argparse
import subprocess
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
port = Path(__file__).resolve().parents[1]
text = (port / 'source/diagnostics_3ds.cpp').read_text()
capture = text[text.index('mk64_3ds::VramDumpResult WriteVramCapture('):text.index('\nbool WriteQuickDump(')]
quick = text[text.index('bool WriteQuickDump('):text.index('\nvoid DiagnosticThread(')]
assert quick.index('WriteVramCapture(directory)') < quick.index('WriteScreenCaptures(directory)') < quick.index('ShowDumpProgress(')
assert 'ShowDumpSavedOverlay(complete)' in quick
assert 'Mk64Auto3DSBenchmarkTick' not in (port / 'source/auto_mode_3ds.cpp').read_text()
assert 'Mk64Auto3DSBenchmarkTick' not in (port / 'source/game_main_3ds.cpp').read_text()
service = text[text.index('extern "C" bool Mk64Diagnostics3DSServiceDumpIfRequested() {'):]
service = service[:service.index('    // Disk writes')]
service += '    ++pauseCalls; return true;\n}\n'
requests = []
for name in ('Mk64Diagnostics3DSRequestDump', 'Mk64Diagnostics3DSRequestFullDump', 'Mk64Diagnostics3DSRequestBenchmarkDump'):
    start = text.index('extern "C" bool ' + name + '() {')
    end = text.index('\n}', start) + 2
    requests.append(text[start:end])

prefix = r'''
#include "vram_dump_3ds.hpp"
#include <atomic>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
std::vector<std::uint32_t> vram(6*1024*1024/4);
bool readable=true, queueReady=true, queueOwned=false, autoActive=false;
unsigned beginCalls=0,endCalls=0,pauseCalls=0;
#define OS_VRAM_VADDR reinterpret_cast<std::uintptr_t>(vram.data())
#define OS_VRAM_PADDR 0x18000000U
#define OS_VRAM_SIZE (6U*1024*1024)
bool EnsureDirectory(const char* path){return std::filesystem::create_directory(path)||std::filesystem::is_directory(path);}
bool IsReadableRange(std::uintptr_t address,std::size_t bytes){assert(address==OS_VRAM_VADDR&&bytes==OS_VRAM_SIZE);return readable;}
namespace mk64_3ds {
bool BeginFrame3DS(unsigned flags,const char* stage){
 assert(flags==0&&std::string(stage)=="diagnostic-vram-gpu-queue-wait"&&!queueOwned);
 ++beginCalls;return queueOwned=queueReady;
}}
void C3D_FrameEnd(unsigned flags){assert(flags==0&&queueOwned);++endCalls;queueOwned=false;}
bool Mk64Auto3DSActive(){return autoActive;}
enum {kDumpRequestNone=0,kDumpRequestClean=1,kDumpRequestBottomUi=2,kDumpRequestFullMemory=3,kDumpRequestBenchmark=4,kDumpRequestLab=5};
std::atomic<unsigned> sDumpRequest{kDumpRequestNone};
const char* DumpTriggerName(unsigned){return "test";}
void LogLine(const char*,const char*){}
'''
suffix = r'''
std::string Read(const std::string& path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 for(std::size_t i=0;i<vram.size();++i)vram[i]=static_cast<std::uint32_t>(i*2654435761U);
 const auto result=WriteVramCapture(".");
 assert(result.complete&&result.bytes==OS_VRAM_SIZE&&beginCalls==1&&endCalls==1&&!queueOwned);
 const auto bytes=Read("vram/1F000000.bin");
 assert(bytes.size()==OS_VRAM_SIZE&&std::memcmp(bytes.data(),vram.data(),bytes.size())==0);
 assert(Read("vram/manifest.txt").find("phase=before-diagnostic-progress")!=std::string::npos);
 assert(Read("vram/manifest.txt").find("status=COMPLETE")!=std::string::npos);
 std::filesystem::create_directory("unreadable");readable=false;
 const auto unmapped=WriteVramCapture("unreadable");
 assert(!unmapped.complete&&beginCalls==1&&!std::filesystem::exists("unreadable/vram/1F000000.bin"));
 assert(Read("unreadable/vram/manifest.txt").find("status=INCOMPLETE")!=std::string::npos);
 std::filesystem::create_directory("busy");readable=true;queueReady=false;
 const auto busy=WriteVramCapture("busy");
 assert(!busy.complete&&beginCalls==2&&endCalls==1&&!queueOwned);
 assert(!std::filesystem::exists("busy/vram/1F000000.bin"));
 unsigned calls=0;
 auto failed=mk64_3ds::WriteVramSnapshot("partial.bin",100000,[&](std::size_t, unsigned char* dst,std::size_t n){
   ++calls;std::memset(dst,0,n);return calls!=2;
 });
 assert(!failed.complete&&failed.bytes==32768&&!std::filesystem::exists("partial.bin"));
 auto missing=mk64_3ds::WriteVramSnapshot("missing-parent/a.bin",4,[](auto,auto,auto){assert(false);return true;});
 assert(!missing.complete&&missing.bytes==0);
 const unsigned char known[]="123456789";
 assert(mk64_3ds::DumpCrc32(0,known,9)==0xcbf43926U);
 for(auto request:{Mk64Diagnostics3DSRequestDump,Mk64Diagnostics3DSRequestFullDump,Mk64Diagnostics3DSRequestBenchmarkDump}){
  autoActive=true;sDumpRequest=kDumpRequestNone;
  assert(!request()&&sDumpRequest==kDumpRequestNone);
  autoActive=false;assert(request());assert(!request());
  autoActive=true;const auto before=pauseCalls;
  assert(!Mk64Diagnostics3DSServiceDumpIfRequested()&&sDumpRequest==kDumpRequestNone&&pauseCalls==before);
  autoActive=false;assert(!Mk64Diagnostics3DSServiceDumpIfRequested());
  assert(request());assert(Mk64Diagnostics3DSServiceDumpIfRequested()&&pauseCalls==before+1);
 }
 std::printf("PASS: exact 6 MiB/CRC, queue ownership, pre-progress ordering, missing mappings/queue/copy/output failures and AUTO request/consumption gates\n");
}
'''
fixture = out / 'vram.cpp'
fixture.write_text(prefix + capture + '\n' + service + '\n' + '\n'.join(requests) + '\n' + suffix)
binary = out / 'vram-test'
subprocess.run(['clang++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-I', str(port / 'include'), str(fixture), '-o', str(binary)], check=True)
subprocess.run([str(binary)], cwd=out, check=True)
raw = (out / 'vram/1F000000.bin').read_bytes()
assert len(raw) == 6 * 1024 * 1024
assert f'crc32={zlib.crc32(raw):08X}' in (out / 'vram/manifest.txt').read_text()
