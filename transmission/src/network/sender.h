#pragma once

#include "connection.h"
#include "transmission/common.h"
#include "../ipc/shm_ring_buffer.h"
#include <thread>
#include <atomic>
#include <memory>

namespace voip {

class Sender {
public:
    Sender(std::shared_ptr<NetworkConnection> connection,
           std::shared_ptr<ShmRingBuffer> txShmBuffer,
           std::shared_ptr<NetworkMetrics> metrics);
    ~Sender();

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

private:
    std::shared_ptr<NetworkConnection> m_connection;
    std::shared_ptr<ShmRingBuffer> m_txShmBuffer;
    std::shared_ptr<NetworkMetrics> m_metrics;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;

    uint16_t m_sequenceNum{0};
    uint32_t m_rtpTimestamp{0};
    uint32_t m_ssrc{0x12345678};

    void workerLoop();
};

} // namespace voip
