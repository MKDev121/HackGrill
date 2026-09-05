#include "codec.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace voip {

AudioCodec::AudioCodec(const std::string& codecName)
    : m_codecName(codecName)
{
}

AudioCodec::~AudioCodec() = default;

bool AudioCodec::encode(const int16_t* pcmSamples, size_t sampleCount, std::vector<uint8_t>& outEncoded) {
    if (!pcmSamples || sampleCount == 0) return false;
    size_t byteCount = sampleCount * sizeof(int16_t);
    outEncoded.resize(byteCount);
    std::memcpy(outEncoded.data(), pcmSamples, byteCount);
    return true;
}

bool AudioCodec::decode(const uint8_t* encodedBytes, size_t byteCount, std::vector<int16_t>& outPcm) {
    if (!encodedBytes || byteCount < sizeof(int16_t)) return false;
    size_t sampleCount = byteCount / sizeof(int16_t);
    outPcm.resize(sampleCount);
    std::memcpy(outPcm.data(), encodedBytes, sampleCount * sizeof(int16_t));
    return true;
}

bool AudioCodec::calculateVAD(const int16_t* samples, size_t count, float thresholdEnergy) {
    if (!samples || count == 0) return false;
    double sumSq = 0;
    for (size_t i = 0; i < count; ++i) {
        sumSq += static_cast<double>(samples[i]) * samples[i];
    }
    double rms = std::sqrt(sumSq / count);
    return rms > thresholdEnergy;
}

void AudioCodec::applyGain(int16_t* samples, size_t count, float gainFactor) {
    if (!samples || count == 0) return;
    for (size_t i = 0; i < count; ++i) {
        float val = samples[i] * gainFactor;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        samples[i] = static_cast<int16_t>(val);
    }
}

} // namespace voip
