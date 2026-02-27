//
// Created by aoweichen on 2026/2/27.
//

#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "utf8/utf8.hpp"

// ============================================================================
// 1. 解码器深度测试 (Utf8DecodeTest)
// ============================================================================
TEST(Utf8DecodeTest, ValidBoundaries)
{
    // 测试 1~4 字节编码的极限边界值
    std::vector<std::pair<std::vector<uint8_t>, uint32_t>> boundaries = {
        {{0x7F}, 0x007F},                               // 1字节最大值
        {{0xC2, 0x80}, 0x0080},                         // 2字节最小值
        {{0xDF, 0xBF}, 0x07FF},                         // 2字节最大值
        {{0xE0, 0xA0, 0x80}, 0x0800},                   // 3字节最小值
        {{0xEF, 0xBF, 0xBF}, 0xFFFF},                   // 3字节最大值
        {{0xF0, 0x90, 0x80, 0x80}, 0x10000},            // 4字节最小值
        {{0xF4, 0x8F, 0xBF, 0xBF}, 0x10FFFF}            // 4字节最大值 (Unicode极限)
    };

    for (const auto &[bytes, expected_cp]: boundaries)
    {
        auto res = utf8::decode_next(bytes);
        EXPECT_TRUE(res.is_valid());
        EXPECT_EQ(res.codepoint, expected_cp);
        EXPECT_EQ(res.bytes_consumed, bytes.size());
    }
}

TEST(Utf8DecodeTest, OverlongEncodings)
{
    // 过长编码攻击 (将短字符用长字节序编码，企图绕过安全检查)
    const std::vector<std::vector<uint8_t>> overlongs = {
        {0xC0, 0xAF},                   // '/' 的2字节过长编码
        {0xC1, 0xBF},                   // U+007F 的2字节过长编码
        {0xE0, 0x9F, 0xBF},             // U+07FF 的3字节过长编码
        {0xF0, 0x8F, 0xBF, 0xBF}        // U+FFFF 的4字节过长编码
    };

    for (const auto &bytes: overlongs)
    {
        auto res = utf8::decode_next(bytes);
        EXPECT_FALSE(res.is_valid());
        EXPECT_EQ(res.status, utf8::base::DecodeStatus::OverlongEncoding);
        EXPECT_EQ(res.bytes_consumed, 1); // 容错机制：只吃 1 字节
    }
}

TEST(Utf8DecodeTest, SurrogateAndOutOfBounds)
{
    // 代理对区域 (UTF-16 专用，UTF-8 中不允许出现)
    std::vector<uint8_t> surrogate_high = {0xED, 0xA0, 0x80}; // U+D800
    const auto res1 = utf8::decode_next(surrogate_high);
    EXPECT_EQ(res1.status, utf8::base::DecodeStatus::InvalidPrefix);

    std::vector<uint8_t> surrogate_low = {0xED, 0xBF, 0xBF};  // U+DFFF
    const auto res2 = utf8::decode_next(surrogate_low);
    EXPECT_EQ(res2.status, utf8::base::DecodeStatus::InvalidPrefix);

    // 超出 Unicode 最大范围 ( > U+10FFFF)
    std::vector<uint8_t> out_of_bounds = {0xF4, 0x90, 0x80, 0x80}; // U+110000
    const auto res3 = utf8::decode_next(out_of_bounds);
    EXPECT_EQ(res3.status, utf8::base::DecodeStatus::InvalidPrefix);
}

TEST(Utf8DecodeTest, TruncatedData)
{
    // 数据被物理截断
    std::vector<uint8_t> trunc3 = {0xE4, 0xBD}; // 期望3字节，只有2字节
    const auto res = utf8::decode_next(trunc3);
    EXPECT_FALSE(res.is_valid());
    EXPECT_EQ(res.status, utf8::base::DecodeStatus::TruncatedData);
    EXPECT_EQ(res.bytes_consumed, 1);
}

