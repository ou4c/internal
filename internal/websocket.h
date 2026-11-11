#pragma once
#include "common.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

class WrappedWebSocket {
public:
    using MessageCallback = std::function<void(const std::string& message)>;
    using CloseCallback = std::function<void(int code, const std::string& reason)>;

    WrappedWebSocket();
    ~WrappedWebSocket();

    bool Connect(const std::string& url);
    void Send(const std::string& message);
    void Close(int code = 1000, const std::string& reason = "");

    void SetOnMessage(MessageCallback cb) { m_OnMessage = cb; }
    void SetOnClose(CloseCallback cb) { m_OnClose = cb; }

    bool IsConnected() const { return m_Connected; }

private:
    void WorkerLoop();

    SOCKET m_Socket = INVALID_SOCKET;
    std::string m_Url;
    std::string m_Host;
    int m_Port = 80;
    std::string m_Path = "/";
    std::atomic<bool> m_Connected{false};
    std::atomic<bool> m_Running{false};
    std::thread m_WorkerThread;

    MessageCallback m_OnMessage;
    CloseCallback m_OnClose;
};
