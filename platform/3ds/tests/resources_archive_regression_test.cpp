#include "o2r_archive_reader.hpp"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace mk64_3ds;
static O2rArchiveReader* active=nullptr;
static unsigned reentrantChecks=0;
static void Progress(unsigned) {
    assert(!active->MakeResident(SIZE_MAX));
    assert(!active->ReclaimPerformanceCache());
    assert(!active->DropResident());
    ++reentrantChecks;
}
static void CloseDuringProgress(unsigned) {active->Close();}
static void Check(const std::vector<uint8_t>& bytes,unsigned index) {
    assert(bytes.size()==208);
    for(unsigned i=0;i<bytes.size();++i)assert(bytes[i]==uint8_t(i*13+index*7));
}
int main(int argc,char** argv) {
    assert(argc==2);O2rArchiveReader archive(argv[1]);active=&archive;
    assert(archive.Open()==O2rReadResult::Ok && archive.Entries().size()==5000);
    archive.SetStreamingCacheBudget(2*1024*1024);
    std::vector<uint8_t> bytes;
    for(unsigned i=0;i<5000;++i){assert(archive.ReadEntryByIndex(i,&bytes)==O2rReadResult::Ok);Check(bytes,i);}
    const auto stats=archive.StreamingCacheStats();assert(stats.entries==5000 && stats.allocationBytes<=2*1024*1024);
    const auto before=archive.PhysicalReadBytes();assert(archive.ReadEntryByIndex(0,&bytes)==O2rReadResult::Ok);Check(bytes,0);
    assert(archive.PhysicalReadBytes()==before);
    const char* names[]={"__OTR__textures/karts/test/0000","missing",nullptr,"textures/karts/test/4999"};
    assert(archive.CacheEntries(names,4,2*1024*1024)==2);
    assert(!archive.MakeResident(1));assert(archive.ResidentResult()==O2rResidentResult::BudgetExceeded);
    assert(archive.MakeResident(archive.ResidentRequiredBytes(),Progress));
    assert(reentrantChecks>0 && archive.IsResident() && !archive.CachedBytes());
    const auto residentReads=archive.PhysicalReadBytes();
    for(unsigned i=0;i<5000;++i){assert(archive.ReadEntryByIndex(i,&bytes)==O2rReadResult::Ok);Check(bytes,i);}
    assert(archive.PhysicalReadBytes()==residentReads);
    assert(archive.DropResident() && !archive.IsResident());
    assert(archive.ReadEntryByIndex(0,&bytes)==O2rReadResult::Ok);Check(bytes,0);
    assert(archive.MakeResident(archive.ResidentRequiredBytes(),CloseDuringProgress));
    assert(!archive.IsOpen()); // Close is deferred until the residency mutation finishes.
    std::cout<<"PASS: 5000 real ZIP entries, byte/index bound, prefetched exact names, compressed-to-resident transition, CRC-preserving reads and reentrant progress/close.\n";
}
