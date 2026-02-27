//
// Created by aoweichen on 2026/2/27.
//

#include <utf8/utf8.hpp>
#include "internal.hpp"

namespace utf8 {
namespace {
using namespace utf8::internal;
using namespace utf8::base;

inline constexpr size_t UTF8_DFA_TABLE_SIZE = 364;
alignas(64) constexpr uint8_t UTF8_DFA_TABLE[UTF8_DFA_TABLE_SIZE] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 3, 3, 11, 6, 6, 6, 5, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,

    0, 12, 24, 36, 60, 96, 84, 12, 12, 12, 48, 72, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 0, 12, 12, 12, 12, 12, 0, 12, 0, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 24, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12,
    12, 36, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12
};

[[nodiscard]] base::DecodeStatus diagnose_error(const uint8_t b0, const uint8_t b1, const uint8_t failed_byte) noexcept
{
    if (b0 == UTF8_INTERNAL_SURROGATE_PREFIX && b1 >= UTF8_INTERNAL_SURROGATE_MIN && b1 <= UTF8_INTERNAL_SURROGATE_MAX)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (b0 == UTF8_INTERNAL_OUT_OF_BOUNDS_PREFIX && b1 >= UTF8_INTERNAL_OUT_OF_BOUNDS_MIN && b1 <=
        UTF8_INTERNAL_OUT_OF_BOUNDS_MAX)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (b0 == UTF8_INTERNAL_OVERLONG_3_PREFIX && b1 >= UTF8_INTERNAL_OVERLONG_3_MIN && b1 <=
        UTF8_INTERNAL_OVERLONG_3_MAX)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    if (b0 == UTF8_INTERNAL_OVERLONG_4_PREFIX && b1 >= UTF8_INTERNAL_OVERLONG_4_MIN && b1 <=
        UTF8_INTERNAL_OVERLONG_4_MAX)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    if (b0 >= UTF8_INTERNAL_CONT_MIN && b0 <= UTF8_INTERNAL_CONT_MAX)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (b0 >= UTF8_INTERNAL_INVALID_PREFIX_F5)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (b0 == UTF8_INTERNAL_OVERLONG_PREFIX_C0 || b0 == UTF8_INTERNAL_OVERLONG_PREFIX_C1)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    if ((failed_byte & UTF8_INTERNAL_CONT_CHECK_MASK) != UTF8_INTERNAL_CONT_MIN)
    {
        return base::DecodeStatus::InvalidContinuation;
    }
    return base::DecodeStatus::InvalidContinuation; // LCOV_EXCL_LINE
}

[[nodiscard]] constexpr base::DecodeStatus strict_postcheck(const uint32_t cp, const uint8_t len) noexcept
{
    if (cp > UTF8_INTERNAL_MAX_CODEPOINT)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    // LCOV_EXCL_START
    if (cp >= UTF8_INTERNAL_SURROGATE_START && cp <= UTF8_INTERNAL_SURROGATE_END)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (len == base::UTF8_BASE_BYTES_1 && cp >= base::UTF8_BASE_ASCII_LIMIT)
    {
        return base::DecodeStatus::InvalidPrefix;
    }
    if (len == UTF8_INTERNAL_BYTES_2 && cp <= UTF8_INTERNAL_ENCODE_1_BYTE_MAX)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    if (len == UTF8_INTERNAL_BYTES_3 && cp <= UTF8_INTERNAL_ENCODE_2_BYTE_MAX)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    if (len == UTF8_INTERNAL_BYTES_4 && cp <= UTF8_INTERNAL_ENCODE_3_BYTE_MAX)
    {
        return base::DecodeStatus::OverlongEncoding;
    }
    // LCOV_EXCL_STOP
    return base::DecodeStatus::Success;
}

[[nodiscard]] constexpr uint8_t expected_len(const uint8_t b0) noexcept
{
    if (b0 < base::UTF8_BASE_ASCII_LIMIT)
    {
        return base::UTF8_BASE_BYTES_1; // LCOV_EXCL_LINE
    }
    if (b0 >= UTF8_INTERNAL_LEN2_MIN && b0 <= UTF8_INTERNAL_LEN2_MAX)
    {
        return UTF8_INTERNAL_BYTES_2;
    }
    if (b0 >= UTF8_INTERNAL_LEN3_MIN && b0 <= UTF8_INTERNAL_LEN3_MAX)
    {
        return UTF8_INTERNAL_BYTES_3;
    }
    if (b0 >= UTF8_INTERNAL_LEN4_MIN && b0 <= UTF8_INTERNAL_LEN4_MAX)
    {
        return UTF8_INTERNAL_BYTES_4;
    }
    return 0; // LCOV_EXCL_LINE
}
}
}

