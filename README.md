# ⚡ ZeroEmbedded: Sovereign Hybrid C + Rust Embedded Framework & Toolchain

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target: Embedded MCU](https://img.shields.io/badge/Target-ARM%20Cortex--M%20%7C%20RISC--V%20%7C%20AVR-orange.svg)]()
[![Runtime: Zero Overhead](https://img.shields.io/badge/Runtime-0%20Overhead%20(No%20GC%20%7C%20No%20VM)-brightgreen.svg)]()
[![Tests: 114 Passed](https://img.shields.io/badge/Tests-114%2F114%20Passing%20(100%25)-brightgreen.svg)]()
[![Performance: Zero-Cost](https://img.shields.io/badge/Overhead-0.946x%20(Zero--Cost%20Verified)-blueviolet.svg)]()
[![Static Analysis: Enforced](https://img.shields.io/badge/Safety-Context%20Analyzer%20(ISR%20%7C%20DMA)-success.svg)]()

**ZeroEmbedded** is an enterprise-grade sovereign embedded framework and toolchain engineered for deterministic real-time microcontrollers. It bridges the gap between **universal C hardware compatibility** and **Rust compile-time memory/concurrency safety** with zero runtime overhead.

---

## 🏛️ Core Architectural Principles

- **Rule 1 — C is the Compatibility Layer**: Vendor SDKs (ST HAL, NXP SDK, ESP-IDF) and existing legacy firmware compile directly without code rewrites or custom compiler forks.
- **Rule 2 — Rust is the Safety Island**: Strategic deployment of Rust for memory-sensitive algorithms, communication protocols, concurrency state machines, and type-state peripheral drivers.
- **Rule 3 — Zero-Cost First, Runtime Safety Fallback**: Compile-time static analysis and type invariants eliminate overhead. Abstractions compile down to raw register assembly identical to hand-written C (`-O2` / `-O3`).
- **Rule 4 — C ABI (`extern "C"`) is the Strict Boundary**: No leaky abstractions across language boundaries; high-frequency execution loops never cross the FFI bridge.

---

## ⚡ Verified Benchmark Performance Matrix (Phase 8)

All benchmarks measured on 64-bit production builds (`MSVC /O2`) with high-resolution CPU performance counters:

| Benchmark Component | Workload Iterations | Baseline Time (C / CRT) | Framework Time | Measured Metric & Speedup |
| :--- | :---: | :---: | :---: | :---: |
| **`fw_span_t` Slicing vs Raw Pointer** | 10,000,000 | 25.92 ms | **24.52 ms** | **0.946x** *(Zero-Cost Verified, overhead $\le 1.02x$)* |
| **`fw_pool` vs CRT `malloc/free`** | 2,000,000 | 175.41 ms | **41.92 ms** | **4.2x Faster** *(20.96 ns/alloc-free, Double-Free protected)* |
| **`fw_buffer_t` vs Pointer-Bump** | 5,000,000 | 5.17 ms | **6.77 ms** | **1.310x** *(Inlined bounds-checked write)* |
| **SPSC Lockless RingBuffer** | 5,000,000 | N/A | **30.59 ms** | **163.5 Million Ops/sec** *(6.12 ns per push+pop)* |
| **ZeroWire Packet Full Cycle** | 500,000 | N/A | **357.29 ms** | **32.03 MB/s** *(714 ns/frame with CRC16-CCITT check)* |

### Flash / Binary Footprint Analysis
```text
buffer.obj: 1.1 KB  |  assert.obj: 1.2 KB  |  pool.obj: 1.9 KB
arena.obj:  2.5 KB  |  spsc.obj:   2.5 KB  |  zerowire.obj: 2.7 KB
```
*Total compiled Flash footprint for the entire Core C Foundation is **under 12 KB**, shrinking to **3–4 KB** with `-Os` and LTO on ARM Cortex-M targets.*

---

## 📦 Subsystems & Architecture Layout

```text
ZeroEmbedded/
├── core-c/                  # C Foundation Tier (Zero-cost primitives & memory engines)
│   ├── CMakeLists.txt       # Unified CMake build script
│   ├── include/zero/
│   │   ├── zero.h           # Master umbrella header
│   │   ├── types.h          # Fixed-width types, fw_size_t, fw_bool_t
│   │   ├── result.h         # fw_status_t, error codes, FW_CHECK() macro
│   │   ├── span.h           # fw_span_t, fw_cspan_t, fw_span_sub_safe()
│   │   ├── string_view.h    # fw_string_view_t (Zero-allocation string slice)
│   │   ├── assert.h         # FW_STATIC_ASSERT, FW_ASSERT, panic hooks
│   │   ├── attributes.h     # FW_INLINE, FW_MEMORY_BARRIER, FW_ISR, FW_DMA, FW_OWNER
│   │   ├── memory/          # pool.h, arena.h, buffer.h
│   │   ├── sync/            # spsc.h (Lock-free single-producer single-consumer)
│   │   ├── hal/             # gpio.h, uart.h, timer.h
│   │   ├── protocol/        # zerowire.h (Binary framing & stream resynchronization)
│   │   └── rtos/            # rtos.h (Unified RTOS abstraction)
│   └── src/                 # pool.c, arena.c, buffer.c, spsc.c, zerowire.c, assert.c
│
├── rust/                    # Rust Safety Tier (Cargo Workspace, strictly #![no_std])
│   ├── Cargo.toml           # Release profile: opt-level = "s", lto = true
│   └── crates/
│       ├── zero-core/       # FwSpan, FwStatus, and safe ZeroWire protocol engine
│       ├── zero-hal/        # Type-State GPIO (Pin<Input>, Pin<Output>) & DMA Ownership Tokens
│       ├── zero-sync/       # SpscQueue<T, N> lock-free ringbuffer & SpinLockFlag
│       └── zero-memory/     # StaticBuffer<N> & pool abstractions
│
├── tooling/                 # Development & Verification Tooling
│   ├── analyzer/            # zero_analyzer.py (Context & ISR safety analysis)
│   └── codegen/             # svd_codegen.py (CMSIS-SVD peripheral register generator)
│
├── hal/                     # Auto-generated hardware register maps (hw_gpioa.h)
├── rtos/                    # Bare-metal, FreeRTOS, and Zephyr adapters
├── benchmarks/              # Comprehensive performance benchmark suite
├── tests/                   # 114 Unit, Concurrency, and Safety Tests (100% Pass)
└── examples/stm32_poc/      # End-to-end verified firmware PoC
```

---

## 🛡️ Key Safety Invariants & Technical Highlights

1. **Bitmap Double-Free Elimination**: Memory pools incorporate an internal O(1) bitset tracker. Freeing an already-freed pointer returns `FW_ERR_INVALID_ARG` immediately, making list cycle corruption mathematically impossible.
2. **Hardware Memory Barriers**: Atomic acquire/release fences (`FW_MEMORY_BARRIER`) protect lockless SPSC queues, preventing out-of-order write buffer hazards on ARM Cortex-M7 and dual-core MCUs (ESP32, RP2040).
3. **Type-State Peripheral Drivers**: Hardware pins are generic types (`Pin<Input>`, `Pin<Output>`). State transitions consume the previous handle, eliminating illegal mode operations at compile time.
4. **DMA Ownership Tokens**: Buffer ownership moves into `DmaTransfer<BUF>` during asynchronous transfers, making stack-use-after-free physically impossible to write.
5. **Stream Resynchronization**: The ZeroWire protocol engine features sliding window SOF detection (`fw_zerowire_stream_sync`), recovering valid packets across noisy serial channels.
6. **Execution Context Verification**: The `zero_analyzer.py` tool scans annotations (`FW_ISR`, `FW_DMA`), catching illegal heap allocations (`malloc`) or blocking calls (`delay_ms`) inside interrupt handlers.

---

## 🚀 Quick Start

### 1. Build and Run Hardened Test Suite (114 Tests)
```bash
# Using MSVC Developer Command Prompt
cl /nologo /W4 /WX /O2 /I core-c/include core-c/src/*.c tests/test_core_memory.c /Fe:test_hardened.exe
.\test_hardened.exe
```

### 2. Run Static Context Analyzer
```bash
python tooling/analyzer/zero_analyzer.py core-c examples/stm32_poc
```

### 3. Run Benchmark Suite
```bash
cl /nologo /W4 /O2 /I core-c/include core-c/src/*.c benchmarks/bench_suite.c /Fe:bench_suite.exe
.\bench_suite.exe
```

### 4. Run STM32 PoC Firmware Demonstration
```bash
cl /nologo /W4 /O2 /I core-c/include core-c/src/*.c examples/stm32_poc/main.c /Fe:poc.exe
.\poc.exe
```

---

## 📄 Authors & License

Architected and developed by **Phong Võ** (`kzxl`). Released under the **MIT License**. Part of the **ZeroUniverse** industrial computing ecosystem.
