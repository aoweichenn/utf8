//
// Created by aoweichen on 2026/2/27.
//

#pragma once

#include <span>
#include <utf8/base.hpp>
#include <utf8/export.hpp>

namespace utf8::detail {
[[nodiscard]] UTF8_API base::DecodeResult decode_slow_path(std::span<const uint8_t> data) noexcept;
[[nodiscard]] UTF8_API base::DecodeUnsafeResult decode_unsafe_slow_path_padded(const uint8_t *ptr) noexcept;
}

namespace utf8 {
[[nodiscard]] inline base::DecodeResult decode_next(const std::span<const uint8_t> data) noexcept
{
    if (data.empty()) [[unlikely]]
    {
        return {base::UTF8_BASE_REPLACEMENT_CHAR, 0, base::DecodeStatus::EndOfInput, {0, 0}};
    }
    if (data.front() < base::UTF8_BASE_ASCII_LIMIT) [[likely]]
    {
        return {data.front(), base::UTF8_BASE_BYTES_1, base::DecodeStatus::Success, {0, 0}};
    }
    return detail::decode_slow_path(data);
}

// 契约: 调用方必须保证 ptr 之后包含至少 4 字节的安全冗余边界 (Padding Sentinel)
[[nodiscard]] inline base::DecodeUnsafeResult decode_next_unsafe_padded(const uint8_t *ptr) noexcept
{
    if (*ptr < base::UTF8_BASE_ASCII_LIMIT)[[likely]]
    {
        return {*ptr, base::UTF8_BASE_BYTES_1};
    }
    return detail::decode_unsafe_slow_path_padded(ptr);
}

// 语义说明: 返回的是“窗口内最佳局部诊断”（最多回溯 4 字节），并非对整个缓冲区的诊断。
[[nodiscard]] UTF8_API base::DecodeResult decode_prev(std::span<const uint8_t> data) noexcept;
}

namespace utf8 {
[[nodiscard]] UTF8_API std::pair<base::EncodeStatus, uint8_t> encode(uint32_t cp,
                                                                     std::span<uint8_t> out_buffer) noexcept;

// 转换契约: 遇到非法字节流时返回 InvalidInput。out_length 保留失败前已成功累计的数量。
[[nodiscard]] UTF8_API base::EncodeStatus utf16_length_from_utf8(std::span<const uint8_t> utf8_data,
                                                                 size_t &out_length) noexcept;

// 转换契约: 遇到非法字节流时返回 InvalidInput。out_written_len 保留失败前已成功写入的数量。
[[nodiscard]] UTF8_API base::EncodeStatus utf8_to_utf16(std::span<const uint8_t> utf8_data,
                                                        std::span<char16_t> out_buffer,
                                                        size_t &out_written_len) noexcept;
}

namespace utf8 {
// 属性查询 (O(1) 纯函数)
[[nodiscard]] UTF8_API bool is_identifier_start(uint32_t cp) noexcept;
[[nodiscard]] UTF8_API bool is_identifier_continue(uint32_t cp) noexcept;
[[nodiscard]] UTF8_API bool is_restricted_confusable(uint32_t cp) noexcept;
[[nodiscard]] UTF8_API bool is_operator_symbol_candidate(uint32_t cp) noexcept;
[[nodiscard]] UTF8_API uint8_t display_width_approx(uint32_t cp) noexcept;
[[nodiscard]] UTF8_API uint32_t fold_case_simple(uint32_t cp) noexcept;
}

namespace utf8::utils {
// 工具函数
[[nodiscard]] UTF8_API std::span<const uint8_t> strip_bom(const std::span<const uint8_t> &data) noexcept;
[[nodiscard]] UTF8_API base::EncodeStatus to_escaped_ascii(const std::span<const uint8_t> &utf8_data,
                                                           std::span<char> out_ascii_buffer,
                                                           size_t &out_written_len) noexcept;
}