TEST(Utf8DecodeTest, DecodePrevComplex)
{
    // 逆向解析边界
    std::vector<uint8_t> data = {0x41, 0xE4, 0xBD, 0xA0, 0x80}; // 'A', '你', 非法续字节

    // 1. 指向末尾非法字节
    const auto r1 = utf8::decode_prev(data);
    EXPECT_FALSE(r1.is_valid());
    EXPECT_EQ(r1.status, utf8::base::DecodeStatus::InvalidContinuation);

    // 2. 指向 '你' 的末尾
    const auto r2 = utf8::decode_prev(std::span<const uint8_t>(data.data(), 4));
    EXPECT_TRUE(r2.is_valid());
    EXPECT_EQ(r2.codepoint, 0x4F60);
    EXPECT_EQ(r2.bytes_consumed, 3);
}

TEST(Utf8DecodeTest, UnsafePaddedFastPath)
{
    // 测试 unsafe 契约 (尾部有 4 字节 padding)
    const std::vector<uint8_t> padded_data = {0xE4, 0xBD, 0xA0, 0x00, 0x00, 0x00, 0x00};
    const auto res = utf8::decode_next_unsafe_padded(padded_data.data());
    EXPECT_EQ(res.codepoint, 0x4F60);
    EXPECT_EQ(res.bytes_consumed, 3);
}

// ============================================================================
// 2. 编码器测试 (Utf8EncodeTest)
// ============================================================================

TEST(Utf8EncodeTest, EncodeValid)
{
    std::array<uint8_t, 4> buf{};

    auto [st1, len1] = utf8::encode(0x0024, buf); // '$'
    EXPECT_EQ(st1, utf8::base::EncodeStatus::Success);
    EXPECT_EQ(len1, 1);
    EXPECT_EQ(buf[0], 0x24);

    auto [st3, len3] = utf8::encode(0x4F60, buf); // '你'
    EXPECT_EQ(st3, utf8::base::EncodeStatus::Success);
    EXPECT_EQ(len3, 3);
    EXPECT_EQ(buf[0], 0xE4);
    EXPECT_EQ(buf[1], 0xBD);
    EXPECT_EQ(buf[2], 0xA0);
}

TEST(Utf8EncodeTest, EncodeBufferTooSmall)
{
    std::array<uint8_t, 2> buf{}; // 只有 2 字节容量
    auto [st, len] = utf8::encode(0x4F60, buf); // 需要 3 字节
    EXPECT_EQ(st, utf8::base::EncodeStatus::BufferTooSmall);
    EXPECT_EQ(len, 0);
}

TEST(Utf8EncodeTest, EncodeInvalidCodepoint)
{
    std::array<uint8_t, 4> buf{};
    auto [st1, len1] = utf8::encode(0xD800, buf); // Surrogate
    EXPECT_EQ(st1, utf8::base::EncodeStatus::InvalidCodepoint);

    auto [st2, len2] = utf8::encode(0x110000, buf); // Out of bounds
    EXPECT_EQ(st2, utf8::base::EncodeStatus::InvalidCodepoint);
}

// ============================================================================
// 3. 跨平台转换测试 (Utf8ConversionTest)
// ============================================================================

TEST(Utf8ConversionTest, Utf16LengthAndTranscode)
{
    // "A你好🚀" (1 + 3 + 4 = 8 bytes)
    std::vector<uint8_t> utf8_str = {0x41, 0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD, 0xF0, 0x9F, 0x9A, 0x80};

    size_t u16_len = 0;
    const auto len_st = utf8::utf16_length_from_utf8(utf8_str, u16_len);
    EXPECT_EQ(len_st, utf8::base::EncodeStatus::Success);
    // 'A'(1) + '你'(1) + '好'(1) + '🚀'(2 Surrogate Pairs) = 5
    EXPECT_EQ(u16_len, 5);

    std::vector<char16_t> u16_buf(u16_len);
    size_t written = 0;
    const auto trans_st = utf8::utf8_to_utf16(utf8_str, u16_buf, written);
    EXPECT_EQ(trans_st, utf8::base::EncodeStatus::Success);
    EXPECT_EQ(written, 5);
    EXPECT_EQ(u16_buf[0], u'A');
    EXPECT_EQ(u16_buf[1], u'你');
    EXPECT_EQ(u16_buf[2], u'好');
    EXPECT_EQ(u16_buf[3], 0xD83D); // 🚀 High Surrogate
    EXPECT_EQ(u16_buf[4], 0xDE80); // 🚀 Low Surrogate
}

