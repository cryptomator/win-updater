#include "updater.h"

#include <windows.h>

#include <cstdlib>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // first thing: load further DLLs only from System32; the static imports are covered by the linker flag /DEPENDENTLOADFLAG:0x800
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    return static_cast<int>(Run(__argc, __wargv));
}
