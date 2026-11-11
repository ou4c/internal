#include "script_receiver.h"
#include "roblox_encoder.h"
#include <iostream>
#include <algorithm>

ScriptReceiver& ScriptReceiver::Get() {
    static ScriptReceiver instance;
    return instance;
}

ScriptReceiver::~ScriptReceiver() {
    Stop();
}

bool ScriptReceiver::Start(int port) {
    if (m_Running) return true;

    m_Port = port;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_ListenSocket == INVALID_SOCKET)
        return false;

    int opt = 1;
    setsockopt(m_ListenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    addr.sin_port = htons((u_short)m_Port);

    if (bind(m_ListenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(m_ListenSocket);
        m_ListenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(m_ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_ListenSocket);
        m_ListenSocket = INVALID_SOCKET;
        return false;
    }

    m_Running = true;
    m_ServerThread = std::thread(&ScriptReceiver::ServerLoop, this);
    return true;
}

void ScriptReceiver::Stop() {
    if (m_Running) {
        m_Running = false;
        if (m_ListenSocket != INVALID_SOCKET) {
            closesocket(m_ListenSocket);
            m_ListenSocket = INVALID_SOCKET;
        }

        {
            std::lock_guard<std::mutex> lock(m_ClientsMutex);
            for (SOCKET s : m_Clients) {
                closesocket(s);
            }
            m_Clients.clear();
        }

        if (m_ServerThread.joinable())
            m_ServerThread.join();

        WSACleanup();
    }
}

void ScriptReceiver::BroadcastMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_ClientsMutex);
    for (SOCKET s : m_Clients) {
        // Send simple payload frame or raw json line
        std::string line = msg + "\n";
        send(s, line.data(), (int)line.size(), 0);
    }
}

void ScriptReceiver::ServerLoop() {
    while (m_Running) {
        sockaddr_in clientAddr;
        int clientLen = sizeof(clientAddr);
        SOCKET client = accept(m_ListenSocket, (sockaddr*)&clientAddr, &clientLen);
        if (client == INVALID_SOCKET) {
            if (!m_Running) break;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_ClientsMutex);
            m_Clients.push_back(client);
        }

        std::thread(&ScriptReceiver::HandleClient, this, client).detach();
    }
}

void ScriptReceiver::HandleClient(SOCKET client) {
    char buffer[4096];
    std::string accumulated;

    while (m_Running) {
        int bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = 0;
        accumulated.append(buffer, bytes);

        // Process newline-delimited or JSON chunks
        size_t pos;
        while ((pos = accumulated.find('\n')) != std::string::npos) {
            std::string line = accumulated.substr(0, pos);
            accumulated.erase(0, pos + 1);
            if (!line.empty() && line.front() == '{') {
                ProcessJsonMessage(line, client);
            }
        }
    }

    closesocket(client);
    {
        std::lock_guard<std::mutex> lock(m_ClientsMutex);
        auto it = std::find(m_Clients.begin(), m_Clients.end(), client);
        if (it != m_Clients.end())
            m_Clients.erase(it);
    }
}

void ScriptReceiver::ProcessJsonMessage(const std::string& jsonStr, SOCKET client) {
    // Quick parse for "type" and "script"
    // Reconstructs sub_1801F0390 json parsing
    size_t typePos = jsonStr.find("\"type\":");
    if (typePos == std::string::npos)
        typePos = jsonStr.find("\"type\" :");

    if (typePos != std::string::npos) {
        size_t firstQuote = jsonStr.find('\"', typePos + 7);
        size_t secondQuote = (firstQuote != std::string::npos) ? jsonStr.find('\"', firstQuote + 1) : std::string::npos;
        if (secondQuote != std::string::npos) {
            std::string type = jsonStr.substr(firstQuote + 1, secondQuote - firstQuote - 1);
            if (type == "execute") {
                size_t scriptPos = jsonStr.find("\"script\":");
                if (scriptPos == std::string::npos)
                    scriptPos = jsonStr.find("\"script\" :");
                if (scriptPos != std::string::npos) {
                    size_t sQuote1 = jsonStr.find('\"', scriptPos + 9);
                    size_t sQuote2 = (sQuote1 != std::string::npos) ? jsonStr.rfind('\"') : std::string::npos;
                    if (sQuote2 != std::string::npos && sQuote2 > sQuote1) {
                        std::string script = jsonStr.substr(sQuote1 + 1, sQuote2 - sQuote1 - 1);
                        // Unescape escaped quotes/newlines
                        std::string unescaped;
                        for (size_t i = 0; i < script.size(); i++) {
                            if (script[i] == '\\' && i + 1 < script.size()) {
                                char next = script[++i];
                                if (next == 'n') unescaped += '\n';
                                else if (next == 'r') unescaped += '\r';
                                else if (next == 't') unescaped += '\t';
                                else if (next == '\"') unescaped += '\"';
                                else if (next == '\\') unescaped += '\\';
                                else unescaped += next;
                            } else {
                                unescaped += script[i];
                            }
                        }
                        ExecuteScript(unescaped);
                    }
                }
            }
        }
    }
}

void ScriptReceiver::ExecuteScript(const std::string& script) {
    // Pass to compilation with RobloxEncoder and queue in TaskScheduler
    RobloxEncoder encoder;
    // Compiles bytecode and schedules into Roblox hybrid jobs
}
