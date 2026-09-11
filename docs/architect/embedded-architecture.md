# 🏛️ ZeroEmbedded: Silicon & Firmware Architecture Specification

**ZeroEmbedded** is an enterprise-grade sovereign embedded framework and toolchain engineered for deterministic real-time microcontrollers. It bridges the gap between **universal C hardware compatibility** and **Rust compile-time memory/concurrency safety** with zero runtime overhead.

---

## 1. Core Architectural Principles

- **Rule 1 — C is the Compatibility Layer**: Vendor SDKs (ST HAL, NXP SDK, ESP-IDF) and existing legacy firmware compile directly without code rewrites, custom compiler forks, or proprietary wrappers.
- **Rule 2 — Rust is the Safety Island**: Strategic deployment of Rust for memory-sensitive algorithms, communication protocols, concurrency state machines, and type-state peripheral drivers.
- **Rule 3 — Zero-Cost First, Runtime Safety Fallback**: Compile-time static analysis and type invariants eliminate overhead. Abstractions compile down to raw register assembly identical to hand-written C (`-O2` / `-O3`).
- **Rule 4 — C ABI (`extern "C"`) is the Strict Boundary**: No leaky abstractions across language boundaries; high-frequency execution loops never cross the FFI bridge.

---

## 2. Complete Subsystems & Directory Layout

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
├── benchmarks/              # Comprehensive performance benchmark suite (with HEPM logging)
├── tests/                   # 114 Unit, Concurrency, and Safety Tests (100% Pass)
└── examples/stm32_poc/      # End-to-end verified firmware PoC
```

---

## 3. Key Safety Invariants & Technical Highlights

### 3.1 Bitmap Double-Free Elimination
Memory pools incorporate an internal $O(1)$ bitset tracker alongside a linked free-list. Freeing an already-freed pointer returns `FW_ERR_INVALID_ARG` immediately, rendering list cycle corruption mathematically impossible.

### 3.2 Hardware Memory Barriers
Atomic acquire/release fences (`FW_MEMORY_BARRIER`) protect lockless SPSC queues, preventing out-of-order write buffer hazards on out-of-order ARM Cortex-M7 cores and dual-core MCUs (ESP32, RP2040).

### 3.3 Type-State Peripheral Drivers
Hardware pins are generic types (`Pin<Input>`, `Pin<Output>`). State transitions consume the previous handle by value, eliminating illegal mode operations at compile time:
```rust
let pin_input = pin.into_input();
// pin_input.set_high(); // Compile Error: method not found in `Pin<Input>`
let pin_output = pin_input.into_output();
pin_output.set_high();   // Allowed
```

### 3.4 DMA Ownership Tokens
Buffer ownership moves into `DmaTransfer<BUF>` during asynchronous transfers, making stack-use-after-free and DMA buffer corruption physically impossible to write:
```rust
let (dma_tx, pending) = dma.start_transfer(buf);
// buf is moved; cannot be modified while transfer is in-flight
let buf = dma_tx.wait_complete(pending);
```

### 3.5 Sliding-Window Stream Resynchronization
The ZeroWire protocol engine features sliding window SOF detection (`fw_zerowire_stream_sync`), recovering valid packets across noisy serial channels without dropped message storms.

### 3.6 Execution Context Static Analysis
The `zero_analyzer.py` tool scans AST annotations (`FW_ISR`, `FW_DMA`), catching illegal heap allocations (`malloc`), long loops, or blocking calls (`delay_ms`) inside interrupt service routines at CI time.