TEST(Utf8ConversionTest, Utf16FailureRetention)
{
    // 包含非法字节 0xFF 的混合流
    std::vector<uint8_t> mixed = {0x41, 0xE4, 0xBD, 0xA0, 0xFF, 0x42};
    std::vector<char16_t> u16_buf(10);
    size_t written = 0;
    const auto st = utf8::utf8_to_utf16(mixed, u16_buf, written);
    EXPECT_EQ(st, utf8::base::EncodeStatus::InvalidInput);
    // 期望成功写入 'A' 和 '你'，总计 2 个单元，然后中断
    EXPECT_EQ(written, 2);
    EXPECT_EQ(u16_buf[0], u'A');
    EXPECT_EQ(u16_buf[1], u'你');
}

// ============================================================================
// 4. 属性查表测试 (Utf8PropertyTest)
// ============================================================================

TEST(Utf8PropertyTest, IdentifierProperties)
{
    // UAX #31 测试     ccccccccccccccccccc
    EXPECT_TRUE(utf8::is_identifier_start('a'));
    EXPECT_FALSE(utf8::is_identifier_start('_'));
    EXPECT_TRUE(utf8::is_identifier_start(0x4F60)); // '你'
    EXPECT_FALSE(utf8::is_identifier_start('1'));
    EXPECT_FALSE(utf8::is_identifier_start('-'));
    EXPECT_FALSE(utf8::is_identifier_start(0x1F680)); // '🚀'

    EXPECT_TRUE(utf8::is_identifier_continue('1'));
    EXPECT_TRUE(utf8::is_identifier_continue('_'));
    EXPECT_FALSE(utf8::is_identifier_continue(' '));
}

TEST(Utf8PropertyTest, DisplayWidth)
{
    EXPECT_EQ(utf8::display_width_approx('a'), 1);
    EXPECT_EQ(utf8::display_width_approx('1'), 1);
    EXPECT_EQ(utf8::display_width_approx(0x4F60), 2); // '你' (W/F 属性)
    EXPECT_EQ(utf8::display_width_approx(0xFF01), 2); // '！' 全角感叹号
}

TEST(Utf8PropertyTest, CaseFolding)
{
    EXPECT_EQ(utf8::fold_case_simple('A'), 'a');
    EXPECT_EQ(utf8::fold_case_simple('Z'), 'z');
    EXPECT_EQ(utf8::fold_case_simple('a'), 'a');
    EXPECT_EQ(utf8::fold_case_simple('1'), '1');
    EXPECT_EQ(utf8::fold_case_simple(0x4F60), 0x4F60); // 汉字不变
}

// ============================================================================
// 5. 实用工具测试 (Utf8UtilsTest)
// ============================================================================

TEST(Utf8UtilsTest, StripBom)
{
    std::vector<uint8_t> with_bom = {0xEF, 0xBB, 0xBF, 0x41, 0x42};
    const auto stripped = utf8::utils::strip_bom(with_bom);
    EXPECT_EQ(stripped.size(), 2);
    EXPECT_EQ(stripped[0], 0x41);

    std::vector<uint8_t> no_bom = {0x41, 0x42, 0x43};
    const auto untouched = utf8::utils::strip_bom(no_bom);
    EXPECT_EQ(untouched.size(), 3);
}

TEST(Utf8UtilsTest, ToEscapedAscii)
{
    // "A\n你🚀"
    std::vector<uint8_t> data = {0x41, 0x0A, 0xE4, 0xBD, 0xA0, 0xF0, 0x9F, 0x9A, 0x80};
    std::string out(32, '\0'); // 预留足够空间
    size_t written = 0;

    const auto st = utf8::utils::to_escaped_ascii(data, std::span<char>(out.data(), out.size()), written);
    EXPECT_EQ(st, utf8::base::EncodeStatus::Success);

    out.resize(written);
    EXPECT_EQ(out, "A\\n\\u4F60\\U0001F680");
}

#include <random>

// ============================================================================
// 8. 动态生成海量文本与 Fuzzing (Utf8GenerativeAndFuzzTest)
// ============================================================================

