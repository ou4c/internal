#include "websocket.h"
#include <iostream>
#include <vector>

WrappedWebSocket::WrappedWebSocket() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

WrappedWebSocket::~WrappedWebSocket() {
    Close();
}

bool WrappedWebSocket::Connect(const std::string& url) {
    m_Url = url;
    // Parse ws://host:port/path
    std::string prefix = "ws://";
    std::string u = url;
    if (u.find(prefix) == 0)
        u = u.substr(prefix.length());

    size_t slashPos = u.find('/');
    std::string hostPort = (slashPos != std::string::npos) ? u.substr(0, slashPos) : u;
    m_Path = (slashPos != std::string::npos) ? u.substr(slashPos) : "/";

    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        m_Host = hostPort.substr(0, colonPos);
        m_Port = std::stoi(hostPort.substr(colonPos + 1));
    } else {
        m_Host = hostPort;
        m_Port = 80;
    }

    struct addrinfo hints = {0}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(m_Host.c_str(), std::to_string(m_Port).c_str(), &hints, &result) != 0)
        return false;

    m_Socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (m_Socket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }

    if (connect(m_Socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
        freeaddrinfo(result);
        return false;
    }
    freeaddrinfo(result);

    // Simple WebSocket HTTP handshake
    std::string handshake =
        "GET " + m_Path + " HTTP/1.1\r\n"
        "Host: " + m_Host + "\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";

    send(m_Socket, handshake.c_str(), (int)handshake.length(), 0);

    char buf[1024];
    int bytesRecv = recv(m_Socket, buf, sizeof(buf) - 1, 0);
    if (bytesRecv <= 0) {
        closesocket(m_Socket);
        m_Socket = INVALID_SOCKET;
        return false;
    }

    m_Connected = true;
    m_Running = true;
    m_WorkerThread = std::thread(&WrappedWebSocket::WorkerLoop, this);
    return true;
}

void WrappedWebSocket::Send(const std::string& message) {
    if (!m_Connected || m_Socket == INVALID_SOCKET)
        return;

    // Build unmasked or masked text frame
    std::vector<uint8_t> frame;
    frame.push_back(0x81); // FIN + opcode 1 (text)

    size_t len = message.length();
    if (len < 126) {
        frame.push_back(0x80 | (uint8_t)len); // Masked
    } else if (len <= 0xFFFF) {
        frame.push_back(0x80 | 126);
        frame.push_back((len >> 8) & 0xFF);
        frame.push_back(len & 0xFF);
    } else {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; i--)
            frame.push_back((len >> (i * 8)) & 0xFF);
    }

    uint8_t mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    frame.push_back(mask[0]);
    frame.push_back(mask[1]);
    frame.push_back(mask[2]);
    frame.push_back(mask[3]);

    for (size_t i = 0; i < len; i++) {
        frame.push_back(message[i] ^ mask[i % 4]);
    }

    send(m_Socket, (const char*)frame.data(), (int)frame.size(), 0);
}

void WrappedWebSocket::Close(int code, const std::string& reason) {
    if (m_Running) {
        m_Running = false;
        m_Connected = false;
        if (m_Socket != INVALID_SOCKET) {
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
        }
        if (m_WorkerThread.joinable())
            m_WorkerThread.join();

        if (m_OnClose)
            m_OnClose(code, reason);
    }
}

void WrappedWebSocket::WorkerLoop() {
    uint8_t header[2];
    while (m_Running) {
        int r = recv(m_Socket, (char*)header, 2, 0);
        if (r <= 0) break;

        uint8_t opcode = header[0] & 0x0F;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7F;

        if (payloadLen == 126) {
            uint16_t extLen = 0;
            recv(m_Socket, (char*)&extLen, 2, 0);
            payloadLen = ntohs(extLen);
        } else if (payloadLen == 127) {
            uint64_t extLen = 0;
            recv(m_Socket, (char*)&extLen, 8, 0);
            // big-endian to host
            payloadLen = _byteswap_uint64(extLen);
        }

        uint8_t maskKey[4] = {0};
        if (masked) {
            recv(m_Socket, (char*)maskKey, 4, 0);
        }

        std::string payload;
        payload.resize(payloadLen);
        size_t totalReceived = 0;
        while (totalReceived < payloadLen) {
            int chunk = recv(m_Socket, &payload[totalReceived], (int)(payloadLen - totalReceived), 0);
            if (chunk <= 0) break;
            totalReceived += chunk;
        }

        if (masked) {
            for (size_t i = 0; i < payloadLen; i++)
                payload[i] ^= maskKey[i % 4];
        }

        if (opcode == 1) { // Text
            if (m_OnMessage)
                m_OnMessage(payload);
        } else if (opcode == 8) { // Close
            break;
        }
    }
    m_Connected = false;
}
