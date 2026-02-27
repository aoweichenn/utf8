//
// Created by aoweichen on 2026/2/27.
//

#pragma once
#include <cstdint>

namespace utf8::base {
inline constexpr uint32_t UTF8_BASE_REPLACEMENT_CHAR = 0xFFFD;
inline constexpr uint8_t UTF8_BASE_ASCII_LIMIT = 0x80;
inline constexpr uint8_t UTF8_BASE_BYTES_1 = 1;
}

namespace utf8::base {
enum class DecodeStatus : uint8_t {
    Success             = 0,
    TruncatedData       = 1,
    InvalidPrefix       = 2,
    InvalidContinuation = 3,
    OverlongEncoding    = 4,
    EndOfInput          = 5
};

enum class EncodeStatus : uint8_t {
    Success          = 0,
    InvalidCodepoint = 1,
    BufferTooSmall   = 2,
    InvalidInput     = 3,
};

struct DecodeResult final {
public:
    uint32_t codepoint;
    uint8_t bytes_consumed;
    DecodeStatus status;
    uint8_t _padding[2];

public:
    [[nodiscard]] constexpr bool is_valid() const noexcept
    {
        return this->status == DecodeStatus::Success;
    }

    [[nodiscard]] constexpr bool is_eof() const noexcept
    {
        return this->status == DecodeStatus::EndOfInput;
    }
};

static_assert(sizeof(DecodeResult) == 8, "FATAL: DecodeResult MUST be exactly 8 bytes for ABI stability");

struct DecodeUnsafeResult final {
    uint32_t codepoint;
    uint8_t bytes_consumed;
};
}