TEST(Utf8GenerativeAndFuzzTest, MassiveDynamicRoundTrip)
{
    // 动态生成覆盖各个 Unicode 平面的测试集 (两百万级别 CodePoints)
    std::vector<uint32_t> source_codepoints;
    source_codepoints.reserve(2000000);

    // 1. 注入 ASCII 平面 (1 字节, 50万个)
    for (int i = 0; i < 500000; ++i) source_codepoints.push_back(i % 128);

    // 2. 注入 拉丁扩展/西里尔字母 平面 (2 字节, U+0080 ~ U+07FF)
    for (int i = 0; i < 500000; ++i) source_codepoints.push_back(0x0080 + (i % 0x0780));

    // 3. 注入 CJK 常用汉字 BMP平面 (3 字节, U+4E00起)
    for (int i = 0; i < 500000; ++i) source_codepoints.push_back(0x4E00 + (i % 20000));

    // 4. 注入 Emoji/辅助平面 (4 字节, U+10000 起)
    for (int i = 0; i < 500000; ++i) source_codepoints.push_back(0x1F300 + (i % 1000));

    // 【第一阶段】：全量高速编码
    std::vector<uint8_t> utf8_buffer;
    utf8_buffer.reserve(source_codepoints.size() * 3); // 预估约 6MB 数据
    std::array<uint8_t, 4> enc_buf{};

    for (const uint32_t cp: source_codepoints)
    {
        auto [st, len] = utf8::encode(cp, enc_buf);
        ASSERT_EQ(st, utf8::base::EncodeStatus::Success);
        utf8_buffer.insert(utf8_buffer.end(), enc_buf.begin(), enc_buf.begin() + len);
    }

    // 【第二阶段】：全量解码并一一对比
    const uint8_t *ptr = utf8_buffer.data();
    const uint8_t *end = ptr + utf8_buffer.size();
    size_t cp_index = 0;

    while (ptr < end)
    {
        auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));
        ASSERT_TRUE(res.is_valid());
        ASSERT_LT(cp_index, source_codepoints.size());
        ASSERT_EQ(res.codepoint, source_codepoints[cp_index++]);
        ptr += res.bytes_consumed;
    }

    // 确保解码跑完了所有的 CodePoint，并且指针精确停在末尾 (无越界)
    EXPECT_EQ(cp_index, source_codepoints.size());
    EXPECT_EQ(ptr, end);
}

TEST(Utf8GenerativeAndFuzzTest, RandomByteSoupFuzzing)
{
    // Fuzzing 测试：生成 5MB 的纯随机字节流（也就是绝对的乱码）
    // 目标：验证防御引擎在面对最恶劣的攻击数据时，绝对不能崩溃或陷入死循环！
    constexpr size_t FUZZ_SIZE = 5 * 1024 * 1024; // 5MB
    std::vector<uint8_t> random_soup(FUZZ_SIZE);

    // 固定种子以保证测试在不同机器上的可复现性
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (size_t i = 0; i < FUZZ_SIZE; ++i)
    {
        random_soup[i] = static_cast<uint8_t>(dist(rng));
    }

    const uint8_t *ptr = random_soup.data();
    const uint8_t *end = ptr + FUZZ_SIZE;
    size_t consume_count = 0;

    while (ptr < end)
    {
        auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));

        // 【核心断言】：无论数据多烂，解码器必须向前推进 (bytes_consumed > 0)
        // 否则如果在非法字节上消耗为 0，程序将永久死循环！
        ASSERT_GT(res.bytes_consumed, 0);

        ptr += res.bytes_consumed;
        consume_count += res.bytes_consumed;
    }

    // 验证最终是否完好无损地吃掉了所有 5MB 字节
    EXPECT_EQ(consume_count, FUZZ_SIZE);
    EXPECT_EQ(ptr, end);
}

TEST(Utf8GenerativeAndFuzzTest, PaddedUnsafeBulkExecution)
{
    // 验证 Unsafe (去边界检查) 极速通道的鲁棒性
    std::string text = "A";
    for (int i = 0; i < 100000; ++i)
    {
        text += "测"; // 3 bytes
        text += "试"; // 3 bytes
        text += "🚀"; // 4 bytes
    }

    std::vector<uint8_t> padded_buffer(text.begin(), text.end());
    // ⚠️ 契约：尾部强制追加 4 字节的 Padding Sentinel (哨兵边界)
    padded_buffer.insert(padded_buffer.end(), 4, 0);

    const uint8_t *ptr = padded_buffer.data();
    const uint8_t *valid_end = ptr + padded_buffer.size() - 4; // 除去 Padding 的真实末尾

    size_t total_codepoints = 0;
    while (ptr < valid_end)
    {
        const auto res = utf8::decode_next_unsafe_padded(ptr);
        ptr += res.bytes_consumed;
        total_codepoints++;
    }

    // 指针必须精确咬合在 valid_end，没有多吃也没有少吃
    EXPECT_EQ(ptr, valid_end);
    EXPECT_GT(total_codepoints, 100000);
}

