# ============================================================================
# UTF-8 基础库工业级 Makefile 控制台
# ============================================================================

JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Fuzzing 默认参数 (支持在命令行覆盖)
FUZZ_TIME ?= 3600
FUZZ_LEN  ?= 512

DEBUG_DIR   := cmake-build-debug
RELEASE_DIR := cmake-build-release
OUTPUT_DIR  := output

.PHONY: help test bench fuzz coverage clean

help:
	@echo "======================================================================"
	@echo " 🚀 UTF-8 Foundation Library - Build & Test Commands"
	@echo "======================================================================"
	@echo " make test   - 构建并运行单元测试 (Debug 模式, 开启 UB 防御)"
	@echo " make bench  - 构建并运行性能压测 (Release 模式, 最高级 -O3 优化)"
	@echo " make fuzz   - 构建并运行模糊测试 (默认 1小时, 最大长度 512 字节)"
	@echo "               自定义参数示例: make fuzz FUZZ_TIME=7200 FUZZ_LEN=1024"
	@echo " make coverage - 生成代码覆盖率 HTML 报告 (适配 Clang 工具链)"
	@echo " make clean  - 清理所有编译缓存和输出产物"
	@echo "======================================================================"

test:
	@echo "\n[1/3] 配置 CMake (Debug 模式, 开启测试, 显示下载进度)..."
	cmake -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug -DUTF8_BUILD_TESTS=ON -DUTF8_BUILD_BENCHMARKS=OFF -DFETCHCONTENT_QUIET=OFF
	@echo "\n[2/3] 编译 utf8_test (并行度: $(JOBS))..."
	cmake --build $(DEBUG_DIR) --target utf8_test -j $(JOBS)
	@echo "\n[3/3] 执行单元测试..."
	./$(OUTPUT_DIR)/debug/bin/utf8_test

bench:
	@echo "\n[1/3] 配置 CMake (Release 模式, 开启压测, 显示下载进度)..."
	cmake -B $(RELEASE_DIR) -DCMAKE_BUILD_TYPE=Release -DUTF8_BUILD_TESTS=OFF -DUTF8_BUILD_BENCHMARKS=ON -DFETCHCONTENT_QUIET=OFF
	@echo "\n[2/3] 编译 utf8_bench (并行度: $(JOBS))..."
	cmake --build $(RELEASE_DIR) --target utf8_bench -j $(JOBS)
	@echo "\n[3/3] 执行极限压测..."
	./$(OUTPUT_DIR)/release/bin/utf8_bench

fuzz:
	@echo "\n[1/3] 配置 CMake (使用 Clang, 注入 Fuzzer 探针)..."
	CC=clang CXX=clang++ cmake -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug -DUTF8_BUILD_TESTS=OFF -DUTF8_BUILD_BENCHMARKS=OFF
	@echo "\n[2/3] 编译 utf8_fuzzer (并行度: $(JOBS))..."
	cmake --build $(DEBUG_DIR) --target utf8_fuzzer -j $(JOBS)
	@echo "\n[3/3] 启动 Fuzzing (时间限制: $(FUZZ_TIME)秒, 最大输入长度: $(FUZZ_LEN)字节)..."
	./$(OUTPUT_DIR)/debug/fuzz/utf8_fuzzer -max_total_time=$(FUZZ_TIME) -max_len=$(FUZZ_LEN)

coverage:
	@echo "\n[1/4] 配置 CMake (强制使用 Clang, Debug 模式, 开启插桩与分支追踪)..."
	CC=clang CXX=clang++ cmake -B $(DEBUG_DIR) -DCMAKE_BUILD_TYPE=Debug -DUTF8_BUILD_TESTS=ON -DUTF8_BUILD_BENCHMARKS=OFF -DUTF8_BUILD_COVERAGE=ON -DFETCHCONTENT_QUIET=OFF
	@echo "\n[2/4] 编译并执行单元测试以生成 .gcda 数据文件..."
	cmake --build $(DEBUG_DIR) --target utf8_test -j $(JOBS)
	./$(OUTPUT_DIR)/debug/bin/utf8_test
	@echo "\n[3/4] 生成 llvm-gcov 包装器并提取 LCOV 数据..."
	@mkdir -p $(OUTPUT_DIR)/coverage
	@echo "#!/bin/bash" > $(OUTPUT_DIR)/coverage/llvm-gcov.sh
	@echo 'exec llvm-cov gcov "$$@"' >> $(OUTPUT_DIR)/coverage/llvm-gcov.sh
	@chmod +x $(OUTPUT_DIR)/coverage/llvm-gcov.sh
	lcov --gcov-tool $(OUTPUT_DIR)/coverage/llvm-gcov.sh --capture --directory $(DEBUG_DIR) --output-file $(OUTPUT_DIR)/coverage/raw.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent
	@echo "\n[3.5/4] 剔除外部库、系统头文件和测试框架本身的代码..."
	lcov --remove $(OUTPUT_DIR)/coverage/raw.info '/usr/*' '*/tests/*' '*/_deps/*' '*/benchmarks/*' '*/c++/*' --output-file $(OUTPUT_DIR)/coverage/filtered.info --rc branch_coverage=1 --ignore-errors unused
	@echo "\n[4/4] 生成终极 HTML 覆盖率报告..."
	genhtml $(OUTPUT_DIR)/coverage/filtered.info --output-directory $(OUTPUT_DIR)/coverage/report --branch-coverage --ignore-errors mismatch,inconsistent
	@echo "\n✅ 挑战完成！覆盖率报告已生成，请在浏览器中打开以下链接查看："
	@echo "file://$(shell pwd)/$(OUTPUT_DIR)/coverage/report/index.html"

clean:
	@echo "清理构建目录和输出产物..."
	rm -rf $(DEBUG_DIR) $(RELEASE_DIR) $(OUTPUT_DIR)
	@echo "清理完成！(保留了 unicode_cache 避免重复下载)"