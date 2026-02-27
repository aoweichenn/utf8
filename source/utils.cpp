//
// Created by aoweichen on 2026/2/27.
//
#include "internal.hpp"
#include <utf8/utf8.hpp>

namespace utf8::utils {
namespace {
using namespace utf8::internal;
}

[[nodiscard]] UTF8_API std::span<const uint8_t> strip_bom(const std::span<const uint8_t> &data) noexcept
{
    if (data.size() >= UTF8_INTERNAL_BOM_LENGTH && data[0] == UTF8_INTERNAL_BOM_1 && data[1] == UTF8_INTERNAL_BOM_2 &&
        data[2] == UTF8_INTERNAL_BOM_3) [[unlikely]]
    {
        return data.subspan(UTF8_INTERNAL_BOM_LENGTH);
    }
    return data;
}

[[nodiscard]] UTF8_API base::EncodeStatus to_escaped_ascii(const std::span<const uint8_t> &utf8_data,
                                                           std::span<char> out_ascii_buffer,
                                                           size_t &out_written_len) noexcept
{
    out_written_len = 0;
    const uint8_t *ptr = utf8_data.data();
    const uint8_t *end = ptr + utf8_data.size();

    auto append_char = [&](const char c) noexcept -> bool {
        if (out_written_len >= out_ascii_buffer.size())
        {
            return false;
        }
        out_ascii_buffer[out_written_len++] = c;
        return true;
    };

    auto append_hex = [&](uint32_t val, const int pad_len) noexcept -> bool {
        const auto need = static_cast<size_t>(pad_len);
        if (out_written_len + need > out_ascii_buffer.size())
        {
            return false;
        }
        for (int i = pad_len - 1; i >= 0; --i)
        {
            out_ascii_buffer[out_written_len + static_cast<size_t>(i)] =
                    UTF8_INTERNAL_HEX_CHARS[val & UTF8_INTERNAL_HEX_MASK];
            val >>= UTF8_INTERNAL_HEX_SHIFT;
        }
        out_written_len += need;
        return true;
    };

    while (ptr < end)
    {
        auto res = decode_next(std::span<const uint8_t>(ptr, end));
        const uint32_t cp = res.is_valid() ? res.codepoint : base::UTF8_BASE_REPLACEMENT_CHAR;

        // 🌟 核心：打下事务快照点
        const size_t snapshot_len = out_written_len;
        bool write_success{};

        if (cp >= UTF8_INTERNAL_ASCII_SPACE && cp <= UTF8_INTERNAL_ASCII_TILDE &&
            cp != UTF8_INTERNAL_ASCII_BACKSLASH && cp != UTF8_INTERNAL_ASCII_DOUBLE_QUOTE) [[likely]]
        {
            write_success = append_char(static_cast<char>(cp));
        }
        else
        {
            write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_BACKSLASH));

            if (write_success)
            {
                switch (cp)
                {
                    case UTF8_INTERNAL_CP_LF: {
                        write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_LOWER_N));
                        break;
                    }
                    case UTF8_INTERNAL_CP_CR: {
                        write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_LOWER_R));
                        break;
                    }
                    case UTF8_INTERNAL_CP_TAB: {
                        write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_LOWER_T));
                        break;
                    }
                    case UTF8_INTERNAL_ASCII_BACKSLASH: {
                        write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_BACKSLASH));
                        break;
                    }
                    case UTF8_INTERNAL_ASCII_DOUBLE_QUOTE: {
                        write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_DOUBLE_QUOTE));
                        break;
                    }
                    default: {
                        if (cp <= UTF8_INTERNAL_ENCODE_3_BYTE_MAX)
                        {
                            write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_LOWER_U)) && append_hex(
                                                cp, 4);
                        }
                        else
                        {
                            write_success = append_char(static_cast<char>(UTF8_INTERNAL_ASCII_UPPER_U)) && append_hex(
                                                cp, 8);
                        }
                        break;
                    }
                }
            }
        }

        // 如果写入失败（空间不足），执行回滚
        if (!write_success)
        {
            out_written_len = snapshot_len;
            return base::EncodeStatus::BufferTooSmall;
        }

        // 只有完整且安全地写完了这个 CodePoint，才推进源数据读取指针
        ptr += res.bytes_consumed;
    }
    return base::EncodeStatus::Success;
}
} // namespace utf8::utils
