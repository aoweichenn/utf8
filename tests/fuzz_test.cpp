// tests/fuzz_test.cpp
#include <array>
#include <cstring>
#include <span>
#include <vector>
#include <utf8/utf8.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
    // 需要至少 4 个字节作为我们的“控制变量”
    if (size < 4) [[unlikely]] return 0;

    // ========================================================================
    // 🌟 核心技巧 1：从 Fuzzer 数据中提取“控制变量”，用于故意制造边界情况
    // ========================================================================
    const uint8_t ctrl_encode_buf_size = data[0] % 5;  // 值域: 0, 1, 2, 3, 4 (用于引发 BufferTooSmall)
    const uint8_t ctrl_u16_buf_ratio = data[1] % 4;  // 值域: 0, 1, 2, 3 (用于动态截断 UTF-16 缓冲)
    const uint8_t ctrl_ascii_buf_ratio = data[2] % 8;  // 用于动态截断 ASCII 缓冲，触发写回滚机制

    // 剩下的数据才是真正的 Payload
    const std::span<const uint8_t> payload(data + 4, size - 4);

    uint32_t random_cp = 0;
    if (payload.size() >= 4)
    {
        std::memcpy(&random_cp, payload.data(), 4);
    }
    else
    {
        random_cp = static_cast<uint32_t>(size * 1000);
    }

    // ========================================================================
    // 🌟 核心技巧 2：人工靶点注入 (保证覆盖特定分支)
    // ========================================================================
    const uint32_t edge_cps[] = {
        random_cp,                             // 纯随机
        random_cp % 0x80,                      // 强制 1 字节 (ASCII)
        0x80 + (random_cp % 0x780),            // 强制 2 字节
        0x800 + (random_cp % 0xF800),          // 强制 3 字节
        0x10000 + (random_cp % 0xFFFF),        // 强制 4 字节 (Supplementary)
        0xD800,                                // 代理区 (Surrogate - 非法)
        0x110000,                              // 超出最大码位 (Out of bounds - 非法)
        0x0A, 0x0D, 0x09, 0x5C, 0x22           // 特殊转义字符 (\n, \r, \t, \\, ")
    };

    // 构建一个保证包含这些极端的有效 UTF-8 字节流
    std::vector<uint8_t> valid_payload;
    for (const uint32_t test_cp: edge_cps)
    {
        // 1. 压测 encode 的 BufferTooSmall 异常分支
        std::vector<uint8_t> tiny_enc_buf(ctrl_encode_buf_size);
        (void) utf8::encode(test_cp, tiny_enc_buf);

        // 2. 正常 encode 生成有效数据，混入有效载荷
        std::array<uint8_t, 4> enc_buf{};
        auto [st, len] = utf8::encode(test_cp, enc_buf);
        if (st == utf8::base::EncodeStatus::Success)
        {
            valid_payload.insert(valid_payload.end(), enc_buf.begin(), enc_buf.begin() + len);
        }

        // 3. 属性查表边界压测
        (void) utf8::is_identifier_start(test_cp);
        (void) utf8::is_identifier_continue(test_cp);
        (void) utf8::is_restricted_confusable(test_cp);
        (void) utf8::is_operator_symbol_candidate(test_cp);
        (void) utf8::display_width_approx(test_cp);
        (void) utf8::fold_case_simple(test_cp);
    }

    // 将 Fuzzer 的纯随机数据拼接到有效数据后面，形成混合测试流
    valid_payload.insert(valid_payload.end(), payload.begin(), payload.end());

    // ========================================================================
    // 🌟 核心技巧 3：压测 BOM 与 Escaped ASCII 分支
    // ========================================================================
    (void) utf8::utils::strip_bom(payload);

    // 强行构造一个带有 BOM 的流来确保分支覆盖
    if (payload.size() >= 3)
    {
        std::vector<uint8_t> bom_buf = {0xEF, 0xBB, 0xBF};
        bom_buf.insert(bom_buf.end(), payload.begin(), payload.end());
        (void) utf8::utils::strip_bom(bom_buf);
    }

    // 压测 to_escaped_ascii 的回滚逻辑 (BufferTooSmall)
    size_t ascii_len = 0;
    size_t target_ascii_cap = (valid_payload.size() * ctrl_ascii_buf_ratio) / 4;
    std::vector<char> ascii_buf(target_ascii_cap);
    (void) utf8::utils::to_escaped_ascii(valid_payload, ascii_buf, ascii_len);

    // ========================================================================
    // 🌟 核心技巧 4：转码 API 的极限缓冲区压测
    // ========================================================================
    size_t u16_len = 0;
    (void) utf8::utf16_length_from_utf8(payload, u16_len);

    // 故意提供不足的 UTF-16 缓冲区，压测内部的越界保护 (out_written_len >= out_buffer.size)
    size_t target_u16_cap = (u16_len * ctrl_u16_buf_ratio) / 3;
    std::vector<char16_t> u16_buf(target_u16_cap);
    size_t written = 0;
    (void) utf8::utf8_to_utf16(payload, u16_buf, written);

    // ========================================================================
    // 🌟 核心技巧 5：全量解码与不变式校验
    // ========================================================================
    const uint8_t *ptr = payload.data();
    const uint8_t *end = ptr + payload.size();

    while (ptr < end)
    {
        auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));
        if (res.bytes_consumed == 0) [[unlikely]] __builtin_trap();
        ptr += res.bytes_consumed;
    }

    // Unsafe Padded 解码压测
    static std::vector<uint8_t> padded_buf;
    padded_buf.assign(payload.begin(), payload.end());
    padded_buf.insert(padded_buf.end(), 4, 0);
    const uint8_t *unsafe_ptr = padded_buf.data();
    const uint8_t *unsafe_end = padded_buf.data() + payload.size();

    while (unsafe_ptr < unsafe_end)
    {
        auto res = utf8::decode_next_unsafe_padded(unsafe_ptr);
        if (res.bytes_consumed == 0) [[unlikely]] __builtin_trap();
        unsafe_ptr += res.bytes_consumed;
    }

    // 逆向回溯压测
    (void) utf8::decode_prev(payload);

    return 0;
}
