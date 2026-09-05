#include "shm_ring_buffer.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace voip {

ShmRingBuffer::ShmRingBuffer(const std::string& name, size_t capacity, Mode mode)
    : m_name(name)
    , m_totalSize(sizeof(RingBufferHeader) + capacity)
    , m_mode(mode)
    , m_mappedAddress(nullptr)
    , m_header(nullptr)
    , m_bufferStart(nullptr)
#if defined(_WIN32)
    , m_fileMappingHandle(nullptr)
#else
    , m_shmFd(-1)
#endif
{
    mapMemory();
}

ShmRingBuffer::~ShmRingBuffer() {
    unmapMemory();
}

bool ShmRingBuffer::mapMemory() {
#if defined(_WIN32)
    std::string fullName = "Local\\" + m_name;
    if (m_mode == Mode::Create) {
        m_fileMappingHandle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            (DWORD)(m_totalSize >> 32),
            (DWORD)(m_totalSize & 0xFFFFFFFF),
            fullName.c_str()
        );
        if (!m_fileMappingHandle) {
            std::cerr << "[ShmRingBuffer] Failed to create file mapping: " << GetLastError() << std::endl;
            return false;
        }
    } else {
        m_fileMappingHandle = OpenFileMappingA(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            fullName.c_str()
        );
        if (!m_fileMappingHandle) {
            std::cerr << "[ShmRingBuffer] Failed to open file mapping: " << GetLastError() << std::endl;
            return false;
        }
    }

    m_mappedAddress = MapViewOfFile(m_fileMappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, m_totalSize);
    if (!m_mappedAddress) {
        std::cerr << "[ShmRingBuffer] Failed to map view of file: " << GetLastError() << std::endl;
        CloseHandle(m_fileMappingHandle);
        m_fileMappingHandle = nullptr;
        return false;
    }
#else
    std::string shmPath = "/" + m_name;
    if (m_mode == Mode::Create) {
        m_shmFd = shm_open(shmPath.c_str(), O_CREAT | O_RDWR, 0666);
        if (m_shmFd < 0) {
            std::cerr << "[ShmRingBuffer] Failed to create shm: " << strerror(errno) << std::endl;
            return false;
        }
        if (ftruncate(m_shmFd, m_totalSize) != 0) {
            std::cerr << "[ShmRingBuffer] Failed to truncate shm: " << strerror(errno) << std::endl;
            close(m_shmFd);
            return false;
        }
    } else {
        m_shmFd = shm_open(shmPath.c_str(), O_RDWR, 0666);
        if (m_shmFd < 0) {
            std::cerr << "[ShmRingBuffer] Failed to open shm: " << strerror(errno) << std::endl;
            return false;
        }
    }

    m_mappedAddress = mmap(nullptr, m_totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);
    if (m_mappedAddress == MAP_FAILED) {
        std::cerr << "[ShmRingBuffer] mmap failed: " << strerror(errno) << std::endl;
        m_mappedAddress = nullptr;
        close(m_shmFd);
        return false;
    }
#endif

    m_header = reinterpret_cast<RingBufferHeader*>(m_mappedAddress);
    m_bufferStart = reinterpret_cast<uint8_t*>(m_mappedAddress) + sizeof(RingBufferHeader);

    if (m_mode == Mode::Create) {
        m_header->magic = SHM_MAGIC;
        m_header->version = 1;
        m_header->capacity = m_totalSize - sizeof(RingBufferHeader);
        m_header->write_pos.store(0, std::memory_order_relaxed);
        m_header->read_pos.store(0, std::memory_order_relaxed);
        m_header->sample_rate = DEFAULT_SAMPLE_RATE;
        m_header->channels = DEFAULT_CHANNELS;
        m_header->bit_depth = DEFAULT_BIT_DEPTH;
        m_header->sequence.store(0, std::memory_order_relaxed);
        std::memset(m_header->reserved, 0, sizeof(m_header->reserved));
    }

    return true;
}

