#include "common.h"
#include <process.h>
#include "filesystem_api.h"
#include "script_receiver.h"
#include "teleport.h"

// Initialization thread reconstructed from sub_1801BE3F0 and sub_1801F1A00
unsigned int __stdcall MainThread(void* param) {
    // 1. Initialize filesystem folders in %LOCALAPPDATA%\Potassium
    FilesystemAPI::Initialize();

    // 2. Start WebSocket script receiver server on localhost:8342
    ScriptReceiver::Get().Start(8342);

    // 3. Execute autoexec scripts
    TeleportHandler::Initialize();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        _beginthreadex(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        ScriptReceiver::Get().Stop();
        break;
    }
    return TRUE;
}
