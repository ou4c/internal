#pragma once
#include "common.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

class ScriptReceiver {
public:
    static ScriptReceiver& Get();

    bool Start(int port = 8342);
    void Stop();
    void ExecuteScript(const std::string& script);
    void BroadcastMessage(const std::string& msg);

private:
    ScriptReceiver() = default;
    ~ScriptReceiver();

    void ServerLoop();
    void HandleClient(SOCKET clientSocket);
    void ProcessJsonMessage(const std::string& jsonStr, SOCKET clientSocket);

    int m_Port = 8342;
    SOCKET m_ListenSocket = INVALID_SOCKET;
    std::atomic<bool> m_Running{false};
    std::thread m_ServerThread;

    std::mutex m_ClientsMutex;
    std::vector<SOCKET> m_Clients;
};
