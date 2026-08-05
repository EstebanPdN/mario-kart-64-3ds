#include "install_log_3ds.h"

#include <cstdarg>
#include <cstdio>

namespace {
constexpr const char* kInstallLogPath = "sdmc:/3ds/MK64/mk64-install.log";
FILE* gInstallLog = nullptr;
unsigned int gInstallLogLine = 0;
Mk64InstallLogCallback gInstallLogCallback = nullptr;
}

void Mk64InstallLogBegin() {
    if (gInstallLog != nullptr) {
        std::fclose(gInstallLog);
    }
    gInstallLog = std::fopen(kInstallLogPath, "wb");
    gInstallLogLine = 0;
    Mk64InstallLogWrite("Mario Kart 64 3DS installer log");
    Mk64InstallLogWrite("Only local installation diagnostics are written to this file.");
}

extern "C" void Mk64InstallLogWrite(const char* message) {
    if (message == nullptr) {
        return;
    }

    if (gInstallLog != nullptr) {
        std::fprintf(gInstallLog, "%04u %s\n", ++gInstallLogLine, message);
        std::fflush(gInstallLog);
    }
    if (gInstallLogCallback != nullptr) {
        gInstallLogCallback(message);
    }
}

extern "C" void Mk64InstallLogSetCallback(Mk64InstallLogCallback callback) {
    gInstallLogCallback = callback;
}

void Mk64InstallLogWritef(const char* format, ...) {
    if (format == nullptr) {
        return;
    }

    char message[512] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    Mk64InstallLogWrite(message);
}

void Mk64InstallLogClose() {
    if (gInstallLog != nullptr) {
        std::fflush(gInstallLog);
        std::fclose(gInstallLog);
        gInstallLog = nullptr;
    }
}
