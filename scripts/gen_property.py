#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import hashlib
import json
import os
import re
import sys
import urllib.request

MAX_CODEPOINT = 0x10FFFF
BLOCK_SIZE = 256
TIMEOUT_SEC = 15


def load_or_init_manifest(manifest_path):
    if os.path.exists(manifest_path) and os.path.getsize(manifest_path) > 5:
        try:
            with open(manifest_path, 'r', encoding='utf-8') as f:
                return json.load(f)
        except json.JSONDecodeError:
            return {}
    return {}


def save_manifest(manifest_path, manifest_data):
    with open(manifest_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(manifest_data, f, indent=4, sort_keys=True)
        f.write('\n')


def fetch_or_cache(url, cache_dir, update_lock, offline_mode, manifest_path):
    filename = url.split('/')[-1]
    cache_path = os.path.join(cache_dir, filename)
    manifest_data = load_or_init_manifest(manifest_path)
    expected_hash = manifest_data.get(filename)

    if os.path.exists(cache_path):
        with open(cache_path, 'rb') as f:
            content_bytes = f.read()
            if expected_hash and hashlib.sha256(content_bytes).hexdigest() == expected_hash:
                return content_bytes.decode('utf-8')

    if offline_mode:
        print(f"[-] ERROR: Cache missing/corrupt for '{filename}' and --offline is set.", file=sys.stderr)
        sys.exit(1)

    print(f"[*] Downloading: {filename}")
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=TIMEOUT_SEC) as response:
        content_bytes = response.read()
        new_hash = hashlib.sha256(content_bytes).hexdigest()

        if expected_hash and new_hash != expected_hash:
            if update_lock:
                manifest_data[filename] = new_hash
                save_manifest(manifest_path, manifest_data)
                print(f"[+] Updated hash for {filename} in manifest.json")
            else:
                print(f"[-] ERROR: Downloaded {filename} hash ({new_hash}) differs from lockfile ({expected_hash})!",
                      file=sys.stderr)
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

        os.makedirs(cache_dir, exist_ok=True)
        with open(cache_path, 'wb') as f:
            f.write(content_bytes)

        return content_bytes.decode('utf-8')


