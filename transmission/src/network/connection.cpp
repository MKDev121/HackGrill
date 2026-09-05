#include "connection.h"
#include <iostream>
#include <cstring>

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
#include <poll.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define IS_INVALID_SOCKET(s) ((s) < 0)
#define CLOSE_SOCKET(s) close(s)
#endif

namespace voip {

NetworkConnection::NetworkConnection(const SessionConfig& config)
    : m_config(config)
{
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    m_remoteSockAddr = new sockaddr_in();
}

NetworkConnection::~NetworkConnection() {
    closeConnection();
    if (m_remoteSockAddr) {
        delete reinterpret_cast<sockaddr_in*>(m_remoteSockAddr);
        m_remoteSockAddr = nullptr;
    }
#if defined(_WIN32)
    WSACleanup();
#endif
}

bool NetworkConnection::initialize() {
    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (IS_INVALID_SOCKET(sock)) {
        std::cerr << "[NetworkConnection] Failed to create UDP socket." << std::endl;
        return false;
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(m_config.localPort);

    if (bind(sock, (sockaddr*)&localAddr, sizeof(localAddr)) != 0) {
        std::cerr << "[NetworkConnection] UDP bind failed on port " << m_config.localPort << std::endl;
        CLOSE_SOCKET(sock);
        return false;
    }

    m_socketHandle = static_cast<intptr_t>(sock);

    updateRemote(m_config.remoteHost, m_config.remotePort);
    m_initialized = true;

    std::cout << "[NetworkConnection] Bound to local UDP port " << m_config.localPort
              << ", targeting " << m_config.remoteHost << ":" << m_config.remotePort << std::endl;
    return true;
}

void NetworkConnection::updateRemote(const std::string& host, uint16_t port) {
    m_config.remoteHost = host;
    m_config.remotePort = port;

    auto* remote = reinterpret_cast<sockaddr_in*>(m_remoteSockAddr);
    std::memset(remote, 0, sizeof(sockaddr_in));
    remote->sin_family = AF_INET;
    remote->sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &remote->sin_addr);
}

void NetworkConnection::closeConnection() {
    if (m_initialized && m_socketHandle != 0) {
        CLOSE_SOCKET(static_cast<socket_t>(m_socketHandle));
        m_socketHandle = 0;
        m_initialized = false;
    }
}

bool NetworkConnection::sendPacket(const uint8_t* data, size_t size) {
    if (!m_initialized || !data || size == 0) return false;
    auto* remote = reinterpret_cast<sockaddr_in*>(m_remoteSockAddr);
    int sent = sendto(static_cast<socket_t>(m_socketHandle),
                      reinterpret_cast<const char*>(data),
                      static_cast<int>(size),
                      0,
                      (sockaddr*)remote,
                      sizeof(sockaddr_in));
    return sent > 0;
}

int NetworkConnection::receivePacket(uint8_t* buffer, size_t maxBufferSize, int timeoutMs) {
    if (!m_initialized || !buffer) return -1;
    socket_t sock = static_cast<socket_t>(m_socketHandle);

#if defined(_WIN32)
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(0, &readSet, NULL, NULL, &tv);
    if (sel <= 0) {
        return sel; // 0 = timeout, <0 = error
    }
#else
    pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLIN;
    int ret = poll(&pfd, 1, timeoutMs);
    if (ret <= 0) {
        return ret;
    }
#endif

    sockaddr_in fromAddr{};
    socklen_t fromLen = sizeof(fromAddr);
    int received = recvfrom(sock,
                            reinterpret_cast<char*>(buffer),
                            static_cast<int>(maxBufferSize),
                            0,
                            (sockaddr*)&fromAddr,
                            &fromLen);
    return received;
}

} // namespace voip
