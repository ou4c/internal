#include "teleport.h"
#include "filesystem_api.h"
#include "script_receiver.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool TeleportHandler::s_ClearOnTp = true;

void TeleportHandler::Initialize() {
    ExecuteAutoexec();
}

void TeleportHandler::OnTeleport(RBX::Instance* teleportService) {
    if (s_ClearOnTp) {
        // Send clearConsole message to UI via ScriptReceiver
        ScriptReceiver::Get().BroadcastMessage("{\"type\":\"clearConsole\"}");
    }
    // Re-execute autoexec on place change
    ExecuteAutoexec();
}

void TeleportHandler::ExecuteAutoexec() {
    fs::path autoexecDir = FilesystemAPI::GetAutoexecPath();
    if (autoexecDir.empty() || !fs::is_directory(autoexecDir))
        return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(autoexecDir, ec)) {
        if (entry.is_regular_file()) {
            std::ifstream file(entry.path(), std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string script = buffer.str();
                if (!script.empty()) {
                    ScriptReceiver::Get().ExecuteScript(script);
                }
            }
        }
    }
}
