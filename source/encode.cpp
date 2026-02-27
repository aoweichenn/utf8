//
// Created by aoweichen on 2026/2/27.
//

#include <utf8/utf8.hpp>
#include "internal.hpp"

namespace utf8 {
namespace {
using namespace utf8::internal;
}

[[nodiscard]] UTF8_API std::pair<base::EncodeStatus, uint8_t> encode(const uint32_t cp,
                                                                     std::span<uint8_t> out_buffer) noexcept
{
    if (cp > UTF8_INTERNAL_MAX_CODEPOINT ||
        (cp >= UTF8_INTERNAL_SURROGATE_START && cp <= UTF8_INTERNAL_SURROGATE_END)) [[unlikely]]
    {
        return {base::EncodeStatus::InvalidCodepoint, 0};
    }
    if (cp <= UTF8_INTERNAL_ENCODE_1_BYTE_MAX) [[likely]]
    {
        if (out_buffer.empty()) [[unlikely]]
        {
            return {base::EncodeStatus::BufferTooSmall, 0};
        }
        out_buffer[0] = static_cast<uint8_t>(cp);
        return {base::EncodeStatus::Success, base::UTF8_BASE_BYTES_1};
    }
    if (cp <= UTF8_INTERNAL_ENCODE_2_BYTE_MAX)
    {
        if (out_buffer.size() < UTF8_INTERNAL_BYTES_2) [[unlikely]]
        {
            return {base::EncodeStatus::BufferTooSmall, 0};
        }
        out_buffer[0] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_2_BYTE | (cp >> UTF8_INTERNAL_SHIFT_STEP_1));
        out_buffer[1] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (
                                                 cp & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
        return {base::EncodeStatus::Success, UTF8_INTERNAL_BYTES_2};
    }
    if (cp <= UTF8_INTERNAL_ENCODE_3_BYTE_MAX)
    {
        if (out_buffer.size() < UTF8_INTERNAL_BYTES_3) [[unlikely]]
        {
            return {base::EncodeStatus::BufferTooSmall, 0};
        }
        out_buffer[0] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_3_BYTE | (cp >> UTF8_INTERNAL_SHIFT_STEP_2));
        out_buffer[1] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (
                                                 (cp >> UTF8_INTERNAL_SHIFT_STEP_1) & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
        out_buffer[2] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (
                                                 cp & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
        return {base::EncodeStatus::Success, UTF8_INTERNAL_BYTES_3};
    }
    if (out_buffer.size() < UTF8_INTERNAL_BYTES_4) [[unlikely]]
    {
        return {base::EncodeStatus::BufferTooSmall, 0};
    }
    out_buffer[0] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_4_BYTE | (cp >> UTF8_INTERNAL_SHIFT_STEP_3));
    out_buffer[1] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (
                                             (cp >> UTF8_INTERNAL_SHIFT_STEP_2) & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
    out_buffer[2] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (
                                             (cp >> UTF8_INTERNAL_SHIFT_STEP_1) & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
    out_buffer[3] = static_cast<uint8_t>(UTF8_INTERNAL_PREFIX_CONTINUATION | (cp & UTF8_INTERNAL_CONT_PAYLOAD_MASK));
    return {base::EncodeStatus::Success, UTF8_INTERNAL_BYTES_4};
}

[[nodiscard]] UTF8_API base::EncodeStatus utf16_length_from_utf8(const std::span<const uint8_t> utf8_data,
                                                                 size_t &out_length) noexcept
{
    out_length = 0;
    const uint8_t *ptr = utf8_data.data();
    const uint8_t *end = ptr + utf8_data.size();

    while (ptr < end)
    {
        auto res = decode_next(std::span<const uint8_t>(ptr, end));
        if (!res.is_valid()) [[unlikely]]
        {
            return base::EncodeStatus::InvalidInput;
        }
        out_length += (res.codepoint >= UTF8_INTERNAL_U16_SUPPLEMENTARY_MIN) ? 2 : 1;
        ptr += res.bytes_consumed;
    }
    return base::EncodeStatus::Success;
}

[[nodiscard]] UTF8_API base::EncodeStatus utf8_to_utf16(const std::span<const uint8_t> utf8_data,
                                                        std::span<char16_t> out_buffer,
                                                        size_t &out_written_len) noexcept
{
    out_written_len = 0;
    const uint8_t *ptr = utf8_data.data();
    const uint8_t *end = ptr + utf8_data.size();

    while (ptr < end)
    {
        if (out_written_len >= out_buffer.size()) [[unlikely]]
        {
            return base::EncodeStatus::BufferTooSmall;
        }
        auto res = decode_next(std::span<const uint8_t>(ptr, end));
        if (!res.is_valid()) [[unlikely]]
        {
            return base::EncodeStatus::InvalidInput;
        }

        const uint32_t cp = res.codepoint;
        ptr += res.bytes_consumed;

        if (cp < UTF8_INTERNAL_U16_SUPPLEMENTARY_MIN) [[likely]]
        {
            out_buffer[out_written_len++] = static_cast<char16_t>(cp);
        }
        else
        {
            if (out_written_len + 1 >= out_buffer.size()) [[unlikely]]
            {
                return base::EncodeStatus::BufferTooSmall;
            }
            const uint32_t cp_adj = cp - UTF8_INTERNAL_U16_SUPPLEMENTARY_MIN;
            out_buffer[out_written_len++] = static_cast<char16_t>(
                UTF8_INTERNAL_U16_HIGH_SURROGATE | (cp_adj >> UTF8_INTERNAL_U16_SHIFT));
            out_buffer[out_written_len++] = static_cast<char16_t>(
                UTF8_INTERNAL_U16_LOW_SURROGATE | (cp_adj & UTF8_INTERNAL_U16_MASK));
        }
    }
    return base::EncodeStatus::Success;
}
}
