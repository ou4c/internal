#pragma once
#include "common.h"

class DecompilerServer {
public:
    static bool Start(int port = 8343);
    static void Stop();

private:
    static void ClientHandler(SOCKET clientSocket);
};
