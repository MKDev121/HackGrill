#include "transmission/common.h"
#include "network/connection.h"
#include "network/receiver.h"
#include "network/sender.h"
#include "ipc/shm_ring_buffer.h"
#include "ipc/control_socket.h"
#include "audio/codec.h"

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <sstream>

static std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\n[TransmissionApp] Signal " << signum << " received. Terminating..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "========================================================\n"
              << "   VoIP Transmission Engine (Combined Send/Receive)     \n"
              << "========================================================\n" << std::endl;

    voip::SessionConfig sessionConfig;
    sessionConfig.sessionId = "session-dev-001";
    sessionConfig.localPort = 5004;
    sessionConfig.remoteHost = "127.0.0.1";
    sessionConfig.remotePort = 5006;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--local-port" && i + 1 < argc) {
            sessionConfig.localPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--remote-port" && i + 1 < argc) {
            sessionConfig.remotePort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--remote-host" && i + 1 < argc) {
            sessionConfig.remoteHost = argv[++i];
        }
    }

    // 1. Initialize Shared Memory Buffers
    std::cout << "[TransmissionApp] Initializing Shared Memory Ring Buffers..." << std::endl;
    auto txShm = std::make_shared<voip::ShmRingBuffer>("voip_audio_tx_shm", voip::DEFAULT_SHM_CAPACITY, voip::ShmRingBuffer::Mode::Create);
    auto rxShm = std::make_shared<voip::ShmRingBuffer>("voip_audio_rx_shm", voip::DEFAULT_SHM_CAPACITY, voip::ShmRingBuffer::Mode::Create);

    if (!txShm->isValid() || !rxShm->isValid()) {
        std::cerr << "[TransmissionApp] FATAL: Failed to initialize shared memory ring buffers." << std::endl;
        return 1;
    }

    // 2. Initialize Network Connection
    auto metrics = std::make_shared<voip::NetworkMetrics>();
    auto connection = std::make_shared<voip::NetworkConnection>(sessionConfig);
    if (!connection->initialize()) {
        std::cerr << "[TransmissionApp] FATAL: Failed to initialize network connection." << std::endl;
        return 1;
    }

    // 3. Initialize Sender and Receiver threads
    auto receiver = std::make_unique<voip::Receiver>(connection, rxShm, metrics);
    auto sender = std::make_unique<voip::Sender>(connection, txShm, metrics);

    receiver->start();
    sender->start();
    std::cout << "[TransmissionApp] Network Receiver & Sender threads started." << std::endl;

    // 4. Initialize Control Socket IPC
    auto controlServer = std::make_unique<voip::ControlSocketServer>(voip::DEFAULT_CONTROL_PORT);
    controlServer->onMessage([&connection, &sessionConfig](const std::string& msg) {
        std::cout << "[TransmissionApp] Received Control IPC Msg: " << msg << std::endl;
        // Simple config parser hook
        if (msg.find("remote_host") != std::string::npos) {
            // Can update remote host/port dynamically
        }
    });
    controlServer->start();

    // 5. Main loop / telemetry broadcast
    std::cout << "[TransmissionApp] Transmission Engine Ready. Running event loop..." << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Format telemetry JSON
        std::ostringstream ss;
        ss << "{\"type\":\"METRICS_UPDATE\","
           << "\"packets_sent\":" << metrics->packetsSent.load() << ","
           << "\"packets_received\":" << metrics->packetsReceived.load() << ","
           << "\"packet_loss_rate\":" << metrics->packetLossRate.load() << ","
           << "\"tx_bytes\":" << metrics->bytesSent.load() << ","
           << "\"rx_bytes\":" << metrics->bytesReceived.load() << "}";

        controlServer->broadcast(ss.str());
    }

    std::cout << "[TransmissionApp] Stopping threads..." << std::endl;
    sender->stop();
    receiver->stop();
    controlServer->stop();
    connection->closeConnection();

    std::cout << "[TransmissionApp] Clean shutdown complete." << std::endl;
    return 0;
}
