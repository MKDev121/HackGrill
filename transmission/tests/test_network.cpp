#include "network/connection.h"
#include "audio/codec.h"
#include <iostream>
#include <vector>
#include <cassert>

int main() {
    std::cout << "[Test] Running AudioCodec unit tests..." << std::endl;

    voip::AudioCodec codec("PCM16");
    std::vector<int16_t> originalSamples = { 100, -200, 300, -400, 500, -600 };
    std::vector<uint8_t> encoded;
    bool encOk = codec.encode(originalSamples.data(), originalSamples.size(), encoded);
    assert(encOk && "Encode failed");
    assert(encoded.size() == originalSamples.size() * sizeof(int16_t));

    std::vector<int16_t> decoded;
    bool decOk = codec.decode(encoded.data(), encoded.size(), decoded);
    assert(decOk && "Decode failed");
    assert(decoded.size() == originalSamples.size());
    for (size_t i = 0; i < decoded.size(); ++i) {
        assert(decoded[i] == originalSamples[i]);
    }

    bool vadActive = voip::AudioCodec::calculateVAD(originalSamples.data(), originalSamples.size(), 50.0f);
    assert(vadActive && "VAD check failed");

    std::cout << "[Test] AudioCodec unit tests PASSED!" << std::endl;
    return 0;
}
