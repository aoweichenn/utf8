#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import hashlib
import json
import os
import re
import sys
import time
import urllib.request
from io import StringIO
from array import array
from functools import wraps
from typing import Dict, List, Tuple, Optional
from concurrent.futures import ThreadPoolExecutor, as_completed

# Unicode 最大码点（U+0000 到 U+10FFFF）
MAX_CODEPOINT = 0x10FFFF
# 压缩时的块大小（每 256 个码点为一个块）
BLOCK_SIZE = 256
# 网络下载超时时间（秒）
TIMEOUT_SEC = 15
# 最大重试次数
MAX_RETRIES = 3
# 重试间隔（秒）
RETRY_DELAY = 1


def retry(max_retries: int = MAX_RETRIES, delay: float = RETRY_DELAY):
    """
    重试装饰器：用于网络请求等可能失败的操作
    """
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            retries = 0
            while retries < max_retries:
                try:
                    return func(*args, **kwargs)
                except (urllib.request.URLError, urllib.request.HTTPError, TimeoutError) as e:
                    retries += 1
                    if retries == max_retries:
                        print(f"[-] FATAL: {func.__name__} failed after {max_retries} retries. Error: {e}", file=sys.stderr)
                        sys.exit(1)
                    print(f"[!] {func.__name__} failed, retrying in {delay}s... ({retries}/{max_retries})", file=sys.stderr)
                    time.sleep(delay)
        return wrapper
    return decorator


def load_or_init_manifest(manifest_path: str) -> Dict[str, str]:
    """
    加载或初始化 manifest.json 缓存锁文件
    """
    if os.path.exists(manifest_path) and os.path.getsize(manifest_path) > 5:
        try:
            with open(manifest_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except json.JSONDecodeError:
            return {}
    return {}


def save_manifest(manifest_path: str, manifest_data: Dict[str, str]) -> None:
    """
    保存 manifest.json 缓存锁文件
    """
    with open(manifest_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest_data, f, indent=4, sort_keys=True)
        f.write('\n')


@retry()
def _download_file(url: str) -> bytes:
    """
    内部函数：下载单个文件（带重试）
    """
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=TIMEOUT_SEC) as response:
        return response.read()


def fetch_or_cache(
        url: str,
        cache_dir: str,
        update_lock: bool,
        offline_mode: bool,
        manifest_path: str
) -> str:
    """
    下载或从缓存获取 Unicode 数据文件
    """
    filename = url.split('/')[-1]
    cache_path = os.path.join(cache_dir, filename)
    manifest_data = load_or_init_manifest(manifest_path)
    expected_hash = manifest_data.get(filename)

    # 优先检查本地缓存
    if os.path.exists(cache_path):
        with open(cache_path, 'rb') as f:
            content_bytes = f.read()
            if expected_hash and hashlib.sha256(content_bytes).hexdigest() == expected_hash:
                return content_bytes.decode('utf-8')

    # 离线模式下缓存缺失则报错
    if offline_mode:
        print(f"[-] ERROR: Cache missing/corrupt for '{filename}' and --offline is set.", file=sys.stderr)
        sys.exit(1)

    # 下载文件
    print(f"[*] Downloading: {filename}")
    content_bytes = _download_file(url)
    new_hash = hashlib.sha256(content_bytes).hexdigest()

    # 校验并更新 manifest
    if expected_hash and new_hash != expected_hash:
        if update_lock:
            manifest_data[filename] = new_hash
            save_manifest(manifest_path, manifest_data)
            print(f"[+] Updated hash for {filename} in manifest.json")
        else:
            print(f"[-] ERROR: Downloaded {filename} hash ({new_hash}) differs from lockfile ({expected_hash})!", file=sys.stderr)
            print(f"[-] Hint: Delete manifest.json to bootstrap, or pass --update-lock.", file=sys.stderr)
            sys.exit(1)
    elif not expected_hash:
        if update_lock:
            manifest_data[filename] = new_hash
            save_manifest(manifest_path, manifest_data)
            print(f"[+] Bootstrapped hash for {filename} in manifest.json")
        else:
            print(f"[-] ERROR: New file {filename} not in lockfile. Run with --update-lock.", file=sys.stderr)
            sys.exit(1)

    # 保存到缓存
    os.makedirs(cache_dir, exist_ok=True)
    with open(cache_path, 'wb') as f:
        f.write(content_bytes)

    return content_bytes.decode('utf-8')


