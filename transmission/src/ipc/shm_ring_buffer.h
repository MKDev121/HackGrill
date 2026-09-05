#pragma once

#include "transmission/common.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace voip {

class ShmRingBuffer {
public:
    enum class Mode {
        Create,  // Create and initialize new shared memory
        Attach   // Attach to existing shared memory region
    };

    ShmRingBuffer(const std::string& name, size_t capacity = DEFAULT_SHM_CAPACITY, Mode mode = Mode::Create);
    ~ShmRingBuffer();

    bool isValid() const { return m_mappedAddress != nullptr; }
    const std::string& getName() const { return m_name; }

    // Write a discrete audio frame into the ring buffer
    bool writeFrame(const AudioFrameHeader& header, const uint8_t* payload);

    // Read next available audio frame from the ring buffer. Returns false if empty.
    bool readFrame(AudioFrameHeader& header, std::vector<uint8_t>& payload);

    // Get number of available unread bytes
    size_t availableReadBytes() const;
    size_t availableWriteBytes() const;

    void reset();

private:
    std::string m_name;
    size_t m_totalSize;
    Mode m_mode;
    void* m_mappedAddress;
    RingBufferHeader* m_header;
    uint8_t* m_bufferStart;

#if defined(_WIN32)
    void* m_fileMappingHandle;
#else
    int m_shmFd;
#endif

    bool mapMemory();
    void unmapMemory();
};

} // namespace voip
