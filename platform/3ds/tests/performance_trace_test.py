#!/usr/bin/env python3
"""Actual recorder: sampling, exclusive spans, owner counters and export failures."""
from pathlib import Path
import csv
import subprocess
import sys

platform = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
(out / '3ds.h').write_text(r'''
#pragma once
#include <cstddef>
#include <cstdint>
constexpr std::uint32_t SYSCLOCK_ARM11 = 268111856U;
std::uint64_t svcGetSystemTick();
void* getThreadLocalStorage();
std::size_t linearSpaceFree();
void* linearAlloc(std::size_t);
void linearFree(void*);
''')
runtime = (platform / 'source/performance_trace_3ds.cpp').read_text()
# Substitute only platform allocation/filesystem boundaries for deterministic
# fault injection; compile the complete production recorder and writer bodies.
for old, new in [('std::malloc(sizeof(TraceStorage))', 'HostMalloc(sizeof(TraceStorage))'),
                 ('std::fwrite(', 'HostWrite('), ('std::fclose(', 'HostClose(')]:
    assert old in runtime
    runtime = runtime.replace(old, new)
prefix = r'''
#include "performance_trace_3ds.hpp"
#include <3ds.h>
#include <cassert>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>
std::atomic<std::uint64_t> ticks{SYSCLOCK_ARM11}, clockCalls{0};
std::size_t available = 16 * 1024 * 1024;
bool linearFail=false, heapFail=false, shortWrite=false, closeFail=false, reenter=false;
std::size_t linearCharge=0, allocatorOverhead=0;
unsigned linearAttempts=0, heapAttempts=0, reentries=0;
std::uint64_t svcGetSystemTick(){++clockCalls;return ticks.load();}
void* getThreadLocalStorage(){static thread_local int identity;return &identity;}
std::size_t linearSpaceFree(){return available;}
void* linearAlloc(std::size_t size){
    ++linearAttempts;if(linearFail)return nullptr;
    void* result=std::malloc(size);if(result){linearCharge=size+allocatorOverhead;available-=linearCharge;}return result;
}
void linearFree(void* pointer){available+=linearCharge;linearCharge=0;std::free(pointer);}
void* HostMalloc(std::size_t size){++heapAttempts;return heapFail?nullptr:std::malloc(size);}
void Advance(unsigned us){ticks.fetch_add(std::uint64_t(us)*SYSCLOCK_ARM11/1000000);}
std::size_t HostWrite(const void*,std::size_t,std::size_t,FILE*);
int HostClose(FILE* file){const int result=std::fclose(file);if(closeFail){closeFail=false;return EOF;}return result;}
'''
suffix = r'''
std::size_t HostWrite(const void* data,std::size_t size,std::size_t count,FILE* file){
    if(reenter){
        reenter=false;++reentries;
        const auto countBefore=sStorage->history.Size();
        const auto epoch=sEpoch;
        mk64_3ds::PerformanceBegin();mk64_3ds::PerformanceEnd();
        mk64_3ds::PerformanceResume();mk64_3ds::PerformanceCourse(999,999);
        mk64_3ds::PerformanceRaceState(false,0,0);
        mk64_3ds::PerformanceStateAfter(0,0,0);
        assert(Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_PHYSICS)==0);
        mk64_3ds::PerformanceShutdown();
        assert(!mk64_3ds::PerformanceWrite("recursive"));
        assert(countBefore==sStorage->history.Size() && epoch==sEpoch);
        Mk64Perf3DSResourceRead(mk64_3ds::PerformanceNow(),7);
    }
    if(shortWrite){shortWrite=false;return count?count-1:0;}
    return std::fwrite(data,size,count,file);
}
std::string Read(const char* path){std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Near(unsigned actual,unsigned expected){assert(actual<=expected+2 && actual+2>=expected);}
void BeginRace(){
    mk64_3ds::PerformanceBegin();
    auto& p=mk64_3ds::PerformanceCurrent();p.game_state=4;p.presents=1;
    mk64_3ds::PerformanceCourse(7,3);mk64_3ds::PerformanceRaceState(true,3,15000);
    mk64_3ds::PerformanceStateAfter(4,3,15033);
}
int main(){
    using namespace mk64_3ds;
    assert(PerformanceInitialize());assert(std::string(PerformanceStorageKind())=="linear");
    const auto storageBytes=PerformanceStorageBytes();assert(storageBytes==sizeof(TraceStorage));
    const auto reservedBytes=PerformanceStorageReservedBytes();
    assert(reservedBytes%128==0&&reservedBytes>=storageBytes&&reservedBytes-storageBytes<128);
    const auto allocations=linearAttempts+heapAttempts;
    const unsigned period=MK64_3DS_PERF_DETAIL_PERIOD;
    for(unsigned i=0;i<40;++i){
        BeginRace();auto& p=PerformanceCurrent();
        const bool sampled=period && (p.tick-1)%(period?period:1)==0;
        assert(PerformanceDetailsEnabled()==sampled && p.detail_sampled==sampled);
        auto calls=clockCalls.load();
        {PerformanceTimer timer(p.matrix_us);Advance(40);}
        assert(clockCalls-calls==(sampled?2:0));Near(p.matrix_us,sampled?40:0);
        calls=clockCalls.load();{PerformanceTimer timer(p.iteration_us);Advance(70);}
        assert(clockCalls-calls==2);Near(p.iteration_us,70);
        p.matrix_calls=19;p.lookups=13;p.sp_vertices=7;p.sp_tri_calls=6;
        p.resource_bytes=999; // Cumulative publication replaces this field.
        PerformanceEnd();const auto& row=sStorage->history.At(i);
        assert(row.matrix_calls==(sampled?19:0));assert(row.lookups==(sampled?13:0));
        assert(row.detail_period==period && row.course_generation==7 && row.course_id==3);
        assert(row.total_us>=108 && row.resource_bytes==0 && row.transition==0);
    }
    assert(linearAttempts+heapAttempts==allocations); // No frame allocation.
    BeginRace();
    auto physics=Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_PHYSICS);Advance(10);
    auto collision=Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_COLLISION);Advance(20);
    auto recursive=Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_COLLISION);Advance(30);
    Mk64Perf3DSGameSectionEnd(recursive);Advance(40);Mk64Perf3DSGameSectionEnd(collision);
    Advance(50);Mk64Perf3DSGameSectionEnd(physics);
    {PerformanceGameTimer visual(MK64_PERF_GAME_VISUAL);Advance(60);}
    Near(PerformanceCurrent().physics_us,60);Near(PerformanceCurrent().collision_us,90);
    Near(PerformanceCurrent().visual_us,60);assert(PerformanceCurrent().leaf_faults==0);
    PerformanceEnd();
    // Worker counters publish independently of the row; local path preserves totals.
    BeginRace();std::vector<std::thread> workers;
    for(unsigned w=0;w<4;++w)workers.emplace_back([](){
        for(unsigned n=0;n<1000;++n){auto start=mk64_3ds::PerformanceNow();
            Mk64Perf3DSResourceRead(start,9);mk64_3ds::PerformanceArchiveRead(start,1,16);}
    });
    for(unsigned n=0;n<1000;++n){auto start=PerformanceNow();Mk64Perf3DSResourceRead(start,11);}
    for(auto& w:workers)w.join();PerformanceEnd();
    assert(PerformanceCurrent().resource_reads==5000 && PerformanceCurrent().resource_bytes==47000);
    assert(PerformanceCurrent().archive_io_calls==4000 && PerformanceCurrent().archive_io_bytes==64000);
    // Resume discards incomplete work and starts a zero-interval epoch.
    const auto count=sStorage->history.Size();BeginRace();
    auto stale=Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_PLAYERS);
    const auto pauseStart=PerformanceNow();Advance(5000000);
    PerformanceResume(PerformancePause::LabExport,pauseStart);PerformanceEnd();
    assert(sStorage->history.Size()==count);BeginRace();
    assert(PerformanceCurrent().interval_us==0 && PerformanceCurrent().epoch==1);
    assert(PerformanceCurrent().resume_reason==2);Near(PerformanceCurrent().excluded_us,5000000);
    Mk64Perf3DSGameSectionEnd(stale);assert(PerformanceCurrent().leaf_faults==1);
    PerformanceEnd();
    // Unbalanced, invalid and overdeep spans are bounded and explicitly flagged.
    BeginRace();assert(Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_SECTION_COUNT)==0);
    std::vector<unsigned> tokens;
    for(unsigned i=0;i<32;++i)tokens.push_back(Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_ACTORS));
    assert(Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_ACTORS)==0);
    Mk64Perf3DSGameSectionEnd(tokens.front());assert(PerformanceCurrent().leaf_faults==3);
    Mk64Perf3DSGameSectionBegin(MK64_PERF_GAME_CAMERA);PerformanceEnd();assert(PerformanceCurrent().leaf_faults==4);
    BeginRace();const auto summaryTicks=sRaceSummary.ticks;
    PerformanceStateAfter(1,0,0);PerformanceEnd();assert(sRaceSummary.ticks==summaryTicks);
    // Fill and wrap both windows, retaining all capacities even on fallback.
    for(unsigned i=0;i<1300;++i){BeginRace();Advance(100);PerformanceEnd();}
    assert(sStorage->raceStart.Size()==256);
#ifdef MK64_3DS_AUTODUMP_LAB
    assert(sStorage->history.Size()==1024);
#else
    assert(sStorage->history.Size()==256);
#endif
    std::vector<unsigned char> snapshot(sizeof(TraceStorage));std::memcpy(snapshot.data(),sStorage,snapshot.size());
    const auto summary=sRaceSummary;reenter=true;
    assert(PerformanceWrite("ok"));assert(reentries==1);
    assert(std::memcmp(snapshot.data(),sStorage,snapshot.size())==0);
    assert(std::memcmp(&summary,&sRaceSummary,sizeof(summary))==0);
    const auto csv=Read("ok/performance.csv");assert(!csv.empty());
    shortWrite=true;assert(!PerformanceWrite("ok"));assert(Read("ok/performance.csv")==csv);
    closeFail=true;assert(!PerformanceWrite("ok"));assert(Read("ok/performance.csv")==csv);
    assert(!PerformanceWrite("missing/directory"));assert(!PerformanceWrite(nullptr));
    assert(!PerformanceWrite(std::string(300,'x').c_str()));
    assert(!PerformanceWrite("rename-failure"));
    PerformanceHistory<1> wide;PerformanceTick wideRow;
    wideRow.start_us=UINT64_MAX;
#define MAX_FIELD(name) wideRow.name=UINT32_MAX;
    MK64_PERF_FIELDS(MAX_FIELD)
#undef MAX_FIELD
    wide.Push(wideRow);assert(WriteCsv("wide.csv",wide));
    PerformanceShutdown();
    available=reservedBytes+kPerformanceLinearReserve-1;
    const auto attempts=linearAttempts;assert(PerformanceInitialize());
    assert(linearAttempts==attempts && std::string(PerformanceStorageKind())=="ordinary-fallback");
    assert(PerformanceStorageBytes()==storageBytes);PerformanceShutdown();
    available=reservedBytes+kPerformanceLinearReserve;linearFail=true;assert(PerformanceInitialize());
    assert(linearAttempts==attempts+1 && std::string(PerformanceStorageKind())=="ordinary-fallback");PerformanceShutdown();
    heapFail=true;assert(!PerformanceInitialize());assert(PerformanceStorageBytes()==0);
    PerformanceBegin();PerformanceEnd();assert(!PerformanceWrite("ok"));
    linearFail=false;heapFail=false;allocatorOverhead=64;
    assert(PerformanceInitialize());assert(std::string(PerformanceStorageKind())=="ordinary-fallback");
    assert(available==reservedBytes+kPerformanceLinearReserve);PerformanceShutdown();
    allocatorOverhead=0;assert(PerformanceInitialize());assert(std::string(PerformanceStorageKind())=="linear");
    assert(available==kPerformanceLinearReserve);
    PerformanceShutdown();
    std::printf("PASS: actual recorder period=%u storage=%zu bytes; sampling, nested exclusive spans, threads, epochs, immutable buffered export, write/close/rename/allocation failures.\n",period,storageBytes);
}
'''
(out / 'recorder.cpp').write_text(prefix + runtime + suffix)
for lab, period in [(True, 16), (True, 0), (True, 1), (False, 16)]:
    variant = f'{"lab" if lab else "normal"}-{period}'
    binary = out / variant
    command = ['clang++', '-std=c++17', '-O1', '-g', '-Wall', '-Wextra',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               f'-DMK64_3DS_PERF_DETAIL_PERIOD={period}', '-pthread',
               '-I' + str(out), '-I' + str(platform / 'include')]
    if lab:
        command += ['-DMK64_3DS_AUTODUMP_LAB=1']
    subprocess.run(command + [str(out / 'recorder.cpp'), '-o', str(binary)], check=True)
    cwd = out / (variant + '-files')
    (cwd / 'ok').mkdir(parents=True, exist_ok=True)
    (cwd / 'rename-failure/performance.csv').mkdir(parents=True, exist_ok=True)
    subprocess.run([str(binary)], cwd=cwd, check=True)
    with (cwd / 'ok/performance.csv').open() as file:
        rows = list(csv.DictReader(file))
    assert len(rows) == (1024 if lab else 256)
    assert all(None not in row and None not in row.values() for row in rows)
    assert all(int(row['detail_period']) == period for row in rows)
    assert all(int(row['art_io_bytes']) == 0 for row in rows)
    assert rows[0]['tick'] != rows[-1]['tick']
    with (cwd / 'wide.csv').open() as file:
        wide = list(csv.DictReader(file))[0]
    assert int(wide.pop('start_us')) == 2**64-1
    assert all(int(value) == 2**32-1 for value in wide.values())
print('PASS: CSV shape/retained capacities and normal-build artwork fields verified for all four configurations.')
