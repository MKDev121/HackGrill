#pragma once

#include "connection.h"
#include "transmission/common.h"
#include "../ipc/shm_ring_buffer.h"
#include <thread>
#include <atomic>
#include <memory>

namespace voip {

class Receiver {
public:
    Receiver(std::shared_ptr<NetworkConnection> connection,
             std::shared_ptr<ShmRingBuffer> rxShmBuffer,
             std::shared_ptr<NetworkMetrics> metrics);
    ~Receiver();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

private:
    std::shared_ptr<NetworkConnection> m_connection;
    std::shared_ptr<ShmRingBuffer> m_rxShmBuffer;
    std::shared_ptr<NetworkMetrics> m_metrics;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    uint32_t m_lastRtpTimestamp{0};
    uint16_t m_lastSeqNum{0};
    bool m_firstPacket{true};

    void workerLoop();
};

} // namespace voip