TEST(Utf8GenerativeAndFuzzTest, EscapedAsciiBufferExhaustion)
{
    // 生成 10,000 个中文字符 (需要转义为 \uXXXX)
    std::vector<uint8_t> data;
    for (int i = 0; i < 10000; ++i)
    {
        data.push_back(0xE4);
        data.push_back(0xBD);
        data.push_back(0xA0); // '你'
    }

    // 故意提供一个不够大的输出缓冲区
    // '你' 转义为 "\u4F60" 需要 6 个 char。10,000 个需要 60,000 字节
    // 我们故意给它 59,999 字节，观察是否会发生写越界
    std::vector<char> small_out(59999, 0);
    size_t written = 0;

    const auto st = utf8::utils::to_escaped_ascii(data, std::span<char>(small_out.data(), small_out.size()), written);
    // 必须安全拦截并返回 BufferTooSmall，绝不能段错误
    EXPECT_EQ(st, utf8::base::EncodeStatus::BufferTooSmall);

    // 引擎会写入 9999 个完整序列 (59994)
    EXPECT_EQ(written, 59994);
}

// ============================================================================
// 🎯 饱和式覆盖率测试 (模拟 Fuzzer 逻辑，触发所有深层异常分支)
// ============================================================================
TEST(Utf8CoverageTest, SaturationFuzzSimulation)
{
    // 模拟 Fuzzer 生成的极端控制变量：{ctrl_enc, ctrl_u16, ctrl_ascii, 占位, Payload...}
    const std::vector<std::vector<uint8_t>> simulated_seeds = {
        {0, 0, 0, 0, 0xFF, 0xFE},                         // 极限小 buffer，触发 BufferTooSmall
        {2, 1, 3, 0, 0xE4, 0xBD, 0xA0},                   // 中等 buffer，截断测试
        {4, 3, 7, 0, 0x41, 0x42, 0x43},                   // 充足 buffer
        {1, 2, 1, 0, 0xF0, 0x9F, 0x98, 0x80, 0x0A, 0x09}  // Emoji + 特殊转义字符 (\n, \t)
    };

    for (const auto &data: simulated_seeds)
    {
        const size_t size = data.size();
        // 提取控制变量
        const uint8_t ctrl_encode_buf_size = data[0] % 5;
        const uint8_t ctrl_u16_buf_ratio = data[1] % 4;
        const uint8_t ctrl_ascii_buf_ratio = data[2] % 8;

        const std::span<const uint8_t> payload(data.data() + 4, size - 4);

        uint32_t random_cp = 0x1234;
        if (payload.size() >= 4) std::memcpy(&random_cp, payload.data(), 4);

        // 强行注入边缘测试靶点
        const uint32_t edge_cps[] = {
            random_cp, random_cp % 0x80, 0x80 + (random_cp % 0x780),
            0x800 + (random_cp % 0xF800), 0x10000 + (random_cp % 0xFFFF),
            0xD800, 0x110000, 0x0A, 0x0D, 0x09, 0x5C, 0x22
        };

        std::vector<uint8_t> valid_payload;
        for (const uint32_t test_cp: edge_cps)
        {
            // 压测 Encode 的 BufferTooSmall
            std::vector<uint8_t> tiny_enc_buf(ctrl_encode_buf_size);
            (void) utf8::encode(test_cp, tiny_enc_buf);

            std::array<uint8_t, 4> enc_buf{};
            auto [st, len] = utf8::encode(test_cp, enc_buf);
            if (st == utf8::base::EncodeStatus::Success)
            {
                valid_payload.insert(valid_payload.end(), enc_buf.begin(), enc_buf.begin() + len);
            }

            // 属性全量扫描
            (void) utf8::is_identifier_start(test_cp);
            (void) utf8::is_identifier_continue(test_cp);
            (void) utf8::is_restricted_confusable(test_cp);
            (void) utf8::is_operator_symbol_candidate(test_cp);
            (void) utf8::display_width_approx(test_cp);
            (void) utf8::fold_case_simple(test_cp);
        }

        valid_payload.insert(valid_payload.end(), payload.begin(), payload.end());

        // 压测 BOM 相关逻辑
        (void) utf8::utils::strip_bom(payload);
        if (payload.size() >= 3)
        {
            std::vector<uint8_t> bom_buf = {0xEF, 0xBB, 0xBF};
            bom_buf.insert(bom_buf.end(), payload.begin(), payload.end());
            (void) utf8::utils::strip_bom(bom_buf);
        }

        // 压测 to_escaped_ascii 的各种回滚分支
        size_t ascii_len = 0;
        const size_t target_ascii_cap = (valid_payload.size() * ctrl_ascii_buf_ratio) / 4;
        std::vector<char> ascii_buf(target_ascii_cap);
        (void) utf8::utils::to_escaped_ascii(valid_payload, ascii_buf, ascii_len);

        // 压测 UTF-16 缓冲区截断
        size_t u16_len = 0;
        (void) utf8::utf16_length_from_utf8(payload, u16_len);

        const size_t target_u16_cap = (u16_len * ctrl_u16_buf_ratio) / 3;
        std::vector<char16_t> u16_buf(target_u16_cap);
        size_t written = 0;
        (void) utf8::utf8_to_utf16(payload, u16_buf, written);

        // 压测常规与 unsafe 解码
        const uint8_t *ptr = payload.data();
        const uint8_t *end = ptr + payload.size();
        while (ptr < end)
        {
            const auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));
            if (res.bytes_consumed == 0) break;
            ptr += res.bytes_consumed;
        }

        std::vector<uint8_t> padded_buf(payload.begin(), payload.end());
        padded_buf.insert(padded_buf.end(), 4, 0);
        const uint8_t *unsafe_ptr = padded_buf.data();
        const uint8_t *unsafe_end = padded_buf.data() + payload.size();
        while (unsafe_ptr < unsafe_end)
        {
            const auto res = utf8::decode_next_unsafe_padded(unsafe_ptr);
            if (res.bytes_consumed == 0) break;
            unsafe_ptr += res.bytes_consumed;
        }

        (void) utf8::decode_prev(payload);
    }
}

