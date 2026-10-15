#include "file_replace_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>

// Model both rename contracts plus injected write/restore failures. Assertions
// are about surviving file contents, not private helper call counts.
struct Files {
    std::map<std::string,std::string> contents{{"temp","new settings"},{"config","old settings"}};
    std::set<std::pair<std::string,std::string>> failures;
    bool replaceExisting=false;
    bool Exists(const char* path) { return contents.count(path); }
    void Remove(const char* path) { contents.erase(path); }
    bool Rename(const char* from,const char* to) {
        if (!Exists(from) || failures.count({from,to}) || (!replaceExisting && Exists(to))) return false;
        auto value=contents.at(from);contents.erase(from);contents[to]=value;return true;
    }
};
int main() {
    for(bool posix:{false,true}) {
        Files f;f.replaceExisting=posix;
        assert(mk64_3ds::ReplacePreparedFile("temp","config","backup",f));
        assert(f.contents.at("config")=="new settings" && !f.Exists("temp") && !f.Exists("backup"));
    }
    Files failWrite;failWrite.failures.insert({"temp","config"});
    assert(!mk64_3ds::ReplacePreparedFile("temp","config","backup",failWrite));
    assert(failWrite.contents.at("config")=="old settings" && failWrite.contents.at("temp")=="new settings");
    Files failRestore;failRestore.failures={{"temp","config"},{"backup","config"}};
    assert(!mk64_3ds::ReplacePreparedFile("temp","config","backup",failRestore));
    assert(failRestore.contents.at("backup")=="old settings");
    Files stale;stale.contents["backup"]="recovery settings";
    assert(!mk64_3ds::ReplacePreparedFile("temp","config","backup",stale));
    assert(stale.contents.at("config")=="old settings" && stale.contents.at("backup")=="recovery settings");
    Files absent;absent.contents.erase("config");
    assert(mk64_3ds::ReplacePreparedFile("temp","config","backup",absent));
    assert(absent.contents.at("config")=="new settings");
    std::puts("PASS: POSIX/FAT replacement, failed replacement/restore, preserved old settings and existing recovery copies.");
}
