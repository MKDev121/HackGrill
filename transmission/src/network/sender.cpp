#include "sender.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace voip {

Sender::Sender(std::shared_ptr<NetworkConnection> connection,
               std::shared_ptr<ShmRingBuffer> txShmBuffer,
               std::shared_ptr<NetworkMetrics> metrics)
    : m_connection(std::move(connection))
    , m_txShmBuffer(std::move(txShmBuffer))
    , m_metrics(std::move(metrics))
{
}

Sender::~Sender() {
    stop();
}

bool Sender::start() {
    if (m_running) return true;
    m_running = true;
    m_workerThread = std::thread(&Sender::workerLoop, this);
    return true;
}

void Sender::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void Sender::workerLoop() {
    AudioFrameHeader frameHeader{};
    std::vector<uint8_t> payload;
    std::vector<uint8_t> packetBuffer(2048);

    while (m_running) {
        if (!m_txShmBuffer || !m_txShmBuffer->isValid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        bool hasFrame = m_txShmBuffer->readFrame(frameHeader, payload);
        if (!hasFrame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        size_t rtpTotalSize = sizeof(RtpHeader) + payload.size();
        if (rtpTotalSize > packetBuffer.size()) {
            packetBuffer.resize(rtpTotalSize);
        }

        auto* rtp = reinterpret_cast<RtpHeader*>(packetBuffer.data());
        rtp->version_p_x_cc = 0x80; // V=2
        rtp->m_pt = 0x60;           // Dynamic payload type
        rtp->sequence_num = htons(m_sequenceNum++);
        rtp->timestamp = htonl(m_rtpTimestamp);
        rtp->ssrc = htonl(m_ssrc);

        // Advance timestamp by sample count (assuming 16-bit mono -> 2 bytes per sample)
        size_t samples = payload.size() / sizeof(int16_t);
        m_rtpTimestamp += static_cast<uint32_t>(samples);

        std::memcpy(packetBuffer.data() + sizeof(RtpHeader), payload.data(), payload.size());

        bool sent = m_connection->sendPacket(packetBuffer.data(), rtpTotalSize);
        if (sent) {
            m_metrics->packetsSent.fetch_add(1, std::memory_order_relaxed);
            m_metrics->bytesSent.fetch_add(rtpTotalSize, std::memory_order_relaxed);
        }
    }
}

} // namespace voip
