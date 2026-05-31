#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SysY2022 编译器边界测试脚本
测试编译器是否能正确识别各类编译错误
用法: python3 run_boundary.py <test_dir> [--compiler PATH]

每个测试点包含：
  - .sy 文件：包含特定编译错误的源程序
  - .out 文件：包含预期错误信息的关键词

测试通过标准：编译器 stderr 输出中包含预期关键词
"""

import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path

COMPILER_DEFAULT = "./build/compiler"
TIMEOUT_DEFAULT = 30

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
RESET = "\033[0m"

def print_pass(msg):
    print(f"{GREEN}[PASS]{RESET} {msg}")

def print_fail(msg, detail=""):
    print(f"{RED}[FAIL]{RESET} {msg}")
    if detail:
        for line in detail.strip().split("\n"):
            print(f"      {line}")

def print_info(msg):
    print(f"[INFO] {msg}")

def run_single_test(sy_file, compiler, timeout):
    base = sy_file.stem
    test_dir = sy_file.parent
    out_file = test_dir / f"{base}.out"

    if not out_file.exists():
        return False, f"Missing expected output: {out_file}"

    # 读取预期错误关键词
    with open(out_file, 'r') as f:
        expected = f.read().strip()

    # 编译（预期失败）
    asm_file = test_dir / f"{base}.s"
    cmd = [compiler, str(sy_file), "-o", str(asm_file)]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, f"Compiler timeout ({timeout}s)"
    except Exception as e:
        return False, f"Compiler exception: {e}"

    # 清理生成的文件
    if asm_file.exists():
        asm_file.unlink()

    # 检查 stderr 中是否包含预期关键词
    stderr_lower = r.stderr.lower()
    expected_lower = expected.lower()

    if expected_lower in stderr_lower:
        return True, f"Expected error found: '{expected}'"
    else:
        return False, f"Expected '{expected}' not found in stderr.\nActual stderr:\n{r.stderr}"

def run_all_tests(test_dir, compiler, timeout):
    sy_files = sorted(Path(test_dir).glob("*.sy"))
    if not sy_files:
        print(f"{YELLOW}[WARN]{RESET} No .sy files found in {test_dir}")
        return 0, 0

    total = len(sy_files)
    passed = 0
    fail_list = []

    print_info(f"Found {total} boundary test cases in {test_dir}")
    print_info(f"Compiler: {compiler} | Timeout: {timeout}s")
    print("-" * 70)

    for i, sy_file in enumerate(sy_files, 1):
        base = sy_file.stem
        ok, detail = run_single_test(sy_file, compiler, timeout)
        if ok:
            passed += 1
            print(f"[{i:3d}/{total}] {GREEN}PASS{RESET} {base:35s} -> {detail}")
        else:
            fail_list.append((base, detail))
            print(f"[{i:3d}/{total}] {RED}FAIL{RESET} {base:35s}")

    print("=" * 70)
    fail_total = total - passed
    print_info(f"Results: {passed}/{total} passed ({passed*100//total if total else 0}%) | {fail_total} failed")

    if fail_total > 0:
        print(f"\n{RED}=== Failed Cases ==={RESET}")
        for name, detail in fail_list:
            print_fail(name, detail)

    return passed, total

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SysY2022 Compiler Boundary Test")
    parser.add_argument("test_dir", help="Directory with .sy/.out files")
    parser.add_argument("--compiler", default=COMPILER_DEFAULT)
    parser.add_argument("--timeout", type=int, default=TIMEOUT_DEFAULT)
    args = parser.parse_args()

    tool_name = args.compiler.split()[0]
    if not shutil.which(tool_name) and not Path(args.compiler).exists():
        print(f"{RED}[ERROR]{RESET} Compiler not found: {args.compiler}")
        sys.exit(1)

    run_all_tests(args.test_dir, args.compiler, args.timeout)
