#include "control_socket.h"
#include <iostream>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define IS_INVALID_SOCKET(s) ((s) < 0)
#define CLOSE_SOCKET(s) close(s)
#endif

namespace voip {

ControlSocketServer::ControlSocketServer(uint16_t port)
    : m_port(port)
{
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

ControlSocketServer::~ControlSocketServer() {
    stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

void ControlSocketServer::onMessage(MessageCallback callback) {
    m_callback = std::move(callback);
}

bool ControlSocketServer::start() {
    if (m_running) return true;
    m_running = true;
    m_listenThread = std::thread(&ControlSocketServer::listenLoop, this);
    return true;
}

void ControlSocketServer::stop() {
    if (!m_running) return;
    m_running = false;

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (auto sock : m_clientSockets) {
            CLOSE_SOCKET(static_cast<socket_t>(sock));
        }
        m_clientSockets.clear();
    }

    if (m_listenThread.joinable()) {
        m_listenThread.detach(); // or join if unblocked
    }
}

void ControlSocketServer::broadcast(const std::string& jsonMessage) {
    uint32_t len = static_cast<uint32_t>(jsonMessage.size());
    uint32_t netLen = htonl(len);

    std::vector<uint8_t> buffer(4 + len);
    std::memcpy(buffer.data(), &netLen, 4);
    std::memcpy(buffer.data() + 4, jsonMessage.data(), len);

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    std::vector<intptr_t> active;
    for (auto sock : m_clientSockets) {
        int sent = send(static_cast<socket_t>(sock), reinterpret_cast<const char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
        if (sent > 0) {
            active.push_back(sock);
        } else {
            CLOSE_SOCKET(static_cast<socket_t>(sock));
        }
    }
    m_clientSockets = std::move(active);
}

void ControlSocketServer::listenLoop() {
    socket_t serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (IS_INVALID_SOCKET(serverSock)) {
        std::cerr << "[ControlSocketServer] Failed to create server socket." << std::endl;
        m_running = false;
        return;
    }

    int opt = 1;
#if defined(_WIN32)
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_port);

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cerr << "[ControlSocketServer] Bind failed on port " << m_port << std::endl;
        CLOSE_SOCKET(serverSock);
        m_running = false;
        return;
    }

    if (listen(serverSock, 5) != 0) {
        std::cerr << "[ControlSocketServer] Listen failed." << std::endl;
        CLOSE_SOCKET(serverSock);
        m_running = false;
        return;
    }

    std::cout << "[ControlSocketServer] Listening for IPC control on 127.0.0.1:" << m_port << std::endl;

    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        socket_t clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientLen);
        if (IS_INVALID_SOCKET(clientSock)) {
            if (!m_running) break;
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            m_clientSockets.push_back(static_cast<intptr_t>(clientSock));
        }

        std::thread([this, clientSock]() {
            this->clientWorker(static_cast<intptr_t>(clientSock));
        }).detach();
    }

    CLOSE_SOCKET(serverSock);
}

void ControlSocketServer::clientWorker(intptr_t clientSockVal) {
    socket_t sock = static_cast<socket_t>(clientSockVal);

    while (m_running) {
        uint32_t netLen = 0;
        int received = recv(sock, reinterpret_cast<char*>(&netLen), 4, MSG_WAITALL);
        if (received <= 0) {
            break;
        }

        uint32_t len = ntohl(netLen);
        if (len > 10 * 1024 * 1024) { // 10MB limit safety
            break;
        }

        std::string payload(len, '\0');
        received = recv(sock, &payload[0], len, MSG_WAITALL);
        if (received <= 0) {
            break;
        }

        if (m_callback) {
            m_callback(payload);
        }
    }

    CLOSE_SOCKET(sock);
}

} // namespace voip
