#pragma once
#include <cstdint>
constexpr std::uint64_t SYSCLOCK_ARM11=268123480;
enum { GFX_TOP, GFX_BOTTOM };
std::uint64_t osGetTime();
std::uint64_t svcGetSystemTick();
void svcSleepThread(std::int64_t);
[[noreturn]] void svcExitProcess();
bool gspIsPresentPending(int);
