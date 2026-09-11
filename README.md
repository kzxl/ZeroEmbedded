# ⚡ ZeroEmbedded: Sovereign Hybrid C + Rust Embedded Framework & Toolchain

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: Embedded MCU](https://img.shields.io/badge/Target-ARM%20Cortex--M%20%7C%20RISC--V%20%7C%20AVR-orange.svg)]()
[![Runtime: Zero Overhead](https://img.shields.io/badge/Runtime-0%20Overhead%20(No%20GC%20%7C%20No%20VM)-brightgreen.svg)]()
[![Tests: 138 Passed](https://img.shields.io/badge/Tests-138%2F138%20Passing%20(100%25)-brightgreen.svg)]()
[![Performance: Zero-Cost](https://img.shields.io/badge/Overhead-0.903x%20(Zero--Cost%20Verified)-blueviolet.svg)]()
[![Static Analysis: Enforced](https://img.shields.io/badge/Safety-Context%20Analyzer%20(ISR%20%7C%20DMA)-success.svg)]()

**ZeroEmbedded** is an enterprise-grade sovereign embedded framework and toolchain engineered for deterministic real-time microcontrollers. It bridges the gap between **universal C hardware compatibility** and **Rust compile-time memory/concurrency safety** with zero runtime overhead.

---

## 🏛️ Architecture & System Structure

ZeroEmbedded is engineered around a sovereign dual-tier foundation:
- **C Foundation Tier (`core-c/`)**: Zero-cost primitives, $O(1)$ bitmap memory pools, linear arenas, lock-free SPSC queues, and the ZeroWire framing engine.
- **Rust Safety Tier (`rust/`)**: Strictly `#![no_std]` crates providing Type-State peripheral drivers (`zero-hal`) and compile-time DMA ownership tokens.
- **Static Tooling & Verification (`tooling/`)**: Clang AST analyzers enforcing context safety in ISR/DMA routines at CI time without runtime overhead.

👉 **[Read the Complete Embedded Architecture & Safety Invariants Specification](docs/architect/embedded-architecture.md)**

---

## ⚡ Verified Benchmark Performance Matrix (Phase 8)

### 🖥️ Hardware & Execution Environment Testbed (HEPM Provenance)
All benchmarks are empirically measured with reproducible hardware counters under the following certified testbed:

| Parameter | Specification & Host Environment |
| :--- | :--- |
| **Host Processor** | Intel(R) Core(TM) i5-10400 CPU @ 2.90GHz (Base: 2.90 GHz, Turbo: up to 4.30 GHz) |
| **CPU Topology** | 6 Physical Cores, 12 Logical Processors (12 MB Intel® Smart Cache) |
| **System Memory** | 32 GB DDR4 (Dual-Channel) |
| **Operating System** | Microsoft Windows 10 Pro 64-bit (Build 19045, x86_64) |
| **Compiler & Flags** | MSVC v19.44.35225 (`cl.exe /W4 /O2 /I core-c/include`) |
| **High-Res Timer** | Windows QueryPerformanceCounter (QPC Frequency: 10,000,000 Hz, 100 ns resolution) |
| **Run Conditions** | Single-threaded real-time loop, 10,000-iteration warm-up before timing |

### Benchmark Results Table
| Benchmark Component | Workload Iterations | Baseline Time (C / CRT) | Framework Time | Measured Metric & Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **`fw_span_t` Slicing vs Raw Pointer** | 10,000,000 | 61.22 ms | **59.27 ms** | **0.968x** *(Zero-Cost Verified, overhead $\le 1.02x$)* |
| **`fw_pool` vs CRT `malloc/free`** | 2,000,000 | 116.13 ms | **37.88 ms** | **3.1x Faster** *(18.94 ns/alloc-free, O(1) Bitset safe)* |
| **`fw_buffer_t` vs Pointer-Bump** | 5,000,000 | 4.06 ms | **6.62 ms** | **1.629x** *(Inlined bounds-checked write)* |
| **SPSC Lockless RingBuffer** | 5,000,000 | N/A | **16.79 ms** | **297.8 Million Ops/sec** *(3.36 ns per push+pop)* |
| **ZeroWire Packet Full Cycle** | 500,000 | N/A | **64.01 ms** | **178.8 MB/s** *(128 ns/frame with LUT CRC16-CCITT)* |

### Flash / Binary Footprint Analysis
```text
buffer.obj: 1.1 KB  |  assert.obj: 1.2 KB  |  pool.obj: 1.9 KB
arena.obj:  2.5 KB  |  spsc.obj:   2.5 KB  |  zerowire.obj: 2.7 KB
```
*Total compiled Flash footprint for the entire Core C Foundation is **under 12 KB**, shrinking to **3–4 KB** with `-Os` and LTO on ARM Cortex-M targets.*

---

## 🚀 Quick Start

### 1. One-Step Automated Build & Verification
```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_and_test.ps1
```

### 2. Standard CMake & CTest Workflow
```bash
# Configure and build all targets (core library, tests, benchmarks, examples)
cmake -B build -S .
cmake --build build --config Release

# Run hardened test suite (Core Memory + HAL & Tasklet Tests)
ctest --test-dir build -C Release --output-on-failure
```

### 3. Run Static Context Analyzer (Zero Violation Discipline)
```bash
python tooling/analyzer/zero_analyzer.py core-c examples
```

### 4. Run Demonstrations
```bash
# Host Simulator (Virtual GPIO, UART Loopback, and Tasklet Dispatcher)
.\build\Release\example_host_sim.exe

# STM32 MCU Target PoC
.\build\Release\example_stm32_poc.exe

# Performance Benchmark Suite (with HEPM logging)
.\build\Release\bench_suite.exe
```

---

## 📄 Authors & License

Architected and developed by **Phong Võ** (`kzxl`). Released under the **MIT License**. Part of the **ZeroUniverse** industrial computing ecosystem.
