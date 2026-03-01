# UTF-8 编解码工具库

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))  <!-- 根据实际语言替换，如 Python/Java 等 -->

## 仓库简介
本仓库实现了 UTF-8 编码/解码的核心逻辑，聚焦**轻量、可复用、易理解**的设计目标，既适用于开发者学习 UTF-8 底层编码原理，也可直接集成到嵌入式、轻量服务等场景中，解决 UTF-8 字符处理、合法性校验等核心问题。

相较于编程语言内置的 UTF-8 工具，本仓库的核心优势：
- 无第三方依赖，代码体积小，可直接嵌入项目；
- 完整的非法字节序列校验与容错处理，避免编解码异常；
- 代码注释详尽，清晰还原 UTF-8 编码规则的底层逻辑；
- 提供简洁的 API 接口，降低业务集成成本。

## 核心功能
| 功能项                | 描述                                                                 |
|-----------------------|----------------------------------------------------------------------|
| UTF-8 编码            | 将 Unicode 码点（UCS-4/UCS-2）转换为 UTF-8 字节序列                  |
| UTF-8 解码            | 将 UTF-8 字节序列还原为 Unicode 码点，支持多字节字符（1-4 字节）     |
| 合法性校验            | 校验 UTF-8 字节序列是否符合标准（如续字节格式、码点范围、超长编码等） |
| 字节长度计算          | 根据 Unicode 码点快速计算对应的 UTF-8 编码字节长度                   |
| 非法序列容错（可选）| 对非法 UTF-8 字节序列进行替换/跳过处理，避免解码中断                 |

## 快速开始

### 环境要求
- 编译/运行环境：C99 及以上编译器（GCC/Clang/MSVC）<!-- 根据实际语言调整，如 Python 3.6+ -->
- 无额外第三方依赖

### 安装/集成
#### 方式 1：直接引入源码
将仓库中 `src/` 目录下的核心文件（如 `utf8.h`、`utf8.c`）复制到你的项目中，直接包含头文件使用：
```c
#include "utf8.h"
```

#### 方式 2：编译静态库（可选）
```bash
# 编译静态库
gcc -c src/utf8.c -o utf8.o -std=c99
ar rcs libutf8.a utf8.o

# 项目中链接使用
gcc your_project.c -o your_project -L./ -lutf8 -std=c99
```

### 使用示例
#### 示例 1：UTF-8 编码（Unicode 码点 → 字节序列）
```c
#include "utf8.h"
#include <stdio.h>

int main() {
    // 待编码的 Unicode 码点（示例：中文“中”，U+4E2D）
    uint32_t codepoint = 0x4E2D;
    uint8_t utf8_buf[4] = {0};
    int len = utf8_encode(codepoint, utf8_buf);

    if (len > 0) {
        printf("编码结果（字节）：");
        for (int i = 0; i < len; i++) {
            printf("%02X ", utf8_buf[i]); // 输出：E4 B8 AD
        }
    } else {
        printf("编码失败：无效的 Unicode 码点\n");
    }
    return 0;
}
```

#### 示例 2：UTF-8 解码（字节序列 → Unicode 码点）
```c
#include "utf8.h"
#include <stdio.h>

int main() {
    // UTF-8 字节序列（“中”：0xE4 0xB8 0xAD）
    uint8_t utf8_buf[] = {0xE4, 0xB8, 0xAD};
    uint32_t codepoint = 0;
    int len = utf8_decode(utf8_buf, sizeof(utf8_buf), &codepoint);

    if (len > 0) {
        printf("解码结果（码点）：U+%04X\n", codepoint); // 输出：U+4E2D
    } else {
        printf("解码失败：非法的 UTF-8 字节序列\n");
    }
    return 0;
}
```

#### 示例 3：UTF-8 合法性校验
```c
#include "utf8.h"
#include <stdio.h>

int main() {
    uint8_t invalid_utf8[] = {0xE4, 0xB8, 0x80, 0xFF}; // 包含非法字节 0xFF
    int is_valid = utf8_validate(invalid_utf8, sizeof(invalid_utf8));

    if (is_valid) {
        printf("UTF-8 序列合法\n");
    } else {
        printf("UTF-8 序列非法\n"); // 输出此结果
    }
    return 0;
}
```

## API 文档
### 1. utf8_encode
```c
/**
 * @brief 将 Unicode 码点编码为 UTF-8 字节序列
 * @param codepoint 待编码的 Unicode 码点（0x000000 - 0x10FFFF）
 * @param out_buf 输出缓冲区，至少需 4 字节空间
 * @return 成功：编码后的字节长度（1-4）；失败：0（无效码点）
 */
int utf8_encode(uint32_t codepoint, uint8_t *out_buf);
```

### 2. utf8_decode
```c
/**
 * @brief 将 UTF-8 字节序列解码为 Unicode 码点
 * @param in_buf 输入 UTF-8 字节序列
 * @param buf_len 输入缓冲区长度
 * @param out_codepoint 输出解码后的 Unicode 码点
 * @return 成功：消耗的字节长度（1-4）；失败：0（非法字节序列）
 */
int utf8_decode(const uint8_t *in_buf, int buf_len, uint32_t *out_codepoint);
```

### 3. utf8_validate
```c
/**
 * @brief 校验 UTF-8 字节序列的合法性
 * @param in_buf 输入 UTF-8 字节序列
 * @param buf_len 输入缓冲区长度
 * @return 1：合法；0：非法
 */
int utf8_validate(const uint8_t *in_buf, int buf_len);
```

### 4. utf8_codepoint_length
```c
/**
 * @brief 计算 Unicode 码点对应的 UTF-8 编码字节长度
 * @param codepoint Unicode 码点
 * @return 成功：1-4；失败：0（无效码点）
 */
int utf8_codepoint_length(uint32_t codepoint);
```

## 测试用例
仓库 `test/` 目录下提供了完整的单元测试用例，覆盖以下场景：
- 基础编解码：单字节（ASCII）、双字节、三字节、四字节字符；
- 边界值测试：Unicode 最小/最大码点、UTF-8 编码边界值；
- 异常测试：非法码点、超长编码、续字节缺失、无效续字节等；

运行测试用例：
```bash
gcc test/utf8_test.c src/utf8.c -o utf8_test -std=c99 -Wall
./utf8_test
```

## 编码规范
- 代码遵循 UTF-8 标准（RFC 3629），兼容 Unicode 15.0 及以上版本；
- 所有接口均做参数合法性校验，避免空指针、缓冲区越界等问题；
- 错误处理：通过返回值明确标识异常，不依赖全局变量/断言。

## 许可证
本项目基于 MIT 许可证开源，详见 [LICENSE](LICENSE) 文件。

## 贡献指南
1. Fork 本仓库；
2. 创建特性分支（`git checkout -b feature/xxx`）；
3. 提交代码（`git commit -m 'feat: 新增 xxx 功能'`）；
4. 推送分支（`git push origin feature/xxx`）；
5. 提交 Pull Request。

贡献需遵循以下规范：
- 新增功能需补充对应的单元测试；
- 代码注释需清晰描述逻辑，关键函数需包含文档注释；
- 保持代码风格与现有代码一致（如缩进、命名规范）。

## 参考资料
- [RFC 3629 - UTF-8, a transformation format of ISO 10646](https://datatracker.ietf.org/doc/html/rfc3629)
- [Unicode Standard 15.0](https://www.unicode.org/versions/Unicode15.0.0/)
- [UTF-8 编码规则详解](https://en.wikipedia.org/wiki/UTF-8)