namespace utf8::detail {
[[nodiscard]] UTF8_API base::DecodeResult decode_slow_path(const std::span<const uint8_t> data) noexcept
{
    if (data.empty()) [[unlikely]]
    {
        return {base::UTF8_BASE_REPLACEMENT_CHAR, 0, base::DecodeStatus::EndOfInput, {0, 0}};
    }

    uint32_t codepoint = 0;
    uint8_t state = UTF8_INTERNAL_DFA_ACCEPT;
    const uint8_t first_byte = data[0];
    const uint8_t second_byte = data.size() > 1 ? data[1] : 0;
    const uint8_t limit = static_cast<uint8_t>(
        data.size() > UTF8_INTERNAL_BYTES_4 ? UTF8_INTERNAL_BYTES_4 : data.size());

    for (uint8_t i = 0; i < limit; ++i)
    {
        const uint8_t byte = data[i];
        const uint8_t type = UTF8_DFA_TABLE[byte];
        codepoint = (state != UTF8_INTERNAL_DFA_ACCEPT)
                        ? (byte & UTF8_INTERNAL_CONT_PAYLOAD_MASK) | (codepoint << UTF8_INTERNAL_SHIFT_STEP_1)
                        : (0xFF >> type) & byte;
        state = UTF8_DFA_TABLE[UTF8_INTERNAL_DFA_STATE_OFFSET + state + type];

        if (state == UTF8_INTERNAL_DFA_ACCEPT) [[likely]]
        {
            const uint8_t bytes_consumed = i + 1;
            const auto st = strict_postcheck(codepoint, bytes_consumed);
            if (st != base::DecodeStatus::Success) [[unlikely]]
            {
                return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1, st, {0, 0}};
            }
            return {codepoint, bytes_consumed, base::DecodeStatus::Success, {0, 0}};
        }
        if (state == UTF8_INTERNAL_DFA_REJECT) [[unlikely]]
        {
            return {
                base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1,
                diagnose_error(first_byte, second_byte, byte), {0, 0}
            };
        }
    }

    const uint8_t need = expected_len(first_byte);
    // LCOV_EXCL_START
    if (need == 0)
    {
        return {
            base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1,
            diagnose_error(first_byte, second_byte, first_byte), {0, 0}
        };
    }
    // LCOV_EXCL_STOP
    if (data.size() < need)
    {
        return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1, base::DecodeStatus::TruncatedData, {0, 0}};
    }
    return {
        base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1, base::DecodeStatus::InvalidPrefix, {0, 0}
    }; // LCOV_EXCL_LINE
}

[[nodiscard]] UTF8_API base::DecodeUnsafeResult decode_unsafe_slow_path_padded(const uint8_t *ptr) noexcept
{
    uint32_t codepoint = 0;
    uint8_t state = UTF8_INTERNAL_DFA_ACCEPT;

    for (uint8_t i = 0; i < UTF8_INTERNAL_BYTES_4; ++i)
    {
        const uint8_t byte = ptr[i];
        const uint8_t type = UTF8_DFA_TABLE[byte];
        codepoint = (state != UTF8_INTERNAL_DFA_ACCEPT)
                        ? (byte & UTF8_INTERNAL_CONT_PAYLOAD_MASK) | (codepoint << UTF8_INTERNAL_SHIFT_STEP_1)
                        : (0xFF >> type) & byte;
        state = UTF8_DFA_TABLE[UTF8_INTERNAL_DFA_STATE_OFFSET + state + type];

        if (state == UTF8_INTERNAL_DFA_ACCEPT) [[likely]]
        {
            if (strict_postcheck(codepoint, static_cast<uint8_t>(i + 1)) != base::DecodeStatus::Success) [[unlikely]]
            {
                return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1};
            }
            return {codepoint, static_cast<uint8_t>(i + 1)};
        }
        if (state == UTF8_INTERNAL_DFA_REJECT) [[unlikely]]
        {
            return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1};
        }
    }
    return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1};
}
}

namespace utf8 {
[[nodiscard]] UTF8_API base::DecodeResult decode_prev(std::span<const uint8_t> data) noexcept
{
    if (data.empty()) [[unlikely]] return {base::UTF8_BASE_REPLACEMENT_CHAR, 0, base::DecodeStatus::EndOfInput, {0, 0}};

    size_t start = data.size() - 1;
    const size_t limit = (data.size() >= UTF8_INTERNAL_BYTES_4) ? (data.size() - UTF8_INTERNAL_BYTES_4) : 0;

    while (start > limit && (data[start] & UTF8_INTERNAL_CONT_CHECK_MASK) == UTF8_INTERNAL_CONT_MIN)
    {
        --start;
    }

    auto target = data.subspan(start, data.size() - start);
    auto res = decode_next(target);

    if (!res.is_valid()) [[unlikely]]
    {
        return {base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1, res.status, {0, 0}};
    }
    if (res.bytes_consumed != target.size()) [[unlikely]]
    {
        return {
            base::UTF8_BASE_REPLACEMENT_CHAR, base::UTF8_BASE_BYTES_1, base::DecodeStatus::InvalidContinuation, {0, 0}
        };
    }
    return res;
}
}
