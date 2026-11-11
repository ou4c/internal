#pragma once
#include <string>

namespace RBX {
    class Instance;
}

class TeleportHandler {
public:
    static void Initialize();
    static void OnTeleport(RBX::Instance* teleportService);
    static void ExecuteAutoexec();

    static bool s_ClearOnTp;
};