def compress_1bit(arr: array) -> Tuple[List[int], List[Tuple[int, int, int, int]]]:
    """
    压缩 1 位属性数组（使用 array 优化内存）
    """
    blocks: List[int] = []
    block_map: Dict[Tuple[int, int, int, int], int] = {}
    unique_blocks: List[Tuple[int, int, int, int]] = []

    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = arr[i:i + BLOCK_SIZE]
        words = [0, 0, 0, 0]
        for w in range(4):
            v = 0
            for b in range(64):
                if chunk[w * 64 + b]:
                    v |= (1 << b)
            words[w] = v
        tpl = tuple(words)
        if tpl not in block_map:
            block_map[tpl] = len(unique_blocks)
            unique_blocks.append(tpl)
        blocks.append(block_map[tpl])

    if len(unique_blocks) > 65535:
        print(f"[-] FATAL: Unique blocks ({len(unique_blocks)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    return blocks, unique_blocks


def parse_derived_core_properties(txt_prop: str) -> Tuple[array, array]:
    """
    解析 DerivedCoreProperties.txt，生成 XID_Start/XID_Continue 数组
    """
    arr_start = array('b', [0]) * (MAX_CODEPOINT + 1)
    arr_cont = array('b', [0]) * (MAX_CODEPOINT + 1)
    pat_prop = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+(XID_Start|XID_Continue)\s+#')

    for line in txt_prop.splitlines():
        m = pat_prop.match(line)
        if m:
            s = int(m.group(1), 16)
            e = int(m.group(2), 16) if m.group(2) else s
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT:
                    if m.group(3) == 'XID_Start':
                        arr_start[cp] = 1
                    if m.group(3) == 'XID_Continue':
                        arr_cont[cp] = 1
    return arr_start, arr_cont


def parse_identifier_status(txt_sec: str) -> array:
    """
    解析 IdentifierStatus.txt，生成安全限制字符数组
    """
    arr_sec = array('b', [0]) * (MAX_CODEPOINT + 1)
    pat_sec = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+Restricted')

    for line in txt_sec.splitlines():
        m = pat_sec.match(line)
        if m:
            s = int(m.group(1), 16)
            e = int(m.group(2), 16) if m.group(2) else s
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT:
                    arr_sec[cp] = 1
    return arr_sec


def parse_unicode_data(txt_data: str) -> array:
    """
    解析 UnicodeData.txt，生成运算符符号数组
    """
    arr_op = array('b', [0]) * (MAX_CODEPOINT + 1)
    valid_op_categories = {'Sm', 'Sc', 'Sk', 'So', 'Po'}

    for line in txt_data.splitlines():
        if not line:
            continue
        parts = line.split(';')
        cp = int(parts[0], 16)
        category = parts[2]
        if cp <= MAX_CODEPOINT and category in valid_op_categories:
            arr_op[cp] = 1
    return arr_op


def parse_east_asian_width(txt_width: str) -> Tuple[array, List[int], List[Tuple[int, ...]]]:
    """
    解析 EastAsianWidth.txt，生成显示宽度数组并压缩
    """
    arr_w = array('B', [1]) * (MAX_CODEPOINT + 1)  # 'B' 表示 unsigned char
    pat_w = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+([A-Za-z]+)\s+#')

    for line in txt_width.splitlines():
        m = pat_w.match(line)
        if m:
            s = int(m.group(1), 16)
            e = int(m.group(2), 16) if m.group(2) else s
            w_val = 2 if m.group(3) in ('W', 'F') else 1
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT:
                    arr_w[cp] = w_val

    # 压缩宽度数组
    blocks_w: List[int] = []
    map_w: Dict[Tuple[int, ...], int] = {}
    uniq_w: List[Tuple[int, ...]] = []

    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = arr_w[i:i + BLOCK_SIZE]
        words = [0] * 8
        for w in range(8):
            v = 0
            for b in range(32):
                v |= (chunk[w * 32 + b] << (b * 2))
            words[w] = v
        tpl = tuple(words)
        if tpl not in map_w:
            map_w[tpl] = len(uniq_w)
            uniq_w.append(tpl)
        blocks_w.append(map_w[tpl])

    if len(uniq_w) > 65535:
        print(f"[-] FATAL: Width blocks ({len(uniq_w)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    return arr_w, blocks_w, uniq_w


def parse_case_folding(txt_cf: str) -> Tuple[List[int], List[int], List[Tuple[int, ...]]]:
    """
    解析 CaseFolding.txt，生成大小写折叠数组并压缩
    """
    arr_cf = list(range(MAX_CODEPOINT + 1))
    pat_cf = re.compile(r'^([0-9A-F]+);\s+[CS];\s+([0-9A-F]+);')

    for line in txt_cf.splitlines():
        m = pat_cf.match(line)
        if m:
            cp = int(m.group(1), 16)
            fold_cp = int(m.group(2), 16)
            if cp <= MAX_CODEPOINT:
                arr_cf[cp] = fold_cp

    # 压缩大小写折叠数组
    blocks_cf: List[int] = []
    map_cf: Dict[Tuple[int, ...], int] = {}
    uniq_cf: List[Tuple[int, ...]] = []

    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = tuple(arr_cf[i:i + BLOCK_SIZE])
        if chunk not in map_cf:
            map_cf[chunk] = len(uniq_cf)
            uniq_cf.append(chunk)
        blocks_cf.append(map_cf[chunk])

    if len(uniq_cf) > 65535:
        print(f"[-] FATAL: Case fold blocks ({len(uniq_cf)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    return arr_cf, blocks_cf, uniq_cf


def build_tables(
        ucd_base: str,
        sec_base: str,
        cache_dir: str,
        update_lock: bool,
        offline_mode: bool,
        manifest_path: str
) -> Dict[str, Tuple[List[int], List[Tuple[int, ...]]]]:
    """
    构建所有 Unicode 属性表（并行下载 + 拆分解析函数）
    """
    # 1. 并行下载所有需要的文件
    urls = [
        f"{ucd_base}/DerivedCoreProperties.txt",
        f"{sec_base}/IdentifierStatus.txt",
        f"{ucd_base}/UnicodeData.txt",
        f"{ucd_base}/EastAsianWidth.txt",
        f"{ucd_base}/CaseFolding.txt",
    ]

    url_to_content: Dict[str, str] = {}
    with ThreadPoolExecutor(max_workers=5) as executor:
        # 提交所有下载任务
        future_to_url = {
            executor.submit(fetch_or_cache, url, cache_dir, update_lock, offline_mode, manifest_path): url
            for url in urls
        }
        # 收集结果
        for future in as_completed(future_to_url):
            url = future_to_url[future]
            url_to_content[url] = future.result()

    # 2. 解析各个文件
    arr_start, arr_cont = parse_derived_core_properties(url_to_content[urls[0]])
    arr_sec = parse_identifier_status(url_to_content[urls[1]])
    arr_op = parse_unicode_data(url_to_content[urls[2]])
    _, blocks_w, uniq_w = parse_east_asian_width(url_to_content[urls[3]])
    _, blocks_cf, uniq_cf = parse_case_folding(url_to_content[urls[4]])

    # 3. 压缩 1 位属性数组
    return {
        "ID_START": compress_1bit(arr_start),
        "ID_CONTINUE": compress_1bit(arr_cont),
        "SECURITY_RESTRICTED": compress_1bit(arr_sec),
        "OPERATOR_SYMBOL": compress_1bit(arr_op),
        "WIDTH": (blocks_w, uniq_w),
        "CASE_FOLD": (blocks_cf, uniq_cf)
    }


def generate_cpp_code(tables: Dict[str, Tuple[List[int], List[Tuple[int, ...]]]], out_path: str) -> None:
    """
    生成 C++ 代码（使用 StringIO 优化 I/O）
    """
    buffer = StringIO()

    # 写入头文件和命名空间
    buffer.write("#include \"utf8/utf8.hpp\"\n\n")
    buffer.write("namespace utf8 {\n")
    buffer.write("namespace {\n")

    # 写入常量定义
    buffer.write("    inline constexpr uint32_t MAX_CODEPOINT = 0x10FFFF;\n")
    buffer.write("    inline constexpr size_t   STAGE1_LEN = 4352;\n")
    buffer.write("    inline constexpr uint8_t  BLOCK_SHIFT = 8;\n")
    buffer.write("    inline constexpr uint32_t OFFSET_MASK = 0xFF;\n")
    buffer.write("    inline constexpr uint8_t  BIT_WORD_SHIFT = 6;\n")
    buffer.write("    inline constexpr uint8_t  BIT_MASK = 0x3F;\n")
    buffer.write("    inline constexpr uint8_t  WIDTH_WORD_SHIFT = 5;\n")
    buffer.write("    inline constexpr uint8_t  WIDTH_BIT_MASK = 0x1F;\n")
    buffer.write("    inline constexpr uint8_t  WIDTH_MULTIPLIER = 2;\n")
    buffer.write("    inline constexpr uint8_t  WIDTH_VALUE_MASK = 0x03;\n")
    buffer.write("    inline constexpr uint8_t  PROP_WORDS_PER_BLOCK_SHIFT = 2;\n")
    buffer.write("    inline constexpr uint8_t  WIDTH_WORDS_PER_BLOCK_SHIFT = 3;\n")
    buffer.write("    inline constexpr uint8_t  CASE_FOLD_WORDS_PER_BLOCK_SHIFT = 8;\n")
    buffer.write("    inline constexpr uint8_t  ASCII_LIMIT = 0x80;\n")
    buffer.write("    inline constexpr uint8_t  ASCII_UPPER_A = 0x41;\n")
    buffer.write("    inline constexpr uint8_t  ASCII_UPPER_Z = 0x5A;\n")
    buffer.write("    inline constexpr uint8_t  ASCII_LOWER_A = 0x61;\n\n")

    # 写入静态断言
    buffer.write("    static_assert(STAGE1_LEN == ((MAX_CODEPOINT + 1u) >> BLOCK_SHIFT), \"FATAL\");\n\n")

    # 写入属性表数组
    for name, (s1, s2) in tables.items():
        buffer.write(f"    alignas(64) constexpr uint16_t {name}_STAGE1[STAGE1_LEN] = {{ {','.join(map(str, s1))} }};\n")
        if name == "CASE_FOLD":
            buffer.write(f"    alignas(64) constexpr uint32_t {name}_STAGE2[] = {{\n")
            for b in s2:
                buffer.write(f"        {','.join(f'0x{w:08X}' for w in b)},\n")
        else:
            buffer.write(f"    alignas(64) constexpr uint64_t {name}_STAGE2[] = {{\n")
            for b in s2:
                buffer.write(f"        {','.join(f'0x{w:016X}ULL' for w in b)},\n")
        buffer.write("    };\n\n")

    # 关闭匿名命名空间
    buffer.write("} // namespace\n\n")

    # 写入属性查询函数
    macros = [
        ("is_identifier_start", "ID_START"),
        ("is_identifier_continue", "ID_CONTINUE"),
        ("is_restricted_confusable", "SECURITY_RESTRICTED"),
        ("is_operator_symbol_candidate", "OPERATOR_SYMBOL")
    ]
    for func, prefix in macros:
        buffer.write(f"[[nodiscard]] UTF8_API bool {func}(uint32_t cp) noexcept {{\n")
        buffer.write(f"    if (cp > MAX_CODEPOINT) [[unlikely]] return false;\n")
        buffer.write(f"    uint32_t block = {prefix}_STAGE1[cp >> BLOCK_SHIFT];\n")
        buffer.write(f"    uint32_t offset = cp & OFFSET_MASK;\n")
        buffer.write(f"    uint64_t bitmap = {prefix}_STAGE2[(block << PROP_WORDS_PER_BLOCK_SHIFT) + (offset >> BIT_WORD_SHIFT)];\n")
        buffer.write(f"    return (bitmap >> (offset & BIT_MASK)) & 1;\n")
        buffer.write("}\n\n")

    # 写入显示宽度函数
    buffer.write("[[nodiscard]] UTF8_API uint8_t display_width_approx(uint32_t cp) noexcept {\n")
    buffer.write("    if (cp > MAX_CODEPOINT) [[unlikely]] return 1;\n")
    buffer.write("    uint32_t block = WIDTH_STAGE1[cp >> BLOCK_SHIFT];\n")
    buffer.write("    uint32_t offset = cp & OFFSET_MASK;\n")
    buffer.write("    uint64_t bitmap = WIDTH_STAGE2[(block << WIDTH_WORDS_PER_BLOCK_SHIFT) + (offset >> WIDTH_WORD_SHIFT)];\n")
    buffer.write("    uint8_t shift = (offset & WIDTH_BIT_MASK) * WIDTH_MULTIPLIER;\n")
    buffer.write("    return (bitmap >> shift) & WIDTH_VALUE_MASK;\n")
    buffer.write("}\n\n")

    # 写入大小写折叠函数
    buffer.write("[[nodiscard]] UTF8_API uint32_t fold_case_simple(uint32_t cp) noexcept {\n")
    buffer.write("    if (cp > MAX_CODEPOINT) [[unlikely]] return cp;\n")
    buffer.write("    if (cp >= ASCII_UPPER_A && cp <= ASCII_UPPER_Z) [[likely]] {\n")
    buffer.write("        return cp + (ASCII_LOWER_A - ASCII_UPPER_A);\n")
    buffer.write("    }\n")
    buffer.write("    if (cp < ASCII_LIMIT) [[likely]] return cp;\n")
    buffer.write("    uint32_t block = CASE_FOLD_STAGE1[cp >> BLOCK_SHIFT];\n")
    buffer.write("    return CASE_FOLD_STAGE2[(block << CASE_FOLD_WORDS_PER_BLOCK_SHIFT) + (cp & OFFSET_MASK)];\n")
    buffer.write("}\n\n")

    # 关闭 utf8 命名空间
    buffer.write("} // namespace utf8\n")

    # 一次性写入文件
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(buffer.getvalue())


def main() -> None:
    """
    主函数
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, help="Output C++ file path")
    parser.add_argument("--ucd-version", default="15.1.0", help="Unicode version (default: 15.1.0)")
    parser.add_argument("--cache-dir", required=True, help="Cache directory path")
    parser.add_argument("--update-lock", action="store_true", help="Update manifest.json hash lockfile")
    parser.add_argument("--offline", action="store_true", help="Offline mode (no network access)")
    args = parser.parse_args()

    manifest_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "manifest.json")

    # 自动 Bootstrap 模式
    auto_bootstrap = False
    if not os.path.exists(manifest_path) or os.path.getsize(manifest_path) < 5:
        print("[*] Manifest missing or empty. Entering Bootstrap mode to generate lockfile...")
        auto_bootstrap = True

    effective_update_lock = args.update_lock or auto_bootstrap

    # 构建属性表
    tables = build_tables(
        f"https://www.unicode.org/Public/{args.ucd_version}/ucd",
        f"https://www.unicode.org/Public/security/{args.ucd_version}",
        args.cache_dir,
        effective_update_lock,
        args.offline,
        manifest_path
    )

    # 生成 C++ 代码
    generate_cpp_code(tables, args.out)


if __name__ == '__main__':
    main()