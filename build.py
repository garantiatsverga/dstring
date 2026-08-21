#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
import time
import platform
from pathlib import Path

# === Colors ===
class Colors:
    RESET  = "\033[0m"
    RED    = "\033[31m"
    GREEN  = "\033[32m"
    YELLOW = "\033[33m"
    BLUE   = "\033[34m"
    MAGENTA= "\033[35m"
    CYAN   = "\033[36m"
    BOLD   = "\033[1m"
    DIM    = "\033[2m"

def print_header(text):
    print(f"\n{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}  {text}{Colors.RESET}")
    print(f"{Colors.BOLD}{Colors.CYAN}{'='*60}{Colors.RESET}")

def print_section(text):
    print(f"\n{Colors.BOLD}{Colors.YELLOW}▶ {text}{Colors.RESET}")

def print_success(text):
    print(f"{Colors.GREEN}✓ {text}{Colors.RESET}")

def print_error(text):
    print(f"{Colors.RED}✗ {text}{Colors.RESET}")

def print_info(text):
    print(f"{Colors.DIM}  {text}{Colors.RESET}")

# === Config ===
DSTRING_HEADER = "dstring.h"
TESTS = {
    "strtest": {
        "src": "tests/strtest.c",
        "out": "strtest",
        "desc": "Unit tests (10 tests)",
        "timeout": 30,
    },
    "strbench": {
        "src": "tests/strbench.c",
        "out": "strbench",
        "desc": "Performance benchmarks",
        "timeout": 120,
    },
    "stress": {
        "src": "tests/stress.c",
        "out": "stress",
        "desc": "Stress tests (2GB arena)",
        "timeout": 300,
    },
}

# === Utils ===
def get_compiler():
    """Detect available C compiler."""
    for cc in ["gcc-14", "gcc-13", "gcc-12", "gcc-11", "gcc", "clang"]:
        try:
            subprocess.run([cc, "--version"], capture_output=True, check=False)
            return cc
        except FileNotFoundError:
            continue
    return None

def get_optimization_flags(compiler):
    """Get default optimization flags."""
    if "clang" in compiler:
        return ["-O3"]
    return ["-O3", "-pipe"]

def get_std_flags():
    return ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-parameter"]

