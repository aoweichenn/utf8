//
// Created by aoweichen on 2026/2/27.
//
//
// Created by aoweichen on 2026/2/27.
//
// benchmarks/bench_core.cpp
#include <benchmark/benchmark.h>
#include <utf8/utf8.hpp>
#include <string>
#include <vector>
#include <random>

// ============================================================================
// 测试数据集生成器
// ============================================================================
std::string generate_ascii_string(const size_t length)
{
    std::string s(length, '\0');
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(32, 126);
    for (size_t i = 0; i < length; ++i) s[i] = static_cast<char>(dist(rng));
    return s;
}

std::string generate_mixed_string(size_t length)
{
    std::string s;
    s.reserve(length + 4); // 预留一点空间防末尾溢出

    std::mt19937 rng(42); // 固定种子
    std::uniform_int_distribution<int> prob_dist(0, 99);

    // 各个字节长度平面的 CodePoint 分布
    std::uniform_int_distribution<uint32_t> ascii_dist(32, 126);
    std::uniform_int_distribution<uint32_t> two_byte_dist(0x0080, 0x07FF);
    std::uniform_int_distribution<uint32_t> three_byte_dist(0x0800, 0xFFFF);
    std::uniform_int_distribution<uint32_t> four_byte_dist(0x10000, 0x10FFFF);

    std::array<uint8_t, 4> enc_buf{};

    while (s.size() < length)
    {
        int p = prob_dist(rng);
        uint32_t cp = 0;

        if (p < 60)
        {
            cp = ascii_dist(rng); // 60% 概率生成 1 字节 (ASCII)
        }
        else if (p < 75)
        {
            cp = two_byte_dist(rng); // 15% 概率生成 2 字节
        }
        else if (p < 95)
        {
            cp = three_byte_dist(rng); // 20% 概率生成 3 字节 (CJK)
            // 避开 UTF-16 代理区 (Surrogate Pair) 否则会编码失败
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0x4F60;
        }
        else
        {
            cp = four_byte_dist(rng); // 5% 概率生成 4 字节 (Emoji 等)
        }

        auto [st, len] = utf8::encode(cp, enc_buf);
        if (st == utf8::base::EncodeStatus::Success)
        {
            s.append(reinterpret_cast<const char *>(enc_buf.data()), len);
        }
    }

    // 精确截断到要求的字节数（可能会截断最后一个多字节字符，这正好测试我们的防截断能力！）
    s.resize(length);
    return s;
}

// ============================================================================
// 1. 解码器吞吐量压测 (MB/s)
// ============================================================================

static void BM_Decode_PureASCII(benchmark::State &state)
{
    const std::string data = generate_ascii_string(state.range(0));
    const std::span<const uint8_t> input(reinterpret_cast<const uint8_t *>(data.data()), data.size());

    for (auto _: state)
    {
        const uint8_t *ptr = input.data();
        const uint8_t *end = ptr + input.size();
        while (ptr < end)
        {
            auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));
            benchmark::DoNotOptimize(res); // 防止编译器把循环优化掉
            ptr += res.bytes_consumed;
        }
    }
    // 自动计算并打印 MB/s 吞吐量
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * data.size());
}

BENCHMARK(BM_Decode_PureASCII)->Range(8, 8 << 16); // 测试从 8B 到 512KB

static void BM_Decode_Mixed(benchmark::State &state)
{
    const std::string data = generate_mixed_string(state.range(0));
    const std::span<const uint8_t> input(reinterpret_cast<const uint8_t *>(data.data()), data.size());

    for (auto _: state)
    {
        const uint8_t *ptr = input.data();
        const uint8_t *end = ptr + input.size();
        while (ptr < end)
        {
            auto res = utf8::decode_next(std::span<const uint8_t>(ptr, end));
            benchmark::DoNotOptimize(res);
            ptr += res.bytes_consumed;
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * data.size());
}

BENCHMARK(BM_Decode_Mixed)->Range(8, 8 << 16);

static void BM_Decode_Unsafe_Padded(benchmark::State &state)
{
    std::string data = generate_mixed_string(state.range(0));
    data.append(4, '\0'); // 强制 4 字节 Padding 哨兵
    const uint8_t *start = reinterpret_cast<const uint8_t *>(data.data());
    const uint8_t *end = start + data.size() - 4;

    for (auto _: state)
    {
        const uint8_t *ptr = start;
        while (ptr < end)
        {
            auto res = utf8::decode_next_unsafe_padded(ptr);
            benchmark::DoNotOptimize(res);
            ptr += res.bytes_consumed;
        }
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * (data.size() - 4));
}

BENCHMARK(BM_Decode_Unsafe_Padded)->Range(8, 8 << 16);

// ============================================================================
// 2. 跨平台转码压测 (UTF-8 -> UTF-16)
// ============================================================================

// ============================================================================
// 2. 跨平台转码压测 (UTF-8 -> UTF-16)
// ============================================================================

static void BM_Transcode_Utf8_To_Utf16(benchmark::State &state)
{
    const std::string data = generate_mixed_string(state.range(0));
    const std::span<const uint8_t> input(reinterpret_cast<const uint8_t *>(data.data()), data.size());

    size_t u16_len = 0;
    // 🌟 接住返回值，消除警告，并防止被编译器优化
    auto len_status = utf8::utf16_length_from_utf8(input, u16_len);
    benchmark::DoNotOptimize(len_status);

    std::vector<char16_t> out_buf(u16_len);

    for (auto _: state)
    {
        size_t written = 0;
        // 🌟 同样接住返回值
        auto trans_status = utf8::utf8_to_utf16(input, out_buf, written);
        benchmark::DoNotOptimize(trans_status);
        benchmark::DoNotOptimize(out_buf);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * data.size());
}

BENCHMARK(BM_Transcode_Utf8_To_Utf16)->Range(8, 8 << 16);

BENCHMARK(BM_Transcode_Utf8_To_Utf16)->Range(8, 8 << 16);

// ============================================================================
// 3. 属性查表 API 极速压测 (O(1) 性能探测)
// ============================================================================

static void BM_Property_IdentifierStart(benchmark::State &state)
{
    constexpr uint32_t cp = 0x4F60; // '你'
    for (auto _: state)
    {
        bool b = utf8::is_identifier_start(cp);
        benchmark::DoNotOptimize(b);
    }
}

BENCHMARK(BM_Property_IdentifierStart);

BENCHMARK_MAIN();