void ShmRingBuffer::unmapMemory() {
    if (!m_mappedAddress) return;

#if defined(_WIN32)
    UnmapViewOfFile(m_mappedAddress);
    if (m_fileMappingHandle) {
        CloseHandle(m_fileMappingHandle);
        m_fileMappingHandle = nullptr;
    }
#else
    munmap(m_mappedAddress, m_totalSize);
    if (m_shmFd >= 0) {
        close(m_shmFd);
        m_shmFd = -1;
    }
    if (m_mode == Mode::Create) {
        std::string shmPath = "/" + m_name;
        shm_unlink(shmPath.c_str());
    }
#endif
    m_mappedAddress = nullptr;
    m_header = nullptr;
    m_bufferStart = nullptr;
}

size_t ShmRingBuffer::availableReadBytes() const {
    if (!m_header) return 0;
    uint64_t w = m_header->write_pos.load(std::memory_order_acquire);
    uint64_t r = m_header->read_pos.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (m_header->capacity - (r - w));
}

size_t ShmRingBuffer::availableWriteBytes() const {
    if (!m_header) return 0;
    return m_header->capacity - availableReadBytes() - 1;
}

bool ShmRingBuffer::writeFrame(const AudioFrameHeader& header, const uint8_t* payload) {
    if (!m_header || !payload) return false;
    size_t totalFrameBytes = sizeof(AudioFrameHeader) + header.payload_size;
    if (availableWriteBytes() < totalFrameBytes) {
        return false; // Ring full
    }

    uint64_t w = m_header->write_pos.load(std::memory_order_relaxed);
    uint64_t cap = m_header->capacity;

    auto writeContiguous = [&](const uint8_t* src, size_t len) {
        size_t firstChunk = std::min(len, static_cast<size_t>(cap - (w % cap)));
        std::memcpy(m_bufferStart + (w % cap), src, firstChunk);
        if (len > firstChunk) {
            std::memcpy(m_bufferStart, src + firstChunk, len - firstChunk);
        }
        w = (w + len) % cap;
    };

    writeContiguous(reinterpret_cast<const uint8_t*>(&header), sizeof(AudioFrameHeader));
    writeContiguous(payload, header.payload_size);

    m_header->write_pos.store(w, std::memory_order_release);
    m_header->sequence.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool ShmRingBuffer::readFrame(AudioFrameHeader& header, std::vector<uint8_t>& payload) {
    if (!m_header) return false;
    if (availableReadBytes() < sizeof(AudioFrameHeader)) {
        return false; // No frame available
    }

    uint64_t r = m_header->read_pos.load(std::memory_order_relaxed);
    uint64_t cap = m_header->capacity;

    auto readContiguous = [&](uint8_t* dst, size_t len) {
        size_t firstChunk = std::min(len, static_cast<size_t>(cap - (r % cap)));
        std::memcpy(dst, m_bufferStart + (r % cap), firstChunk);
        if (len > firstChunk) {
            std::memcpy(dst + firstChunk, m_bufferStart, len - firstChunk);
        }
        r = (r + len) % cap;
    };

    readContiguous(reinterpret_cast<uint8_t*>(&header), sizeof(AudioFrameHeader));

    if (header.magic != FRAME_MAGIC || header.payload_size > 1024 * 1024) {
        // Corrupted frame or desync, reset read pointer
        m_header->read_pos.store(m_header->write_pos.load(std::memory_order_relaxed), std::memory_order_release);
        return false;
    }

    if (availableReadBytes() < header.payload_size) {
        // Partial payload not yet written, rollback read pointer
        return false;
    }

    payload.resize(header.payload_size);
    readContiguous(payload.data(), header.payload_size);

    m_header->read_pos.store(r, std::memory_order_release);
    return true;
}

void ShmRingBuffer::reset() {
    if (m_header) {
        m_header->write_pos.store(0, std::memory_order_release);
        m_header->read_pos.store(0, std::memory_order_release);
        m_header->sequence.store(0, std::memory_order_release);
    }
}

} // namespace voip
