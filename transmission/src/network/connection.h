#pragma once

#include "transmission/common.h"
#include <string>
#include <memory>
#include <vector>

namespace voip {

#pragma pack(push, 1)
struct RtpHeader {
    uint8_t  version_p_x_cc; // V=2, P=0, X=0, CC=0 -> 0x80
    uint8_t  m_pt;           // Marker + Payload Type (e.g. 0x00 for PCM / 0x70 for custom)
    uint16_t sequence_num;   // Sequence Number
    uint32_t timestamp;      // Sample timestamp
    uint32_t ssrc;           // Synchronization source ID
};
#pragma pack(pop)

class NetworkConnection {
public:
    NetworkConnection(const SessionConfig& config);
    ~NetworkConnection();

    bool initialize();
    void closeConnection();
    bool isConnected() const { return m_initialized; }

    // Send an RTP packet over UDP
    bool sendPacket(const uint8_t* data, size_t size);

    // Receive an RTP packet from UDP socket. Returns received bytes or negative on error/timeout.
    int receivePacket(uint8_t* buffer, size_t maxBufferSize, int timeoutMs = 100);

    const SessionConfig& getConfig() const { return m_config; }
    void updateRemote(const std::string& host, uint16_t port);

private:
    SessionConfig m_config;
    bool m_initialized{false};
    intptr_t m_socketHandle{0};
    void* m_remoteSockAddr{nullptr};
};

} // namespace voip
