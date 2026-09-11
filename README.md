# ⚡ ZeroEmbedded: Sovereign Hybrid C + Rust Embedded Framework & Toolchain

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: Embedded MCU](https://img.shields.io/badge/Target-ARM%20Cortex--M%20%7C%20RISC--V-orange.svg)]()
[![Runtime: Zero Overhead](https://img.shields.io/badge/Runtime-0%20Overhead%20(No%20GC%20%7C%20No%20VM)-brightgreen.svg)]()

**ZeroEmbedded** is a next-generation embedded systems framework and toolchain engineering a pragmatic bridge between **C ecosystem compatibility** and **Rust compile-time memory/concurrency safety**.

---

## 🎯 Architectural Principles

1. **Rule 1 — C is the Compatibility Layer**: Existing firmware, vendor BSPs, and silicon SDKs (ST HAL, NXP SDK, ESP-IDF) compile directly without forks.
2. **Rule 2 — Rust is the Safety Island**: Rust is strategically applied where memory safety, complex protocol parsers, and concurrency logic provide massive ROI.
3. **Rule 3 — Zero-Cost First, Runtime Safety Fallback**: Enforce invariants at compile-time via static analysis and types. Abstractions must compile down to identical machine code as hand-written C (`-O2` / `-O3`).
4. **Rule 4 — C ABI (`extern "C"`) is the Strict Boundary**: No leaky abstractions across language boundaries; hot execution loops never cross the FFI bridge.

---

## 🏛️ Directory Layout

```text
ZeroEmbedded/
├── core-c/                  # Phase 1 & 2: Core C Primitives & Memory Engines
│   ├── include/zero/        # types.h, span.h, result.h, assert.h, attributes.h
│   │   └── memory/          # pool.h, arena.h, buffer.h
│   └── src/
├── rust/                    # Phase 3: Rust Safety Crates (Cargo Workspace, #![no_std])
│   ├── Cargo.toml
│   └── crates/
│       ├── zero-core/       # Core types & FFI interop
│       ├── zero-memory/     # Safe buffer ownership, lifetime tracking
│       ├── zero-sync/       # Concurrency & interrupt-safe wrappers
│       └── zero-hal/        # Type-safe HAL traits
├── tooling/                 # Phase 4, 5 & 7: Static Analyzer & Hardware Codegen
│   ├── analyzer/            # Clang LibTooling / annotations (FW_ISR, FW_DMA, FW_OWNER)
│   └── codegen/             # CMSIS-SVD-to-Driver / register generator
├── hal/                     # Phase 6: Hardware Abstractions (STM32, RISC-V)
├── benchmarks/              # Phase 8: Cycle count, RAM/Flash, code size comparisons
├── tests/safety/            # Phase 9: Systematic UB, UAF, and ISR violation tests
└── examples/                # Phase 10: Verified hardware PoC firmware
```

---

## 🗺️ Implementation Roadmap

- **P0 Architecture**: Framework principles, ABI boundaries, and folder structure. *(Completed)*
- **P1 Core C Foundation**: Types, spans, results, string views, compiler attributes. *(In Progress)*
- **P2 Memory Safety**: Pool, Arena, Static allocators with zero heap dependencies.
- **P3 Rust Integration**: `#![no_std]` crates with C ABI extern boundaries.
- **P4 Static Analyzer**: Clang attributes for ownership (`FW_OWNER`, `FW_BORROW`).
- **P5 ISR/DMA Safety**: Execution context detection (`FW_ISR`, `FW_DMA`, `FW_NORMAL`).
- **P6 Type-Safe HAL**: Zero-cost register manipulation and pin state types.
- **P7 Hardware Generator**: CMSIS-SVD parser to C & Rust driver generators.
- **P8 Benchmark Engine**: Automated measurement of CPU cycles, RAM, and Flash footprint.
- **P9 Safety Test Suite**: Regression suite verifying caught memory and concurrency bugs.
- **P10 Real Firmware PoC**: End-to-end verification on real ARM Cortex-M hardware (e.g. STM32).
- **P11 RTOS Integration**: Adapters for FreeRTOS, Zephyr, and ThreadX.
- **P12 ZeroLang (Exploratory)**: Domain-specific embedded language targeting the verified framework.

---

## 📄 Licensing

Part of the **ZeroUniverse** ecosystem. Released under the **MIT License**.
