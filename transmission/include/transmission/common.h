#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <atomic>

namespace voip {

constexpr uint32_t SHM_MAGIC = 0x53484D52; // 'SHMR'
constexpr uint32_t FRAME_MAGIC = 0x4652414D; // 'FRAM'
constexpr uint32_t DEFAULT_SAMPLE_RATE = 16000;
constexpr uint16_t DEFAULT_CHANNELS = 1;
constexpr uint16_t DEFAULT_BIT_DEPTH = 16;
constexpr size_t DEFAULT_SHM_CAPACITY = 1024 * 1024; // 1 MB
constexpr uint16_t DEFAULT_CONTROL_PORT = 9200;

#pragma pack(push, 1)
struct RingBufferHeader {
    uint32_t magic;                 // SHM_MAGIC
    uint32_t version;               // 1
    uint64_t capacity;              // Total buffer bytes
    std::atomic<uint64_t> write_pos;// Offset for next write
    std::atomic<uint64_t> read_pos; // Offset for next read
    uint32_t sample_rate;           // e.g. 16000
    uint16_t channels;              // 1
    uint16_t bit_depth;             // 16
    std::atomic<uint64_t> sequence; // Frame count sequence
    uint8_t  reserved[16];          // Alignment padding
};

struct AudioFrameHeader {
    uint32_t magic;                 // FRAME_MAGIC
    uint64_t timestamp_us;          // Microsecond timestamp (epoch/monotonic)
    uint64_t sequence_num;          // Incremental sequence ID
    uint32_t payload_size;          // Audio bytes following header
    uint16_t sample_rate;           // Sample rate
    uint8_t  channels;              // Channel count
    uint8_t  flags;                 // Bit flags (0x1 = active speech, 0x2 = muted)
};
#pragma pack(pop)

struct SessionConfig {
    std::string sessionId;
    std::string remoteHost = "127.0.0.1";
    uint16_t remotePort = 5004;
    uint16_t localPort = 5004;
    std::string codec = "PCM16";
    std::string sourceLang = "hi-IN";
    std::string targetLang = "en-IN";
    bool enableVAD = true;
};

struct NetworkMetrics {
    std::atomic<uint64_t> packetsSent{0};
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> bytesSent{0};
    std::atomic<uint64_t> bytesReceived{0};
    std::atomic<double> packetLossRate{0.0};
    std::atomic<double> jitterMs{0.0};
    std::atomic<double> rttMs{0.0};
};

} // namespace voip
