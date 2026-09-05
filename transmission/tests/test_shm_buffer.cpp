#include "ipc/shm_ring_buffer.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

int main() {
    std::cout << "[Test] Running ShmRingBuffer unit tests..." << std::endl;

    std::string testShmName = "voip_test_ring_buf";
    voip::ShmRingBuffer writer(testShmName, 64 * 1024, voip::ShmRingBuffer::Mode::Create);
    assert(writer.isValid() && "Writer SHM creation failed");

    voip::AudioFrameHeader frameHeader{};
    frameHeader.magic = voip::FRAME_MAGIC;
    frameHeader.timestamp_us = 1000000;
    frameHeader.sequence_num = 1;
    frameHeader.payload_size = 640; // 20ms of 16kHz PCM16
    frameHeader.sample_rate = 16000;
    frameHeader.channels = 1;
    frameHeader.flags = 0;

    std::vector<uint8_t> payload(640, 0x55);
    bool written = writer.writeFrame(frameHeader, payload.data());
    assert(written && "Failed to write frame to SHM buffer");

    voip::AudioFrameHeader readHeader{};
    std::vector<uint8_t> readPayload;
    bool readSuccess = writer.readFrame(readHeader, readPayload);
    assert(readSuccess && "Failed to read frame from SHM buffer");
    assert(readHeader.sequence_num == 1 && "Sequence number mismatch");
    assert(readPayload.size() == 640 && "Payload size mismatch");
    assert(readPayload[0] == 0x55 && "Payload content mismatch");

    std::cout << "[Test] ShmRingBuffer unit tests PASSED!" << std::endl;
    return 0;
}
