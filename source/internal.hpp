//
// Created by aoweichen on 2026/2/27.
//

#pragma once
#include <cstddef>
#include <cstdint>

namespace utf8::internal {
inline constexpr uint32_t UTF8_INTERNAL_MAX_CODEPOINT = 0x10FFFF;
inline constexpr uint32_t UTF8_INTERNAL_SURROGATE_START = 0xD800;
inline constexpr uint32_t UTF8_INTERNAL_SURROGATE_END = 0xDFFF;

inline constexpr uint8_t UTF8_INTERNAL_PREFIX_CONTINUATION = 0x80;
inline constexpr uint8_t UTF8_INTERNAL_PREFIX_2_BYTE = 0xC0;
inline constexpr uint8_t UTF8_INTERNAL_PREFIX_3_BYTE = 0xE0;
inline constexpr uint8_t UTF8_INTERNAL_PREFIX_4_BYTE = 0xF0;
inline constexpr uint8_t UTF8_INTERNAL_CONT_PAYLOAD_MASK = 0x3F;
inline constexpr uint8_t UTF8_INTERNAL_CONT_CHECK_MASK = 0xC0;
inline constexpr uint8_t UTF8_INTERNAL_CONT_MIN = 0x80;
inline constexpr uint8_t UTF8_INTERNAL_CONT_MAX = 0xBF;

inline constexpr uint32_t UTF8_INTERNAL_ENCODE_1_BYTE_MAX = 0x007F;
inline constexpr uint32_t UTF8_INTERNAL_ENCODE_2_BYTE_MAX = 0x07FF;
inline constexpr uint32_t UTF8_INTERNAL_ENCODE_3_BYTE_MAX = 0xFFFF;

inline constexpr uint8_t UTF8_INTERNAL_SHIFT_STEP_1 = 6;
inline constexpr uint8_t UTF8_INTERNAL_SHIFT_STEP_2 = 12;
inline constexpr uint8_t UTF8_INTERNAL_SHIFT_STEP_3 = 18;

inline constexpr uint8_t UTF8_INTERNAL_BYTES_2 = 2;
inline constexpr uint8_t UTF8_INTERNAL_BYTES_3 = 3;
inline constexpr uint8_t UTF8_INTERNAL_BYTES_4 = 4;

inline constexpr uint32_t UTF8_INTERNAL_U16_SUPPLEMENTARY_MIN = 0x10000;
inline constexpr uint16_t UTF8_INTERNAL_U16_HIGH_SURROGATE = 0xD800;
inline constexpr uint16_t UTF8_INTERNAL_U16_LOW_SURROGATE = 0xDC00;
inline constexpr uint8_t UTF8_INTERNAL_U16_SHIFT = 10;
inline constexpr uint32_t UTF8_INTERNAL_U16_MASK = 0x3FF;

// --- 解码专有常量 ---
inline constexpr uint8_t UTF8_INTERNAL_DFA_ACCEPT = 0;
inline constexpr uint8_t UTF8_INTERNAL_DFA_REJECT = 12;
inline constexpr uint16_t UTF8_INTERNAL_DFA_STATE_OFFSET = 256;

inline constexpr uint8_t UTF8_INTERNAL_INVALID_PREFIX_F5 = 0xF5;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_PREFIX_C0 = 0xC0;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_PREFIX_C1 = 0xC1;
inline constexpr uint8_t UTF8_INTERNAL_SURROGATE_PREFIX = 0xED;
inline constexpr uint8_t UTF8_INTERNAL_SURROGATE_MIN = 0xA0;
inline constexpr uint8_t UTF8_INTERNAL_SURROGATE_MAX = 0xBF;
inline constexpr uint8_t UTF8_INTERNAL_OUT_OF_BOUNDS_PREFIX = 0xF4;
inline constexpr uint8_t UTF8_INTERNAL_OUT_OF_BOUNDS_MIN = 0x90;
inline constexpr uint8_t UTF8_INTERNAL_OUT_OF_BOUNDS_MAX = 0xBF;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_3_PREFIX = 0xE0;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_3_MIN = 0x80;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_3_MAX = 0x9F;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_4_PREFIX = 0xF0;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_4_MIN = 0x80;
inline constexpr uint8_t UTF8_INTERNAL_OVERLONG_4_MAX = 0x8F;

inline constexpr uint8_t UTF8_INTERNAL_LEN2_MIN = 0xC2;
inline constexpr uint8_t UTF8_INTERNAL_LEN2_MAX = 0xDF;
inline constexpr uint8_t UTF8_INTERNAL_LEN3_MIN = 0xE0;
inline constexpr uint8_t UTF8_INTERNAL_LEN3_MAX = 0xEF;
inline constexpr uint8_t UTF8_INTERNAL_LEN4_MIN = 0xF0;
inline constexpr uint8_t UTF8_INTERNAL_LEN4_MAX = 0xF4;

// --- Utils 转义专有常量 ---
inline constexpr uint8_t UTF8_INTERNAL_BOM_1 = 0xEF;
inline constexpr uint8_t UTF8_INTERNAL_BOM_2 = 0xBB;
inline constexpr uint8_t UTF8_INTERNAL_BOM_3 = 0xBF;
inline constexpr size_t UTF8_INTERNAL_BOM_LENGTH = 3;

inline constexpr uint32_t UTF8_INTERNAL_CP_TAB = 0x0009;
inline constexpr uint32_t UTF8_INTERNAL_CP_LF = 0x000A;
inline constexpr uint32_t UTF8_INTERNAL_CP_CR = 0x000D;

inline constexpr uint8_t UTF8_INTERNAL_ASCII_SPACE = 0x20;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_DOUBLE_QUOTE = 0x22;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_UPPER_U = 0x55;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_BACKSLASH = 0x5C;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_LOWER_N = 0x6E;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_LOWER_R = 0x72;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_LOWER_T = 0x74;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_LOWER_U = 0x75;
inline constexpr uint8_t UTF8_INTERNAL_ASCII_TILDE = 0x7E;

inline constexpr char UTF8_INTERNAL_HEX_CHARS[] = "0123456789ABCDEF";
inline constexpr uint8_t UTF8_INTERNAL_HEX_MASK = 0x0F;
inline constexpr uint8_t UTF8_INTERNAL_HEX_SHIFT = 4;
} // namespace utf8::internal
