#include "core/common/HashUtils.h"

#include <cstring>
#include <sstream>
#include <iomanip>
#include <functional>

// Minimal SHA-256 implementation (standalone, no external dependency)
// Based on FIPS 180-4. For production, consider replacing with picosha2 or OpenSSL.
namespace {

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

} // anonymous namespace

namespace usdcleaner {

struct SHA256Hasher::State {
    uint32_t h[8];
    uint8_t buffer[64];
    size_t bufferLen = 0;
    uint64_t totalLen = 0;

    State() { Reset(); }

    void Reset() {
        h[0] = 0x6a09e667; h[1] = 0xbb67ae85;
        h[2] = 0x3c6ef372; h[3] = 0xa54ff53a;
        h[4] = 0x510e527f; h[5] = 0x9b05688c;
        h[6] = 0x1f83d9ab; h[7] = 0x5be0cd19;
        bufferLen = 0;
        totalLen = 0;
    }

    void ProcessBlock(const uint8_t* block) {
        uint32_t W[64];
        for (int i = 0; i < 16; i++) {
            W[i] = (uint32_t(block[i*4]) << 24)
                 | (uint32_t(block[i*4+1]) << 16)
                 | (uint32_t(block[i*4+2]) << 8)
                 | uint32_t(block[i*4+3]);
        }
        for (int i = 16; i < 64; i++) {
            W[i] = gamma1(W[i-2]) + W[i-7] + gamma0(W[i-15]) + W[i-16];
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; i++) {
            uint32_t t1 = hh + sigma1(e) + ch(e, f, g) + K[i] + W[i];
            uint32_t t2 = sigma0(a) + maj(a, b, c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
};

SHA256Hasher::SHA256Hasher() : state_(std::make_unique<State>()) {}
SHA256Hasher::~SHA256Hasher() = default;

void SHA256Hasher::Update(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    state_->totalLen += length;

    while (length > 0) {
        size_t space = 64 - state_->bufferLen;
        size_t toCopy = (length < space) ? length : space;
        std::memcpy(state_->buffer + state_->bufferLen, bytes, toCopy);
        state_->bufferLen += toCopy;
        bytes += toCopy;
        length -= toCopy;

        if (state_->bufferLen == 64) {
            state_->ProcessBlock(state_->buffer);
            state_->bufferLen = 0;
        }
    }
}

void SHA256Hasher::Update(const std::string& str) {
    Update(str.data(), str.size());
}

void SHA256Hasher::Update(float value) {
    Update(&value, sizeof(value));
}

void SHA256Hasher::Update(int value) {
    Update(&value, sizeof(value));
}

HashDigest SHA256Hasher::Finalize() {
    // Padding
    uint64_t bitLen = state_->totalLen * 8;
    uint8_t pad = 0x80;
    Update(&pad, 1);

    uint8_t zero = 0;
    while (state_->bufferLen != 56) {
        Update(&zero, 1);
    }

    // Append bit length (big-endian)
    uint8_t lenBytes[8];
    for (int i = 7; i >= 0; i--) {
        lenBytes[i] = static_cast<uint8_t>(bitLen & 0xFF);
        bitLen >>= 8;
    }
    Update(lenBytes, 8);

    // Extract digest
    HashDigest digest;
    for (int i = 0; i < 8; i++) {
        digest[i*4]   = static_cast<uint8_t>(state_->h[i] >> 24);
        digest[i*4+1] = static_cast<uint8_t>(state_->h[i] >> 16);
        digest[i*4+2] = static_cast<uint8_t>(state_->h[i] >> 8);
        digest[i*4+3] = static_cast<uint8_t>(state_->h[i]);
    }

    return digest;
}

void SHA256Hasher::Reset() {
    state_->Reset();
}

HashDigest SHA256Hasher::Hash(const std::string& input) {
    SHA256Hasher hasher;
    hasher.Update(input);
    return hasher.Finalize();
}

std::string SHA256Hasher::DigestToHex(const HashDigest& digest) {
    std::ostringstream ss;
    for (uint8_t byte : digest) {
        ss << std::hex << std::setfill('0') << std::setw(2)
           << static_cast<int>(byte);
    }
    return ss.str();
}

size_t HashIndexSequence(const int* indices, size_t count) {
    size_t h = 0;
    for (size_t i = 0; i < count; ++i) {
        // boost::hash_combine equivalent
        h ^= std::hash<int>{}(indices[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

} // namespace usdcleaner
