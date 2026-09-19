#include "o2r_archive_reader.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <vector>
// Mimic a heap which has ample total memory but no allocation above 256 KiB.
static bool constrained = false;
static size_t allocations = 0, failAt = SIZE_MAX, largest = 0;
void* operator new[](size_t size) {
    if (constrained) {
        ++allocations; largest = std::max(largest, size);
        if (size > 256u*1024u || allocations == failAt) throw std::bad_alloc();
    }
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t n, const std::nothrow_t&) noexcept {
    try { return ::operator new[](n); } catch (...) { return nullptr; }
}
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, size_t) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
int main(int argc, char** argv) {
    using namespace mk64_3ds;
    assert(argc == 2);
    O2rArchiveReader archive(argv[1]); assert(archive.Open() == O2rReadResult::Ok);
    assert(!archive.MakeResident(1024));
    assert(archive.ResidentResult() == O2rResidentResult::BudgetExceeded);
    const size_t required = archive.ResidentRequiredBytes(); assert(required > 1024);
    constrained = true; failAt = 10;
    assert(!archive.MakeResident(required)); assert(!archive.IsResident());
    assert(archive.ResidentResult() == O2rResidentResult::AllocationFailed);
    std::vector<uint8_t> bytes;
    assert(archive.ReadEntryByIndex(0, &bytes) == O2rReadResult::Ok);
    allocations = 0; failAt = SIZE_MAX;
    assert(archive.MakeResident(required)); assert(archive.IsResident());
    assert(archive.ResidentResult() == O2rResidentResult::Ready);
    assert(allocations > 20 && largest <= 256u*1024u);
    std::cout << "PASS: fragmented heap, " << allocations << " bounded allocations, max=" << largest
              << "; partial-allocation rollback, exact budget and SD fallback reads.\n";
    constrained = false;
    const auto path = std::filesystem::temp_directory_path()/"mk64-e6-short-read.o2r";
    assert(!std::filesystem::exists(path)); std::filesystem::copy_file(argv[1], path);
    O2rArchiveReader broken(path.string()); assert(broken.Open() == O2rReadResult::Ok);
    std::ofstream(path, std::ios::trunc).close();
    assert(!broken.MakeResident(required)); assert(!broken.IsResident());
    assert(broken.ResidentResult() == O2rResidentResult::ReadFailed);
    std::filesystem::remove(path);
    std::cout << "PASS: archive read failure distinguished from memory failure.\n";
}