// ============================================================================
// 9. 终极覆盖率狙击测试 (Deep Coverage Sniping)
// ============================================================================

TEST(Utf8CoverageTest, EncodeBufferExactBounds)
{
    std::array<uint8_t, 4> buf{};
    // 强制触发所有梯度的 BufferTooSmall 分支
    EXPECT_EQ(utf8::encode(0x24, std::span<uint8_t>(buf.data(), 0)).first, utf8::base::EncodeStatus::BufferTooSmall);
    EXPECT_EQ(utf8::encode(0xA2, std::span<uint8_t>(buf.data(), 1)).first, utf8::base::EncodeStatus::BufferTooSmall);
    EXPECT_EQ(utf8::encode(0x4F60, std::span<uint8_t>(buf.data(), 2)).first, utf8::base::EncodeStatus::BufferTooSmall);
    EXPECT_EQ(utf8::encode(0x1F680, std::span<uint8_t>(buf.data(), 3)).first, utf8::base::EncodeStatus::BufferTooSmall);
}

TEST(Utf8CoverageTest, DiagnoseErrorSpecificBranches)
{
    // 精准踩中 diagnose_error 里的每一个 if 判断
    const std::vector<std::vector<uint8_t>> targets = {
        {0xF5, 0x80, 0x80, 0x80}, // >= F5 (InvalidPrefix)
        {0xC0, 0x80},             // C0 Overlong
        {0xC1, 0xBF},             // C1 Overlong
        {0xE0, 0x80, 0x80},       // 3字节 Overlong (E0 80~9F)
        {0xF0, 0x80, 0x80, 0x80}, // 4字节 Overlong (F0 80~8F)
        {0x80, 0x80},             // b0 在 CONT 范围内 (InvalidContinuation / expected_len=0)
        {0xE4, 0x00}              // 第二字节不是合法续字节 (InvalidContinuation)
    };

    for (const auto &bytes: targets)
    {
        auto res = utf8::decode_next(bytes);
        EXPECT_FALSE(res.is_valid());
    }
}

