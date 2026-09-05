#pragma once

#include "transmission/common.h"
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace voip {

using MessageCallback = std::function<void(const std::string& message)>;

class ControlSocketServer {
public:
    ControlSocketServer(uint16_t port = DEFAULT_CONTROL_PORT);
    ~ControlSocketServer();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    // Broadcast message to all connected clients (e.g. Flutter frontend / Python processing)
    void broadcast(const std::string& jsonMessage);

    // Register callback for incoming messages
    void onMessage(MessageCallback callback);

private:
    uint16_t m_port;
    std::atomic<bool> m_running{false};
    std::thread m_listenThread;
    MessageCallback m_callback;
    std::mutex m_clientsMutex;
    std::vector<intptr_t> m_clientSockets;

    void listenLoop();
    void clientWorker(intptr_t clientSock);
};

} // namespace voip
