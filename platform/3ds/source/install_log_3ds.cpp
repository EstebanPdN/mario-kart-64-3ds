#include "install_log_3ds.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
constexpr const char* kInstallLogPath = "sdmc:/3ds/MK64/mk64-install.log";
FILE* gInstallLog = nullptr;
unsigned int gInstallLogLine = 0;
Mk64InstallLogCallback gInstallLogCallback = nullptr;

void WriteInstallLogLine(const char* message, size_t length) {
    if (gInstallLog != nullptr) {
        std::fprintf(gInstallLog, "%04u ", ++gInstallLogLine);
        if (length != 0) {
            std::fwrite(message, 1, length, gInstallLog);
        }
        std::fputc('\n', gInstallLog);
        std::fflush(gInstallLog);
    }

    if (gInstallLogCallback != nullptr) {
        // The installation console keeps a much shorter visible line, but it
        // still needs a terminated string when a diagnostic contains a line
        // break. The complete, untruncated line remains in the SD-card log.
        char callbackLine[512] = {};
        const size_t callbackLength = length < sizeof(callbackLine) - 1 ? length : sizeof(callbackLine) - 1;
        if (callbackLength != 0) {
            std::memcpy(callbackLine, message, callbackLength);
        }
        gInstallLogCallback(callbackLine);
    }
}
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

    // Prefix every physical line. Besides keeping the file machine-readable,
    // this preserves every line of multi-line Torch errors instead of writing
    // an unnumbered tail that can be mistaken for a truncated log.
    const char* line = message;
    while (true) {
        const char* newline = std::strchr(line, '\n');
        if (newline == nullptr) {
            WriteInstallLogLine(line, std::strlen(line));
            break;
        }
        WriteInstallLogLine(line, static_cast<size_t>(newline - line));
        line = newline + 1;
        if (*line == '\0') {
            break;
        }
    }
}

extern "C" void Mk64InstallLogSetCallback(Mk64InstallLogCallback callback) {
    gInstallLogCallback = callback;
}

extern "C" void Mk64InstallLogWritef(const char* format, ...) {
    if (format == nullptr) {
        return;
    }

    char message[1024] = {};
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