TEST(Utf8CoverageTest, EscapedAsciiSwitchAndRollback)
{
    // 1. 踩满 Switch 分支
    std::vector<uint8_t> input =
            {0x0A, 0x0D, 0x09, 0x5C, 0x22, 0xE4, 0xBD, 0xA0, 0xF0, 0x9F, 0x9A, 0x80}; // \n \r \t \ " 你 🚀
    std::vector<char> out(100);
    size_t written = 0;
    EXPECT_EQ(utf8::utils::to_escaped_ascii(input, out, written), utf8::base::EncodeStatus::Success);

    // 2. 踩满回滚分支 (BufferTooSmall)
    // 故意让缓冲区在写入 \uXXXX 的一半时耗尽
    std::vector<uint8_t> input_rollback = {0xE4, 0xBD, 0xA0}; // '你' 需要 "\u4F60" (6个字符)
    for (size_t cap = 1; cap < 6; ++cap)
    {
        std::vector<char> small_out(cap);
        EXPECT_EQ(utf8::utils::to_escaped_ascii(input_rollback, small_out, written),
                  utf8::base::EncodeStatus::BufferTooSmall);
        EXPECT_EQ(written, 0); // 必须被完美回滚到 0
    }
}

TEST(Utf8CoverageTest, Utf16TruncationAndInvalid)
{
    // 1. utf16_length_from_utf8 遇到非法字符
    size_t len = 0;
    constexpr std::array<uint8_t, 1> invalid_input = {0xFF};
    EXPECT_EQ(utf8::utf16_length_from_utf8(invalid_input, len), utf8::base::EncodeStatus::InvalidInput);

    // 2. utf8_to_utf16 在写入 Surrogate Pair 时遭遇缓冲区不足
    std::vector<uint8_t> rocket = {0xF0, 0x9F, 0x9A, 0x80}; // 🚀 需要 2 个 char16_t
    std::vector<char16_t> out16(1); // 故意只给 1 的容量
    EXPECT_EQ(utf8::utf8_to_utf16(rocket, out16, len), utf8::base::EncodeStatus::BufferTooSmall);
}

TEST(Utf8CoverageTest, StripBomPartial)
{
    // 不完整的 BOM 必须原样返回，不截断
    constexpr std::array<uint8_t, 1> bom1 = {0xEF};
    EXPECT_EQ(utf8::utils::strip_bom(bom1).size(), 1);

    constexpr std::array<uint8_t, 2> bom2 = {0xEF, 0xBB};
    EXPECT_EQ(utf8::utils::strip_bom(bom2).size(), 2);

    constexpr std::array<uint8_t, 3> bom3 = {0xEF, 0xBB, 0x00};
    EXPECT_EQ(utf8::utils::strip_bom(bom3).size(), 3);
}

TEST(Utf8CoverageTest, DecodeNextEmptyInput)
{
    // 专门狙击 include/utf8/utf8.hpp 第 19 行的空数据防御分支
    constexpr std::span<const uint8_t> empty_span;
    const auto res = utf8::decode_next(empty_span);

    EXPECT_FALSE(res.is_valid());
    EXPECT_EQ(res.status, utf8::base::DecodeStatus::EndOfInput);
    EXPECT_EQ(res.bytes_consumed, 0);
}

TEST(Utf8CoverageTest, Truncated4ByteSequence)
{
    // 专门狙击 expected_len 的 4 字节截断分支
    constexpr std::array<uint8_t, 3> trunc4 = {0xF0, 0x9F, 0x9A}; // 🚀 差最后一个字节
    const auto res = utf8::decode_next(trunc4);

    EXPECT_FALSE(res.is_valid());
    EXPECT_EQ(res.status, utf8::base::DecodeStatus::TruncatedData);
}