def run_command(cmd, cwd=".", timeout=60):
    """Run command, return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=False,  # Keep bytes to avoid decode errors
            timeout=timeout,
            shell=False
        )
        stdout = result.stdout.decode('utf-8', errors='replace') if result.stdout else ""
        stderr = result.stderr.decode('utf-8', errors='replace') if result.stderr else ""
        return result.returncode, stdout, stderr
    except subprocess.TimeoutExpired:
        return -1, "", f"Timeout after {timeout}s"
    except Exception as e:
        return -1, "", str(e)

# === Build functions ===
def check_dstring_header():
    """Check if dstring.h exists."""
    if not os.path.isfile(DSTRING_HEADER):
        print_error(f"{DSTRING_HEADER} not found!")
        return False
    print_success(f"{DSTRING_HEADER} found")
    return True

def build_test(test_name, compiler, cflags, opt_flags):
    """Build a single test."""
    test = TESTS[test_name]
    src = test["src"]
    out = test["out"]

    if not os.path.isfile(src):
        print_error(f"Source {src} not found")
        return None

    # FIXED: добавили -D_POSIX_C_SOURCE для clock_gettime
    cmd = [compiler] + opt_flags + cflags + [
        "-D_POSIX_C_SOURCE=199309L",
        "-o", out, src
    ]
    print_info(f"{' '.join(cmd)}")

    returncode, stdout, stderr = run_command(cmd, timeout=60)

    if returncode != 0:
        print_error(f"Build failed for {test_name}")
        if stderr:
            print(stderr)
        return None

    print_success(f"{test_name} built → ./{out}")
    return out

def run_test(test_name):
    """Run a built test."""
    test = TESTS[test_name]
    out = test["out"]

    if not os.path.isfile(out):
        print_error(f"{out} not found (run build first)")
        return False

    print_info(f"Running ./{out} ...")
    start_time = time.time()

    returncode, stdout, stderr = run_command([f"./{out}"], timeout=test["timeout"])

    elapsed = time.time() - start_time

    if returncode == 0:
        print_success(f"{test_name} passed ({elapsed:.2f}s)")
        if stdout:
            print(stdout)
        return True
    else:
        print_error(f"{test_name} failed (exit {returncode}, {elapsed:.2f}s)")
        if stderr:
            print(stderr)
        if stdout:
            print(stdout)
        return False

# === Main ===
def main():
    parser = argparse.ArgumentParser(description="Build and test dstring.h")
    parser.add_argument("--clean", action="store_true", help="Remove binaries before build")
    parser.add_argument("--all", action="store_true", help="Run all tests (default)")
    parser.add_argument("--test", choices=list(TESTS.keys()), help="Run specific test")
    parser.add_argument("--build-only", action="store_true", help="Only build, don't run")
    parser.add_argument("--compiler", default=None, help="Specify compiler (gcc, clang, etc.)")
    parser.add_argument("--cflags", default="", help="Extra compiler flags")
    parser.add_argument("--no-color", action="store_true", help="Disable colored output")
    args = parser.parse_args()

    # Disable colors if requested
    if args.no_color:
        Colors.RESET = Colors.RED = Colors.GREEN = Colors.YELLOW = ""
        Colors.BLUE = Colors.MAGENTA = Colors.CYAN = Colors.BOLD = Colors.DIM = ""

    print_header("dstring.h — Build & Test Runner")

    # Detect environment
    print_info(f"OS: {platform.system()} {platform.release()}")
    print_info(f"Python: {platform.python_version()}")
    print_info(f"Arch: {platform.machine()}")

    # Check dstring.h
    if not check_dstring_header():
        sys.exit(1)

    # Select compiler
    compiler = args.compiler or get_compiler()
    if not compiler:
        print_error("No C compiler found (gcc/clang required)")
        sys.exit(1)
    print_success(f"Compiler: {compiler}")

    # Build flags
    opt_flags = get_optimization_flags(compiler)
    cflags = get_std_flags()

    if args.cflags:
        cflags.extend(args.cflags.split())

    print_info(f"CFLAGS: {' '.join(cflags)}")
    print_info(f"OPT: {' '.join(opt_flags)}")

    # Clean
    if args.clean:
        print_section("Cleaning binaries")
        for test_name in TESTS:
            out = TESTS[test_name]["out"]
            if os.path.isfile(out):
                os.remove(out)
                print_info(f"Removed ./{out}")
            if os.path.isfile(out + ".exe"):
                os.remove(out + ".exe")
                print_info(f"Removed ./{out}.exe")

    # Determine which tests to run
    if args.test:
        test_list = [args.test]
    else:
        test_list = list(TESTS.keys())

    # Build
    print_section("Building tests")
    built = []
    for test_name in test_list:
        out = build_test(test_name, compiler, cflags, opt_flags)
        if out:
            built.append(test_name)

    if not built:
        print_error("No tests built")
        sys.exit(1)

    # Run
    if not args.build_only:
        print_section("Running tests")
        passed = 0
        failed = 0
        for test_name in built:
            if run_test(test_name):
                passed += 1
            else:
                failed += 1

        print_section("Summary")
        print(f"  Total: {passed + failed}")
        print(f"  {Colors.GREEN}Passed: {passed}{Colors.RESET}")
        if failed > 0:
            print(f"  {Colors.RED}Failed: {failed}{Colors.RESET}")
        else:
            print(f"  Failed: 0")
        print("")

        if failed > 0:
            sys.exit(1)
    else:
        print_success("Build complete (--build-only)")

if __name__ == "__main__":
    main()