def compress_1bit(arr):
    blocks, block_map, unique_blocks = [], {}, []
    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = arr[i:i + BLOCK_SIZE]
        words = []
        for w in range(4):
            v = 0
            for b in range(64):
                if chunk[w * 64 + b]: v |= (1 << b)
            words.append(v)
        tpl = tuple(words)
        if tpl not in block_map:
            block_map[tpl] = len(unique_blocks)
            unique_blocks.append(tpl)
        blocks.append(block_map[tpl])

    if len(unique_blocks) > 65535:
        print(f"[-] FATAL: Unique blocks ({len(unique_blocks)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    return blocks, unique_blocks


def build_tables(ucd_base, sec_base, cache_dir, update_lock, offline_mode, manifest_path):
    txt_prop = fetch_or_cache(f"{ucd_base}/DerivedCoreProperties.txt", cache_dir, update_lock, offline_mode,
                              manifest_path)
    arr_start, arr_cont = [False] * (MAX_CODEPOINT + 1), [False] * (MAX_CODEPOINT + 1)
    pat_prop = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+(XID_Start|XID_Continue)\s+#')
    for line in txt_prop.splitlines():
        m = pat_prop.match(line)
        if m:
            s, e = int(m.group(1), 16), int(m.group(2), 16) if m.group(2) else int(m.group(1), 16)
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT:
                    if m.group(3) == 'XID_Start': arr_start[cp] = True
                    if m.group(3) == 'XID_Continue': arr_cont[cp] = True

    txt_sec = fetch_or_cache(f"{sec_base}/IdentifierStatus.txt", cache_dir, update_lock, offline_mode, manifest_path)
    arr_sec = [False] * (MAX_CODEPOINT + 1)
    pat_sec = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+Restricted')
    for line in txt_sec.splitlines():
        m = pat_sec.match(line)
        if m:
            s, e = int(m.group(1), 16), int(m.group(2), 16) if m.group(2) else int(m.group(1), 16)
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT: arr_sec[cp] = True

    txt_data = fetch_or_cache(f"{ucd_base}/UnicodeData.txt", cache_dir, update_lock, offline_mode, manifest_path)
    arr_op = [False] * (MAX_CODEPOINT + 1)
    valid_op_categories = {'Sm', 'Sc', 'Sk', 'So', 'Po'}
    for line in txt_data.splitlines():
        if not line: continue
        parts = line.split(';')
        cp, category = int(parts[0], 16), parts[2]
        if cp <= MAX_CODEPOINT and category in valid_op_categories:
            arr_op[cp] = True

    txt_width = fetch_or_cache(f"{ucd_base}/EastAsianWidth.txt", cache_dir, update_lock, offline_mode, manifest_path)
    arr_w = [1] * (MAX_CODEPOINT + 1)
    pat_w = re.compile(r'^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+([A-Za-z]+)\s+#')
    for line in txt_width.splitlines():
        m = pat_w.match(line)
        if m:
            s, e = int(m.group(1), 16), int(m.group(2), 16) if m.group(2) else int(m.group(1), 16)
            w_val = 2 if m.group(3) in ('W', 'F') else 1
            for cp in range(s, e + 1):
                if cp <= MAX_CODEPOINT: arr_w[cp] = w_val

    blocks_w, map_w, uniq_w = [], {}, []
    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = arr_w[i:i + BLOCK_SIZE]
        words = []
        for w in range(8):
            v = 0
            for b in range(32):
                v |= (chunk[w * 32 + b] << (b * 2))
            words.append(v)
        tpl = tuple(words)
        if tpl not in map_w:
            map_w[tpl] = len(uniq_w)
            uniq_w.append(tpl)
        blocks_w.append(map_w[tpl])

    if len(uniq_w) > 65535:
        print(f"[-] FATAL: Width blocks ({len(uniq_w)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    txt_cf = fetch_or_cache(f"{ucd_base}/CaseFolding.txt", cache_dir, update_lock, offline_mode, manifest_path)
    arr_cf = [cp for cp in range(MAX_CODEPOINT + 1)]
    pat_cf = re.compile(r'^([0-9A-F]+);\s+[CS];\s+([0-9A-F]+);')
    for line in txt_cf.splitlines():
        m = pat_cf.match(line)
        if m:
            cp, fold_cp = int(m.group(1), 16), int(m.group(2), 16)
            if cp <= MAX_CODEPOINT: arr_cf[cp] = fold_cp

    blocks_cf, map_cf, uniq_cf = [], {}, []
    for i in range(0, MAX_CODEPOINT + 1, BLOCK_SIZE):
        chunk = tuple(arr_cf[i:i + BLOCK_SIZE])
        if chunk not in map_cf:
            map_cf[chunk] = len(uniq_cf)
            uniq_cf.append(chunk)
        blocks_cf.append(map_cf[chunk])

    if len(uniq_cf) > 65535:
        print(f"[-] FATAL: Case fold blocks ({len(uniq_cf)}) exceeds uint16_t capacity!", file=sys.stderr)
        sys.exit(1)

    return {
        "ID_START": compress_1bit(arr_start),
        "ID_CONTINUE": compress_1bit(arr_cont),
        "SECURITY_RESTRICTED": compress_1bit(arr_sec),
        "OPERATOR_SYMBOL": compress_1bit(arr_op),
        "WIDTH": (blocks_w, uniq_w),
        "CASE_FOLD": (blocks_cf, uniq_cf)
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--ucd-version", default="15.1.0")
    parser.add_argument("--cache-dir", required=True)
    parser.add_argument("--update-lock", action="store_true")
    parser.add_argument("--offline", action="store_true")
    args = parser.parse_args()

    manifest_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "manifest.json")

    # ✅ 核心增强：如果发现 manifest.json 不存在或者为空，直接进入自动 Bootstrap 模式！
    auto_bootstrap = False
    if not os.path.exists(manifest_path) or os.path.getsize(manifest_path) < 5:
        print("[*] Manifest missing or empty. Entering Bootstrap mode to generate lockfile...")
        auto_bootstrap = True

    effective_update_lock = args.update_lock or auto_bootstrap

    tables = build_tables(
        f"https://www.unicode.org/Public/{args.ucd_version}/ucd",
        f"https://www.unicode.org/Public/security/{args.ucd_version}",
        args.cache_dir,
        effective_update_lock,
        args.offline,
        manifest_path
    )

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write("#include \"utf8/utf8.hpp\"\n\n")
        f.write("namespace utf8 {\n")
        f.write("namespace {\n")

        f.write("    inline constexpr uint32_t MAX_CODEPOINT = 0x10FFFF;\n")
        f.write("    inline constexpr size_t   STAGE1_LEN = 4352;\n")
        f.write("    inline constexpr uint8_t  BLOCK_SHIFT = 8;\n")
        f.write("    inline constexpr uint32_t OFFSET_MASK = 0xFF;\n")
        f.write("    inline constexpr uint8_t  BIT_WORD_SHIFT = 6;\n")
        f.write("    inline constexpr uint8_t  BIT_MASK = 0x3F;\n")
        f.write("    inline constexpr uint8_t  WIDTH_WORD_SHIFT = 5;\n")
        f.write("    inline constexpr uint8_t  WIDTH_BIT_MASK = 0x1F;\n")
        f.write("    inline constexpr uint8_t  WIDTH_MULTIPLIER = 2;\n")
        f.write("    inline constexpr uint8_t  WIDTH_VALUE_MASK = 0x03;\n")
        f.write("    inline constexpr uint8_t  PROP_WORDS_PER_BLOCK_SHIFT = 2;\n")
        f.write("    inline constexpr uint8_t  WIDTH_WORDS_PER_BLOCK_SHIFT = 3;\n")
        f.write("    inline constexpr uint8_t  CASE_FOLD_WORDS_PER_BLOCK_SHIFT = 8;\n")
        f.write("    inline constexpr uint8_t  ASCII_LIMIT = 0x80;\n")
        f.write("    inline constexpr uint8_t  ASCII_UPPER_A = 0x41;\n")
        f.write("    inline constexpr uint8_t  ASCII_UPPER_Z = 0x5A;\n")
        f.write("    inline constexpr uint8_t  ASCII_LOWER_A = 0x61;\n\n")

        f.write("    static_assert(STAGE1_LEN == ((MAX_CODEPOINT + 1u) >> BLOCK_SHIFT), \"FATAL\");\n\n")

        for name, (s1, s2) in tables.items():
            f.write(f"    alignas(64) constexpr uint16_t {name}_STAGE1[STAGE1_LEN] = {{ {','.join(map(str, s1))} }};\n")
            if name == "CASE_FOLD":
                f.write(f"    alignas(64) constexpr uint32_t {name}_STAGE2[] = {{\n")
                for b in s2: f.write(f"        {','.join(f'0x{w:08X}' for w in b)},\n")
            else:
                f.write(f"    alignas(64) constexpr uint64_t {name}_STAGE2[] = {{\n")
                for b in s2: f.write(f"        {','.join(f'0x{w:016X}ULL' for w in b)},\n")
            f.write("    };\n\n")
        f.write("} // namespace\n\n")

        macros = [
            ("is_identifier_start", "ID_START"),
            ("is_identifier_continue", "ID_CONTINUE"),
            ("is_restricted_confusable", "SECURITY_RESTRICTED"),
            ("is_operator_symbol_candidate", "OPERATOR_SYMBOL")
        ]
        for func, prefix in macros:
            f.write(f"[[nodiscard]] UTF8_API bool {func}(uint32_t cp) noexcept {{\n")
            f.write(f"    if (cp > MAX_CODEPOINT) [[unlikely]] return false;\n")
            f.write(f"    uint32_t block = {prefix}_STAGE1[cp >> BLOCK_SHIFT];\n")
            f.write(f"    uint32_t offset = cp & OFFSET_MASK;\n")
            f.write(
                f"    uint64_t bitmap = {prefix}_STAGE2[(block << PROP_WORDS_PER_BLOCK_SHIFT) + (offset >> BIT_WORD_SHIFT)];\n")
            f.write(f"    return (bitmap >> (offset & BIT_MASK)) & 1;\n")
            f.write("}\n\n")

        f.write("[[nodiscard]] UTF8_API uint8_t display_width_approx(uint32_t cp) noexcept {\n")
        f.write("    if (cp > MAX_CODEPOINT) [[unlikely]] return 1;\n")
        f.write("    uint32_t block = WIDTH_STAGE1[cp >> BLOCK_SHIFT];\n")
        f.write("    uint32_t offset = cp & OFFSET_MASK;\n")
        f.write(
            "    uint64_t bitmap = WIDTH_STAGE2[(block << WIDTH_WORDS_PER_BLOCK_SHIFT) + (offset >> WIDTH_WORD_SHIFT)];\n")
        f.write("    uint8_t shift = (offset & WIDTH_BIT_MASK) * WIDTH_MULTIPLIER;\n")
        f.write("    return (bitmap >> shift) & WIDTH_VALUE_MASK;\n")
        f.write("}\n\n")

        f.write("[[nodiscard]] UTF8_API uint32_t fold_case_simple(uint32_t cp) noexcept {\n")
        f.write("    if (cp > MAX_CODEPOINT) [[unlikely]] return cp;\n")
        f.write("    if (cp >= ASCII_UPPER_A && cp <= ASCII_UPPER_Z) [[likely]] {\n")
        f.write("        return cp + (ASCII_LOWER_A - ASCII_UPPER_A);\n")
        f.write("    }\n")
        f.write("    if (cp < ASCII_LIMIT) [[likely]] return cp;\n")
        f.write("    uint32_t block = CASE_FOLD_STAGE1[cp >> BLOCK_SHIFT];\n")
        f.write("    return CASE_FOLD_STAGE2[(block << CASE_FOLD_WORDS_PER_BLOCK_SHIFT) + (cp & OFFSET_MASK)];\n")
        f.write("}\n\n")

        f.write("} // namespace utf8\n")


if __name__ == '__main__':
    main()