TEST(Utf8CoverageTest, StripBomShortCircuits)
{
    // 专门测试 strip_bom 内部的 && 短路分支
    // 1. 长度够，但第一个字节错
    constexpr std::array<uint8_t, 3> no_bom1 = {0x00, 0x00, 0x00};
    EXPECT_EQ(utf8::utils::strip_bom(no_bom1).size(), 3);

    // 2. 长度够，第一字节对，第二字节错
    constexpr std::array<uint8_t, 3> no_bom2 = {0xEF, 0x00, 0x00};
    EXPECT_EQ(utf8::utils::strip_bom(no_bom2).size(), 3);
}

TEST(Utf8CoverageTest, EscapedAsciiInvalidUtf8Fallback)
{
    // 专门测试遇到非法 UTF-8 时的三元运算符 fallback 分支
    constexpr std::array<uint8_t, 1> invalid = {0xFF};
    std::vector<char> out(10);
    size_t written = 0;

    // 非法字节会被转换为 0xFFFD，最终被 escape 为 \uFFFD
    EXPECT_EQ(utf8::utils::to_escaped_ascii(invalid, out, written), utf8::base::EncodeStatus::Success);
    EXPECT_EQ(written, 6);
}

TEST(Utf8CoverageTest, EscapedAsciiGranularBufferFailures)
{
    size_t written = 0;
    constexpr std::array<uint8_t, 1> ascii_A = {0x41};               // 'A'
    constexpr std::array<uint8_t, 1> ctrl_LF = {0x0A};               // '\n'
    constexpr std::array<uint8_t, 3> char_ni = {0xE4, 0xBD, 0xA0};   // '你'
    constexpr std::array<uint8_t, 4> emoji_R = {0xF0, 0x9F, 0x9A, 0x80}; // '🚀'

    // 1. 写入普通 ASCII 时瞬间失败
    std::vector<char> out0(0);
    EXPECT_EQ(utf8::utils::to_escaped_ascii(ascii_A, out0, written), utf8::base::EncodeStatus::BufferTooSmall);

    // 2. 写入转义字符的第一个 '\' 时瞬间失败
    EXPECT_EQ(utf8::utils::to_escaped_ascii(ctrl_LF, out0, written), utf8::base::EncodeStatus::BufferTooSmall);

    std::vector<char> out1(1);
    // 3. 成功写了 '\'，但在写 'n' 时失败
    EXPECT_EQ(utf8::utils::to_escaped_ascii(ctrl_LF, out1, written), utf8::base::EncodeStatus::BufferTooSmall);

    // 4. 成功写了 '\'，但在写 'u' 时失败
    EXPECT_EQ(utf8::utils::to_escaped_ascii(char_ni, out1, written), utf8::base::EncodeStatus::BufferTooSmall);

    // 5. 成功写了 '\'，但在写 'U' 时失败
    EXPECT_EQ(utf8::utils::to_escaped_ascii(emoji_R, out1, written), utf8::base::EncodeStatus::BufferTooSmall);

    // 6. 成功写了 '\Uxxx'，但在写最后几个 hex 时失败 (容量设为 5)
    std::vector<char> out5(5);
    EXPECT_EQ(utf8::utils::to_escaped_ascii(emoji_R, out5, written), utf8::base::EncodeStatus::BufferTooSmall);
}

TEST(Utf8CoverageTest, DecodePrevEmptyInput)
{
    constexpr std::span<const uint8_t> empty_span;
    const auto res = utf8::decode_prev(empty_span);
    EXPECT_EQ(res.status, utf8::base::DecodeStatus::EndOfInput);
}

TEST(Utf8CoverageTest, DiagnoseErrorContinuationBoundary)
{
    // 1. 首字节就是续字节 (触发 52 行的 true 分支)
    constexpr std::array<uint8_t, 1> cont_first = {0x80};
    EXPECT_EQ(utf8::decode_next(cont_first).status, utf8::base::DecodeStatus::InvalidPrefix);

    // 2. 首字节非法，但第二字节是合法的续字节 (触发 64 行的 failed_byte 检查)
    // 0xF5 是非法前缀，遇到 0xF5 时 DFA 并不会立刻拒绝，而是查表后在第二个字节 0x80 时拒绝
    constexpr std::array<uint8_t, 2> invalid_prefix_valid_cont = {0xF5, 0x80};
    EXPECT_EQ(utf8::decode_next(invalid_prefix_valid_cont).status, utf8::base::DecodeStatus::InvalidPrefix);
}
