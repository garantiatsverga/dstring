#!/usr/bin/env python3

import os
import sys
import subprocess
import argparse
import time
import platform
import shutil
import re
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

def print_warning(text):
    print(f"{Colors.YELLOW}⚠ {text}{Colors.RESET}")

def print_info(text):
    print(f"{Colors.DIM}  {text}{Colors.RESET}")

# === Config ===
DSTRING_HEADER = "dstring.h"
TESTS = {
    "strtest": {
        "src": "tests/strtest.c",
        "out": "strtest",
        "desc": "Unit tests",
        "timeout": 60,
        "lang": "c",
        "ram_mb": 10,
        "dangerous": False,
    },
    "strbench": {
        "src": "tests/strbench.c",
        "out": "strbench",
        "desc": "Performance benchmarks (C)",
        "timeout": 120,
        "lang": "c",
        "ram_mb": 100,
        "dangerous": False,
    },
    "strbench_cpp": {
        "src": "tests/strbench.cpp",
        "out": "strbench_cpp",
        "desc": "Performance benchmarks (C++)",
        "timeout": 120,
        "lang": "cpp",
        "ram_mb": 200,
        "dangerous": False,
        "std": "c++17",
    },
    "membench": {
        "src": "tests/membench.c",
        "out": "membench",
        "desc": "Memory benchmarks (C)",
        "timeout": 120,
        "lang": "c",
        "ram_mb": 500,
        "dangerous": False,
    },
    "membench_cpp": {
        "src": "tests/membench.cpp",
        "out": "membench_cpp",
        "desc": "Memory benchmarks (C++)",
        "timeout": 120,
        "lang": "cpp",
        "ram_mb": 700,
        "dangerous": False,
        "std": "c++17",
    },
    "stress": {
        "src": "tests/stress.c",
        "out": "stress",
        "desc": "Stress tests - dstring (2GB arena)",
        "timeout": 300,
        "lang": "c",
        "ram_mb": 2048,
        "dangerous": True,
    },
    "stress_cpp": {
        "src": "tests/stress.cpp",
        "out": "stress_cpp",
        "desc": "Stress tests - std::string",
        "timeout": 300,
        "lang": "cpp",
        "ram_mb": 2048,
        "dangerous": True,
        "std": "c++17",
    },
}

# === Utils ===
def get_available_ram_mb():
    """Get available RAM in MB."""
    try:
        if os.path.exists('/proc/meminfo'):
            with open('/proc/meminfo', 'r') as f:
                for line in f:
                    if line.startswith('MemAvailable:'):
                        return int(line.split()[1]) // 1024
        elif sys.platform == 'darwin':
            result = subprocess.run(['sysctl', 'hw.memsize'], capture_output=True, text=True)
            if result.returncode == 0:
                total_bytes = int(result.stdout.split(':')[1].strip())
                return total_bytes // (1024 * 1024)
        elif sys.platform == 'win32':
            import ctypes
            class MEMORYSTATUSEX(ctypes.Structure):
                _fields_ = [
                    ("dwLength", ctypes.c_ulong),
                    ("dwMemoryLoad", ctypes.c_ulong),
                    ("ullTotalPhys", ctypes.c_ulonglong),
                    ("ullAvailPhys", ctypes.c_ulonglong),
                    ("ullTotalPageFile", ctypes.c_ulonglong),
                    ("ullAvailPageFile", ctypes.c_ulonglong),
                    ("ullTotalVirtual", ctypes.c_ulonglong),
                    ("ullAvailVirtual", ctypes.c_ulonglong),
                    ("ullAvailExtendedVirtual", ctypes.c_ulonglong),
                ]
            memory_status = MEMORYSTATUSEX()
            memory_status.dwLength = ctypes.sizeof(MEMORYSTATUSEX)
            ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(memory_status))
            return memory_status.ullAvailPhys // (1024 * 1024)
    except:
        pass
    return None

def get_compiler(lang="c"):
    """Detect available compiler for the specified language."""
    if lang == "cpp":
        compilers = ["g++-14", "g++-13", "g++-12", "g++-11", "g++", "clang++"]
    else:
        compilers = ["gcc-14", "gcc-13", "gcc-12", "gcc-11", "gcc", "clang"]
    
    for cc in compilers:
        try:
            result = subprocess.run([cc, "--version"], capture_output=True, check=False)
            if result.returncode == 0:
                return cc
        except FileNotFoundError:
            continue
    return None

