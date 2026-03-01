# UTF-8 编解码工具库

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language](https://img.shields.io/badge/language-C++-brightgreen.svg)](https://en.wikipedia.org/wiki/C++)

## 仓库简介

本仓库实现了 UTF-8 编码/解码的核心逻辑，聚焦**轻量、可复用、易理解**的设计目标，既适用于开发者学习 UTF-8
底层编码原理，也可直接集成到嵌入式、轻量服务等场景中，解决 UTF-8 字符处理、合法性校验等核心问题。

相较于编程语言内置的 UTF-8 工具，本仓库的核心优势：

- 无第三方依赖，代码体积小，可直接嵌入项目；
- 完整的非法字节序列校验与容错处理，避免编解码异常；
- 代码注释详尽，清晰还原 UTF-8 编码规则的底层逻辑；
- 提供简洁的 API 接口，降低业务集成成本。

## 核心功能

| 功能项        | 描述                                         |
|------------|--------------------------------------------|
| UTF-8 编码   | 将 Unicode 码点（UCS-4/UCS-2）转换为 UTF-8 字节序列    |
| UTF-8 解码   | 将 UTF-8 字节序列还原为 Unicode 码点，支持多字节字符（1-4 字节） |
| 合法性校验      | 校验 UTF-8 字节序列是否符合标准（如续字节格式、码点范围、超长编码等）     |
| 字节长度计算     | 根据 Unicode 码点快速计算对应的 UTF-8 编码字节长度          |
| 非法序列容错（可选） | 对非法 UTF-8 字节序列进行替换/跳过处理，避免解码中断             |

## 快速开始

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
