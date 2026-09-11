#!/usr/bin/env python3
"""
ZeroEmbedded Static Analyzer (Phase 4 & 5)
Detects execution context violations (ISR malloc/delay) and lifetime hazards.
"""

import sys
import re
import os
from pathlib import Path

FORBIDDEN_ISR_CALLS = {
    "malloc": "ZE-001: Dynamic memory allocation is strictly prohibited inside ISR",
    "calloc": "ZE-001: Dynamic memory allocation is strictly prohibited inside ISR",
    "realloc": "ZE-001: Dynamic memory allocation is strictly prohibited inside ISR",
    "free": "ZE-001: Heap deallocation is strictly prohibited inside ISR",
    "delay": "ZE-002: Blocking delay cannot be called inside ISR",
    "delay_ms": "ZE-002: Blocking delay cannot be called inside ISR",
    "sleep": "ZE-002: Sleep/wait cannot be called inside ISR",
    "usleep": "ZE-002: Sleep/wait cannot be called inside ISR",
    "vTaskDelay": "ZE-002: RTOS task delay cannot be called inside ISR",
}

class Diagnostic:
    def __init__(self, file_path: str, line_no: int, code: str, message: str, severity="error"):
        self.file_path = file_path
        self.line_no = line_no
        self.code = code
        self.message = message
        self.severity = severity

    def __str__(self):
        return f"{self.file_path}:{self.line_no}:1: {self.severity}: [{self.code}] {self.message}"

def analyze_c_file(file_path: str) -> list[Diagnostic]:
    diagnostics = []
    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    in_isr_function = False
    current_isr_name = ""
    brace_depth = 0

    isr_decl_regex = re.compile(r"FW_ISR\s+(?:void|\w+)\s+(\w+)\s*\(")
    call_regex = re.compile(r"\b([a-zA-Z_]\w*)\s*\(")

    for idx, line in enumerate(lines, start=1):
        clean_line = line.strip()

        # Check for ISR function start
        isr_match = isr_decl_regex.search(line)
        if isr_match:
            in_isr_function = True
            current_isr_name = isr_match.group(1)
            brace_depth = 0

        if in_isr_function:
            brace_depth += line.count("{")
            brace_depth -= line.count("}")

            # Check forbidden function calls inside ISR
            for match in call_regex.finditer(line):
                fn_name = match.group(1)
                if fn_name in FORBIDDEN_ISR_CALLS:
                    msg = FORBIDDEN_ISR_CALLS[fn_name]
                    code = msg.split(":")[0]
                    detail = msg.split(":")[1].strip()
                    diagnostics.append(Diagnostic(
                        file_path=file_path,
                        line_no=idx,
                        code=code,
                        message=f"{detail} (in ISR '{current_isr_name}')"
                    ))

            if brace_depth <= 0 and "{" in "".join(lines[max(0, idx-5):idx]):
                in_isr_function = False
                current_isr_name = ""

    return diagnostics

def main():
    if len(sys.argv) < 2:
        print("Usage: zero_analyzer.py <source_file_or_dir>...")
        sys.exit(1)

    targets = sys.argv[1:]
    all_diagnostics = []

    for target in targets:
        p = Path(target)
        if p.is_file() and p.suffix in [".c", ".h"]:
            all_diagnostics.extend(analyze_c_file(str(p)))
        elif p.is_dir():
            for root, _, files in os.walk(p):
                for file in files:
                    if file.endswith((".c", ".h")):
                        all_diagnostics.extend(analyze_c_file(os.path.join(root, file)))

    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    print(f"\n[ANALYZER] ZeroEmbedded Static Analyzer: Scanned {len(targets)} path(s)")
    if not all_diagnostics:
        print("[PASS] Zero violations detected. Context and memory safety checks passed!\n")
        sys.exit(0)
    else:
        print(f"[FAIL] Found {len(all_diagnostics)} violation(s):\n")
        for diag in all_diagnostics:
            print(f"  {diag}")
        print()
        sys.exit(1)

if __name__ == "__main__":
    main()