def get_optimization_flags(compiler):
    """Get default optimization flags."""
    flags = ["-O3"]
    if "clang" not in compiler:
        flags.append("-pipe")
    return flags

def get_std_flags(lang="c", cpp_std="c++17"):
    """Get standard flags based on language."""
    if lang == "cpp":
        return [f"-std={cpp_std}", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-parameter"]
    return ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wno-unused-parameter"]

def run_command(cmd, cwd=".", timeout=60):
    """Run command, return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            timeout=timeout,
            shell=False
        )
        return result.returncode, result.stdout or "", result.stderr or ""
    except subprocess.TimeoutExpired:
        return -1, "", f"Timeout after {timeout}s"
    except Exception as e:
        return -1, "", str(e)

def ask_yes_no(question, default=None):
    """Ask user a yes/no question."""
    if default is None:
        prompt = f"{question} [y/n]: "
    elif default:
        prompt = f"{question} [Y/n]: "
    else:
        prompt = f"{question} [y/N]: "
    
    while True:
        answer = input(prompt).strip().lower()
        if answer == '' and default is not None:
            return default
        if answer in ['y', 'yes']:
            return True
        if answer in ['n', 'no']:
            return False
        print_info("Please answer 'y' or 'n'")

def check_dstring_header():
    """Check if dstring.h exists."""
    if os.path.isfile(DSTRING_HEADER):
        print_success(f"{DSTRING_HEADER} found")
        return True
    elif os.path.isfile(os.path.join("..", DSTRING_HEADER)):
        print_success(f"{DSTRING_HEADER} found in parent directory")
        return True
    else:
        print_error(f"{DSTRING_HEADER} not found!")
        return False

def build_test(test_name, compiler, cflags, opt_flags, hash_strategy=None):
    """Build a single test."""
    test = TESTS[test_name]
    src = test["src"]
    out = test["out"]
    lang = test.get("lang", "c")

    if not os.path.isfile(src):
        print_error(f"Source {src} not found")
        return None

    # Add POSIX define for clock_gettime (C tests only)
    extra_flags = []
    if lang == "c":
        extra_flags.append("-D_POSIX_C_SOURCE=199309L")
    
    # For C++ tests, add _GNU_SOURCE for additional compatibility
    if lang == "cpp":
        extra_flags.append("-D_GNU_SOURCE")
    
    # Add hash strategy if specified
    if hash_strategy is not None and lang == "c":
        extra_flags.append(f"-DDS_HASH_STRATEGY={hash_strategy}")
    
    # Add include path for dstring.h
    include_paths = []
    if os.path.isfile(DSTRING_HEADER):
        include_paths.append("-I.")
    elif os.path.isfile(os.path.join("..", DSTRING_HEADER)):
        include_paths.append("-I..")
    
    cmd = [compiler] + opt_flags + cflags + extra_flags + include_paths + [
        "-o", out, src
    ]
    print_info(f"{' '.join(cmd)}")

    returncode, stdout, stderr = run_command(cmd, timeout=60)

    if returncode != 0:
        print_error(f"Build failed for {test_name}")
        if stderr:
            lines = stderr.split('\n')
            critical_errors = [l for l in lines if 'error:' in l.lower()]
            warnings = [l for l in lines if 'warning:' in l.lower()]
            
            if critical_errors:
                print("  Critical errors:")
                for l in critical_errors[:10]:
                    print(f"    {l}")
            if warnings and len(warnings) < 5:
                print("  Warnings:")
                for l in warnings[:5]:
                    print(f"    {l}")
            elif warnings:
                print_info(f"  ({len(warnings)} warnings)")
        return None

    print_success(f"{test_name} built → ./{out}")
    return out

def run_test(test_name):
    """Run a built test and display full output."""
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
        # ALWAYS show full output
        if stdout:
            print(stdout)
        return True
    else:
        print_error(f"{test_name} failed (exit {returncode}, {elapsed:.2f}s)")
        # Always show everything for failures
        if stderr:
            print("STDERR:")
            print(stderr)
        if stdout:
            print("STDOUT:")
            print(stdout)
        return False

# === Main ===
def main():
    parser = argparse.ArgumentParser(description="Build and test dstring.h")
    parser.add_argument("--clean", action="store_true", help="Remove binaries before build")
    parser.add_argument("--test", choices=list(TESTS.keys()), help="Run specific test")
    parser.add_argument("--build-only", action="store_true", help="Only build, don't run")
    parser.add_argument("--compiler", default=None, help="Specify compiler (gcc, clang, g++, clang++)")
    parser.add_argument("--cflags", default="", help="Extra compiler flags")
    parser.add_argument("--no-color", action="store_true", help="Disable colored output")
    parser.add_argument("--hash-strategy", type=int, choices=[0, 1, 2], 
                       help="Hash strategy: 0=eager, 1=lazy, 2=hybrid (default)")
    parser.add_argument("--no-prompt", action="store_true", help="Don't ask before running tests")
    parser.add_argument("--skip-ram-check", action="store_true", help="Skip RAM availability check")
    parser.add_argument("--cpp-std", default="c++17", 
                       help="C++ standard (default: c++17)")
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
    
    # Show available RAM
    available_ram = get_available_ram_mb()
    if available_ram:
        print_info(f"Available RAM: ~{available_ram} MB ({available_ram // 1024} GB)")
    else:
        print_info("Available RAM: Unknown")

    # Check dstring.h
    if not check_dstring_header():
        sys.exit(1)

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

    # Show what will be built
    print_section("Test Plan")
    for test_name in test_list:
        test = TESTS[test_name]
        ram_req = test.get("ram_mb", 100)
        dangerous = "⚠" if test.get("dangerous", False) else " "
        std_info = f" [{test.get('std', '')}]" if test.get('std') else ""
        print_info(f"  {dangerous} {test_name:<20} - {test['desc']} (~{ram_req} MB RAM){std_info}")
    
    # Ask user if they want to continue
    if not args.no_prompt and not args.build_only:
        if not ask_yes_no("\nProceed with building and running these tests?", default=True):
            print_info("Aborted by user")
            sys.exit(0)

    # Build
    print_section("Building tests")
    built = []
    build_failed = []
    
    for test_name in test_list:
        test = TESTS[test_name]
        lang = test.get("lang", "c")
        
        # Select compiler based on language
        if args.compiler:
            compiler = args.compiler
        else:
            compiler = get_compiler(lang)
        
        if not compiler:
            print_error(f"No {'C++' if lang == 'cpp' else 'C'} compiler found for {test_name}")
            build_failed.append(test_name)
            continue
        
        # Get language-specific flags
        cpp_std = test.get("std", args.cpp_std)
        cflags = get_std_flags(lang, cpp_std)
        if args.cflags:
            cflags.extend(args.cflags.split())
        opt_flags = get_optimization_flags(compiler)
        
        print_info(f"\nBuilding {test_name} ({lang.upper()}) with {compiler}")
        print_info(f"  Description: {test['desc']}")
        if lang == "cpp":
            print_info(f"  C++ Standard: {cpp_std}")
        
        out = build_test(test_name, compiler, cflags, opt_flags, args.hash_strategy)
        if out:
            built.append(test_name)
        else:
            build_failed.append(test_name)

    if not built:
        print_error("No tests built successfully")
        if build_failed:
            print_info(f"Failed: {', '.join(build_failed)}")
        sys.exit(1)

    # Run
    if not args.build_only:
        print_section("Running tests")
        passed = 0
        failed = 0
        skipped = 0
        
        for test_name in built:
            # Check RAM for dangerous tests
            if not args.skip_ram_check:
                test = TESTS[test_name]
                required_ram = test.get("ram_mb", 100)
                available_ram = get_available_ram_mb()
                
                if available_ram and available_ram < required_ram:
                    print_warning(f"Skipping {test_name}: needs ~{required_ram} MB RAM, have ~{available_ram} MB")
                    if not args.no_prompt:
                        if ask_yes_no("Run anyway?", default=False):
                            pass
                        else:
                            skipped += 1
                            continue
            
            if run_test(test_name):
                passed += 1
            else:
                failed += 1

        print_section("Summary")
        print(f"  Total: {passed + failed + skipped}")
        print(f"  {Colors.GREEN}Passed: {passed}{Colors.RESET}")
        if failed > 0:
            print(f"  {Colors.RED}Failed: {failed}{Colors.RESET}")
        else:
            print(f"  Failed: 0")
        if skipped > 0:
            print(f"  {Colors.YELLOW}Skipped: {skipped}{Colors.RESET}")
        
        if build_failed:
            print(f"  {Colors.RED}Build failed: {len(build_failed)}{Colors.RESET}")
            for t in build_failed:
                print(f"    - {t}")
        
        print("")

        if failed > 0:
            sys.exit(1)
    else:
        print_success("Build complete (--build-only)")
        if build_failed:
            print_error(f"Failed to build: {', '.join(build_failed)}")
            sys.exit(1)

if __name__ == "__main__":
    main()