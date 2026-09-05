#include "receiver.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace voip {

Receiver::Receiver(std::shared_ptr<NetworkConnection> connection,
                   std::shared_ptr<ShmRingBuffer> rxShmBuffer,
                   std::shared_ptr<NetworkMetrics> metrics)
    : m_connection(std::move(connection))
    , m_rxShmBuffer(std::move(rxShmBuffer))
    , m_metrics(std::move(metrics))
{
}

Receiver::~Receiver() {
    stop();
}

bool Receiver::start() {
    if (m_running) return true;
    m_running = true;
    m_firstPacket = true;
    m_workerThread = std::thread(&Receiver::workerLoop, this);
    return true;
}

void Receiver::stop() {
    if (!m_running) return;
    m_running = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void Receiver::workerLoop() {
    std::vector<uint8_t> packetBuffer(2048);

    while (m_running) {
        int bytesRead = m_connection->receivePacket(packetBuffer.data(), packetBuffer.size(), 50);
        if (bytesRead <= 0) {
            continue;
        }

        if (bytesRead < static_cast<int>(sizeof(RtpHeader))) {
            continue; // runt packet
        }

        const auto* rtp = reinterpret_cast<const RtpHeader*>(packetBuffer.data());
        uint16_t seqNum = ntohs(rtp->sequence_num);
        uint32_t timestamp = ntohl(rtp->timestamp);

        if (!m_firstPacket) {
            int16_t diff = static_cast<int16_t>(seqNum - m_lastSeqNum);
            if (diff > 1) {
                // Lost packets detected
                double currentLoss = m_metrics->packetLossRate.load();
                m_metrics->packetLossRate.store(currentLoss * 0.9 + 0.1 * (diff - 1));
            }
        }
        m_firstPacket = false;
        m_lastSeqNum = seqNum;
        m_lastRtpTimestamp = timestamp;

        m_metrics->packetsReceived.fetch_add(1, std::memory_order_relaxed);
        m_metrics->bytesReceived.fetch_add(bytesRead, std::memory_order_relaxed);

        const uint8_t* payload = packetBuffer.data() + sizeof(RtpHeader);
        size_t payloadSize = bytesRead - sizeof(RtpHeader);

        // Frame header for Shared Memory buffer
        AudioFrameHeader frameHeader{};
        frameHeader.magic = FRAME_MAGIC;
        frameHeader.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frameHeader.sequence_num = seqNum;
        frameHeader.payload_size = static_cast<uint32_t>(payloadSize);
        frameHeader.sample_rate = DEFAULT_SAMPLE_RATE;
        frameHeader.channels = DEFAULT_CHANNELS;
        frameHeader.flags = 0;

        if (m_rxShmBuffer && m_rxShmBuffer->isValid()) {
            m_rxShmBuffer->writeFrame(frameHeader, payload);
        }
    }
}

} // namespace voip
