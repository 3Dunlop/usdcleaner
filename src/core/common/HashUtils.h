#pragma once

#include "core/common/Types.h"

#include <string>

namespace usdcleaner {

// Lightweight SHA-256 implementation for material hashing.
// Uses picosha2 when available, otherwise a minimal built-in implementation.
class USDCLEANER_API SHA256Hasher {
public:
    SHA256Hasher();
    ~SHA256Hasher();

    // Feed data incrementally
    void Update(const void* data, size_t length);
    void Update(const std::string& str);
    void Update(float value);
    void Update(int value);

    // Finalize and return the 32-byte digest
    HashDigest Finalize();

    // Reset for reuse
    void Reset();

    // Convenience: hash a single string
    static HashDigest Hash(const std::string& input);

    // Convert digest to hex string for display
    static std::string DigestToHex(const HashDigest& digest);

private:
    // Internal state for incremental hashing
    struct State;
    std::unique_ptr<State> state_;
};

// Hash a sorted sequence of integers (used for face deduplication)
USDCLEANER_API size_t HashIndexSequence(const int* indices, size_t count);

} // namespace usdcleaner
