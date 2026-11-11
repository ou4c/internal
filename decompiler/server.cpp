#include "server.h"
#include "decompiler.h"
#include <thread>

#pragma comment(lib, "ws2_32.lib")

static SOCKET s_Server = INVALID_SOCKET;
static bool s_Running = false;

void DecompilerServer::ClientHandler(SOCKET client) {
    std::string received;
    char buffer[4096];

    while (s_Running) {
        int r = recv(client, buffer, sizeof(buffer), 0);
        if (r <= 0) break;
        received.append(buffer, r);

        // Check if full buffer received
        if (received.find("\n__DECOMPILE_EOF__\n") != std::string::npos) {
            size_t pos = received.find("\n__DECOMPILE_EOF__\n");
            std::string bc = received.substr(0, pos);
            std::string decompiled = LuauDecompiler::DecompileBytecode(bc);
            decompiled += "\n__DECOMPILE_RES_EOF__\n";
            send(client, decompiled.data(), (int)decompiled.size(), 0);
            break;
        }
    }
    closesocket(client);
}

bool DecompilerServer::Start(int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    s_Server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_Server == INVALID_SOCKET)
        return false;

    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((u_short)port);

    if (bind(s_Server, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s_Server);
        return false;
    }

    if (listen(s_Server, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(s_Server);
        return false;
    }

    s_Running = true;
    std::cout << "[Decompiler] Listening on 127.0.0.1:" << port << "...\n";

    while (s_Running) {
        SOCKET client = accept(s_Server, NULL, NULL);
        if (client == INVALID_SOCKET) {
            if (!s_Running) break;
            continue;
        }
        std::thread(ClientHandler, client).detach();
    }

    return true;
}

void DecompilerServer::Stop() {
    s_Running = false;
    if (s_Server != INVALID_SOCKET) {
        closesocket(s_Server);
        s_Server = INVALID_SOCKET;
    }
    WSACleanup();
}
