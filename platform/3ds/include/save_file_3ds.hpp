#pragma once
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

namespace mk64_3ds {
// 3DS FS rename does not replace an existing destination. Retain a backup
// across the two renames so startup can recover an interrupted replacement.
inline bool RecoverSaveBackup(const char* path) {
    struct stat info;
    if (stat(path, &info) == 0) return true;
    if (errno != ENOENT) return false;
    char backup[256];
    const int n=std::snprintf(backup,sizeof(backup),"%s.bak",path);
    if(n<0 || static_cast<size_t>(n)>=sizeof(backup)) return false;
    if(stat(backup,&info)!=0) return errno==ENOENT;
    return std::rename(backup,path)==0;
}

inline bool FinishSaveWrite(FILE* file,const char* temporary,const char* path,bool ok) {
    if(!file) return false;
    if(ok && std::fflush(file)!=0) ok=false;
    if(ok && fsync(fileno(file))!=0) ok=false;
    if(std::fclose(file)!=0) ok=false;
    if(!ok) { std::remove(temporary);return false; }
    if(!RecoverSaveBackup(path)) { std::remove(temporary);return false; }
    char backup[256];
    const int n=std::snprintf(backup,sizeof(backup),"%s.bak",path);
    if(n<0 || static_cast<size_t>(n)>=sizeof(backup)) {std::remove(temporary);return false;}
    struct stat info;
    const bool existed=stat(path,&info)==0;
    if(!existed && errno!=ENOENT) {std::remove(temporary);return false;}
    if(existed) {
        if((std::remove(backup)!=0 && errno!=ENOENT) || std::rename(path,backup)!=0) {
            std::remove(temporary);return false;
        }
    }
    if(std::rename(temporary,path)!=0) {
        if(existed) std::rename(backup,path);
        std::remove(temporary);return false;
    }
    if(existed) std::remove(backup);
    return true;
}
}
