#define UNICODE
#define _UNICODE
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR args, int show) {
    wchar_t root[32768], python[32768], command[65536], plugins[32768];
    if (!GetModuleFileNameW(NULL, root, 32768)) return 1;
    wchar_t* slash = wcsrchr(root, L'\\');
    if (!slash) return 1;
    *slash = 0;
    _snwprintf(python, 32768, L"%ls\\python\\pythonw.exe", root);
    _snwprintf(command, 65536, L"\"%ls\" -E -s \"%ls\\app\\app.py\"", python, root);
    _snwprintf(plugins, 32768, L"%ls\\python\\Lib\\site-packages\\PySide6\\plugins", root);
    SetEnvironmentVariableW(L"QT_PLUGIN_PATH", plugins);
    STARTUPINFOW startup = {0}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {0};
    if (!CreateProcessW(python, command, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, root, &startup, &process)) {
        MessageBoxW(NULL, L"The bundled application could not start. Extract the complete ZIP before opening MK64 Extractor.exe.",
                    L"MK64 Extractor", MB_OK | MB_ICONERROR);
        return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
