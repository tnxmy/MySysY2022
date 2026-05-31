#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import subprocess
import argparse
import shutil
from pathlib import Path
from enum import Enum

COMPILER_DEFAULT = "./build/compiler"
SYLIB_DEFAULT = "./sylib_stub.c"
RISCV_GCC = "riscv64-linux-gnu-gcc"
QEMU = "qemu-riscv64"
TIMEOUT_DEFAULT = 180

GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
BLUE = "\033[94m"
RESET = "\033[0m"

class FailType(Enum):
    COMPILE = "COMPILE"
    LINK = "LINK"
    RUNTIME = "RUNTIME"
    WRONG_ANSWER = "WRONG_ANSWER"
    MISSING_OUTPUT = "MISSING_OUTPUT"

def print_pass(msg):
    print(f"{GREEN}[PASS]{RESET} {msg}")

def print_fail(msg, ft=None):
    tag = f"{RED}[{ft.value}]{RESET}" if ft else f"{RED}[FAIL]{RESET}"
    print(f"{tag} {msg}")

def print_warn(msg):
    print(f"{YELLOW}[WARN]{RESET} {msg}")

def print_info(msg):
    print(f"[INFO] {msg}")

def print_stage(msg):
    print(f"{BLUE}[{msg}]{RESET}", end=" ")

def run_single_test(sy_file, compiler, sylib, timeout):
    base = sy_file.stem
    test_dir = sy_file.parent
    in_file = test_dir / f"{base}.in"
    out_file = test_dir / f"{base}.out"
    ans_file = test_dir / f"{base}.ans"

    if not out_file.exists():
        return False, FailType.MISSING_OUTPUT, f"Missing expected output: {out_file}"

    # Step 1: Compile
    print_stage("COMPILE")
    asm_file = test_dir / f"{base}.s"
    cmd = [compiler, str(sy_file), "-o", str(asm_file)]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        if r.returncode != 0:
            return False, FailType.COMPILE, f"Compiler exit={r.returncode}:\n{r.stderr}"
    except subprocess.TimeoutExpired:
        return False, FailType.COMPILE, f"Compiler timeout ({timeout}s)"
    except Exception as e:
        return False, FailType.COMPILE, f"Compiler exception: {e}"

    # Step 2: Link
    print_stage("LINK")
    exe_file = test_dir / f"{base}_riscv"
    cmd = [RISCV_GCC, str(asm_file), sylib, "-o", str(exe_file), "-static"]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        if r.returncode != 0:
            return False, FailType.LINK, f"Linker exit={r.returncode}:\n{r.stderr}"
    except subprocess.TimeoutExpired:
        return False, FailType.LINK, f"Linker timeout ({timeout}s)"
    except Exception as e:
        return False, FailType.LINK, f"Linker exception: {e}"

    # Step 3: Run with QEMU
    print_stage("RUN")
    cmd = [QEMU, str(exe_file)]
    stdin_src = None
    if in_file.exists():
        stdin_src = open(in_file, "rb")
    try:
        r = subprocess.run(cmd, stdin=stdin_src, capture_output=True, text=True, timeout=timeout)
        if stdin_src:
            stdin_src.close()
        if r.returncode != 0:
            return False, FailType.RUNTIME, f"Runtime exit={r.returncode}:\nstderr={r.stderr}\nstdout={r.stdout}"
        with open(ans_file, "w") as f:
            f.write(r.stdout)
    except subprocess.TimeoutExpired:
        if stdin_src:
            stdin_src.close()
        return False, FailType.RUNTIME, f"Runtime timeout ({timeout}s)"
    except Exception as e:
        if stdin_src:
            stdin_src.close()
        return False, FailType.RUNTIME, f"Runtime exception: {e}"

    # Step 4: Diff
    print_stage("DIFF")
    try:
        r = subprocess.run(["diff", "-w", "--strip-trailing-cr", "-u", str(out_file), str(ans_file)],
                 capture_output=True, text=True)
        if r.returncode == 0:
            return True, None, "Output matches"
        diff_text = r.stdout
        if len(diff_text) > 500:
            diff_text = diff_text[:500] + "\n... (truncated)"
        return False, FailType.WRONG_ANSWER, f"Output mismatch:\n{diff_text}"
    except Exception as e:
        return False, FailType.WRONG_ANSWER, f"Diff exception: {e}"
    finally:
        for f in [asm_file, exe_file]:
            if f.exists():
                f.unlink()

def run_all_tests(test_dir, compiler, sylib, timeout):
    sy_files = sorted(Path(test_dir).glob("*.sy"))
    if not sy_files:
        print_warn(f"No .sy files found in {test_dir}")
        return 0, 0, {}

    total = len(sy_files)
    passed = 0
    fail_stats = {ft: [] for ft in FailType}

    print_info(f"Found {total} test cases in {test_dir}")
    print_info(f"Compiler: {compiler} | Sylib: {sylib} | Timeout: {timeout}s")
    print("-" * 70)

    for i, sy_file in enumerate(sy_files, 1):
        base = sy_file.stem
        ok, ft, detail = run_single_test(sy_file, compiler, sylib, timeout)
        if ok:
            passed += 1
            print(f"\r[{i:3d}/{total}] {GREEN}PASS{RESET} {base:30s}")
        else:
            fail_stats[ft].append((base, detail))
            print(f"\r[{i:3d}/{total}] {RED}{ft.value:12s}{RESET} {base:30s}")

    print("=" * 70)
    fail_total = total - passed
    print_info(f"Results: {passed}/{total} passed ({passed*100//total}%) | {fail_total} failed")

    if fail_total > 0:
        print(f"\n{RED}=== Failure Breakdown ==={RESET}")
        for ft in FailType:
            cases = fail_stats[ft]
            if not cases:
                continue
            print(f"\n{RED}[{ft.value}] {len(cases)} cases{RESET}")
            for name, detail in cases[:5]:
                print(f"  - {name}")
                for line in detail.strip().split("\n"):
                    print(f"      {line}")
            if len(cases) > 5:
                print(f"  ... and {len(cases)-5} more")

    return passed, total, fail_stats

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="SysY2022 Compiler Test Runner")
    parser.add_argument("test_dir", help="Directory with .sy/.in/.out files")
    parser.add_argument("--compiler", default=COMPILER_DEFAULT)
    parser.add_argument("--sylib", default=SYLIB_DEFAULT)
    parser.add_argument("--timeout", type=int, default=TIMEOUT_DEFAULT)
    args = parser.parse_args()

    for tool in [args.compiler, RISCV_GCC, QEMU]:
        tool_name = tool.split()[0]
        if not shutil.which(tool_name) and not Path(tool).exists():
            print_fail(f"Required tool not found: {tool}")
            sys.exit(1)

    run_all_tests(args.test_dir, args.compiler, args.sylib, args.timeout)