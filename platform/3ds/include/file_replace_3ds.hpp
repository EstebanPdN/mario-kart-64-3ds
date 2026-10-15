#pragma once

namespace mk64_3ds {
// Replace an already flushed/closed temporary file. POSIX can replace directly;
// FAT may refuse an existing destination. Keep the previous complete file in a
// recovery path until the second rename succeeds. Never overwrite an existing
// recovery file on the fallback path. Ops is an effect seam for fault tests.
// False leaves either destination or recovery available; the caller owns temp.
// This is recoverable replacement, not a claim of power-loss atomicity on FAT.
template<class Ops>
bool ReplacePreparedFile(const char* temp,const char* destination,const char* recovery,Ops& ops) {
    if (ops.Rename(temp,destination)) {
        ops.Remove(recovery);
        return true;
    }
    if (ops.Exists(recovery) || !ops.Rename(destination,recovery)) return false;
    if (ops.Rename(temp,destination)) {
        ops.Remove(recovery);
        return true;
    }
    // If restoration also fails, retain the recovery copy for the next load.
    ops.Rename(recovery,destination);
    return false;
}
} // namespace mk64_3ds
