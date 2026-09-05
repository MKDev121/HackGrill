#pragma once

#include "transmission/common.h"
#include <vector>
#include <cstdint>

namespace voip {

class AudioCodec {
public:
    AudioCodec(const std::string& codecName = "PCM16");
    ~AudioCodec();

    // Encode raw PCM16 samples to wire payload
    bool encode(const int16_t* pcmSamples, size_t sampleCount, std::vector<uint8_t>& outEncoded);

    // Decode wire payload to PCM16 samples
    bool decode(const uint8_t* encodedBytes, size_t byteCount, std::vector<int16_t>& outPcm);

    // Basic Voice Activity Detection (energy threshold)
    static bool calculateVAD(const int16_t* samples, size_t count, float thresholdEnergy = 500.0f);

    // Apply linear gain / normalization
    static void applyGain(int16_t* samples, size_t count, float gainFactor);

private:
    std::string m_codecName;
};

} // namespace voip
