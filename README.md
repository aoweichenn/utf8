# UTF-8 编解码工具库

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C++20-brightgreen.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)

## 📖 仓库简介

本仓库实现了一套高性能、内存安全的 C++20 UTF-8 处理基础库。聚焦于**极致性能、零内存分配、现代 C++ 契约式设计**。不仅适用于学习
UTF-8 底层编码原理，更能直接集成到对性能要求严苛的工业级场景中（如编译器前端、高性能文本渲染、网络协议解析等）。

相较于传统的 UTF-8 工具库，本仓库的核心优势：

* **现代 C++20 设计**：全面采用 `std::span` 替代裸指针，保证内存安全与零拷贝处理；
* **极致的性能优化**：
    * 内置针对 ASCII 的 Fast-Path，并配合 `[[likely]]` / `[[unlikely]]` 优化分支预测；
    * 提供带有 Padding Sentinel（安全冗余边界）的 Unsafe 接口，极限压榨解码性能；
    * 字符属性查询（如标识符判断、宽度计算等）全部实现为 **O(1) 纯函数**；
* **高安全性与容错**：不依赖异常机制 (No Exceptions)，不阻断程序，完全通过状态码返回完整的局部诊断信息；
* **无第三方依赖**：纯头文件与极简源码，极易嵌入任何 C++ 工程。

## ✨ 核心功能

| 功能模块             | API 概览                           | 描述                                                 |
|------------------|----------------------------------|----------------------------------------------------|
| **高性能解码**        | `decode_next` / `decode_prev`    | 基于 `std::span` 的双向解码，安全且支持完整状态诊断。                  |
| **极速解码(Unsafe)** | `decode_next_unsafe_padded`      | **高性能模式**：调用方保证 4 字节安全边界，跳过边界检查直接解码。               |
| **编码与转换**        | `encode` / `utf8_to_utf16`       | 高效的 UTF-8 编码以及与 UTF-16 格式的互转，支持保留已写入长度。            |
| **Unicode 属性查询** | `is_identifier_start` 等          | O(1) 判断标识符、运算符候选、大小写折叠 (`fold_case_simple`) 及显示宽度。 |
| **实用工具**         | `strip_bom` / `to_escaped_ascii` | 快速剥离 UTF-8 BOM 头，或将非 ASCII 字符安全转义。                 |

## 🚀 快速开始

### 1. 编译与集成

本项目推荐使用 CMake 进行集成。你可以直接将源码目录添加到你的项目中：

```cmake
# 在你的 CMakeLists.txt 中添加
add_subdirectory(path/to/utf8)
target_link_libraries(your_target_name PRIVATE utf8)

```

*注：本项目依赖 C++20 标准（如 `std::span`），请确保你的编译器支持并已开启 `-std=c++20`。*

### 2. 代码示例

#### 示例 A：安全的单步解码 (Safe Decoding)

使用 `std::span` 进行安全的越界检查和解码：

```cpp
#include <vector>
#include <iostream>
#include "utf8/utf8.hpp" // 替换为实际头文件路径

int main() {
    std::vector<uint8_t> text = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD}; // "你好"
    std::span<const uint8_t> view(text);

    while (!view.empty()) {
        auto result = utf8::decode_next(view);
        
        if (result.status == utf8::base::DecodeStatus::Success) {
            std::cout << "Codepoint: U+" << std::hex << result.codepoint << std::endl;
        } else {
            std::cerr << "Decode error!" << std::endl;
        }
        
        // 推进视图窗口
        view = view.subspan(result.bytes_consumed);
    }
    return 0;
}

```

#### 示例 B：极限性能解码 (Unsafe Padded Decoding)

在编译器词法分析等高性能场景，你可以通过在缓冲区末尾填充 4 字节的 Padding 来免除边界检查：

```cpp
// 注意：契约要求 buffer 末尾必须包含至少 4 字节的安全冗余边界！
std::vector<uint8_t> padded_buffer = {0x41, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00};
const uint8_t* ptr = padded_buffer.data();

// 极速解析，无视边界检查
auto result = utf8::decode_next_unsafe_padded(ptr);
std::cout << "Codepoint: " << result.codepoint << std::endl; 

```

#### 示例 C：属性查询与实用工具

库内置了 O(1) 的丰富属性查询，非常适合用于文本编辑器或词法分析器开发：

```cpp
uint32_t cp = 0x53D8; // '变'

// 判断是否可以作为标识符的开头
if (utf8::is_identifier_start(cp)) {
    // ...
}

// 估算字符的终端显示宽度
uint8_t width = utf8::display_width_approx(cp);

// 移除 BOM 头
std::span<const uint8_t> file_data = /* ... */;
auto data_without_bom = utf8::utils::strip_bom(file_data);

```

## 📜 编码规范与契约

* **异常安全**：全库使用 `noexcept`，不抛出任何异常。
* **内存安全**：常规接口强制使用 `std::span` 防御越界；`_unsafe` 接口采用严格的调用方契约（Caller Contract）。
* **兼容性**：行为符合 RFC 3629 标准。

## 📄 许可证

本项目基于 MIT 许可证开源，详见 [LICENSE](LICENSE) 文件。
