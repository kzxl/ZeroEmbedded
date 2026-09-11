# ZeroEmbedded Static Analyzer Rules Specification (Phase 4 & 5)

This document defines the semantic analysis rules enforced by the **ZeroEmbedded Static Analyzer** for mission-critical embedded software.

---

## 1. Ownership & Lifetime Annotations (Phase 4)

### 1.1 `FW_OWNER` (Owning Resource)
- **Definition**: A pointer or handle decorated with `FW_OWNER` represents the unique owner of an allocated memory block.
- **Rule 4.1 (Single Ownership)**: An owning pointer cannot be assigned to another owning pointer without explicit transfer semantics.
- **Rule 4.2 (Must-Release)**: An owning pointer falling out of scope without being freed (e.g., via `fw_pool_free`) triggers a `LEAK_DETECTED` compile-time error.
- **Rule 4.3 (No Use-After-Free)**: Referencing an owning pointer after its releasing function triggers a `USE_AFTER_FREE` compile-time error.

### 1.2 `FW_BORROW` (Borrowed Reference)
- **Definition**: A pointer decorated with `FW_BORROW` is a non-owning temporary reference.
- **Rule 4.4 (No Release)**: Calling a freeing function on an `FW_BORROW` pointer is strictly illegal (`ILLEGAL_DEALLOCATION`).
- **Rule 4.5 (Lifetime Subordination)**: A borrowed reference cannot outlive its parent owning resource.

---

## 2. Execution Context Rules (Phase 5)

Embedded firmware operates in distinct execution contexts with strict architectural boundaries.

| Context Annotation | Execution Level | Preemption Capability | Permitted Operations |
| :--- | :--- | :--- | :--- |
| **`FW_NORMAL`** | Main loop / RTOS Task | Preemptible by ISR | All operations, blocking waits, task yields. |
| **`FW_ISR`** | Interrupt Service Routine | High priority, atomic | O(1) lock-free queues (`fw_spsc_push`), flag sets. **NO dynamic allocation, NO blocking delays**. |
| **`FW_DMA`** | Peripheral DMA engine | Autonomous hardware bus | DMA-safe buffers with physical alignment and persistent lifetimes. |
| **`FW_INIT`** | Hardware startup / reset | Single-threaded init | Static buffer layout, pool/arena configuration, peripheral clock enable. |

### 2.1 Context Violations

#### Violation 5.1: `ISR_CALLS_BLOCKING`
```c
FW_ISR void TIM2_IRQHandler(void) {
    delay_ms(10); // ERROR: Cannot call blocking delays from an ISR context!
}
```

#### Violation 5.2: `ISR_CALLS_DYNAMIC_ALLOC`
```c
FW_ISR void UART_IRQHandler(void) {
    void *p = malloc(64); // ERROR: malloc() cannot be called from an ISR context!
}
```

#### Violation 5.3: `DMA_LIFETIME_EXCEEDED`
```c
void send_packet(void) {
    uint8_t temp_buf[32]; // Stack-allocated
    dma_transmit(temp_buf, 32); 
    // ERROR: Stack buffer lifetime ends before asynchronous DMA completion!
}
```

---

## 3. Analyzer Diagnostic Codes

- `ZE-001`: Allocation in ISR context
- `ZE-002`: Blocking function invoked from ISR
- `ZE-003`: Memory leak on owning pointer
- `ZE-004`: Use-after-free detected
- `ZE-005`: Stack buffer passed to asynchronous DMA
- `ZE-006`: Null pointer passed to `FW_NONNULL` parameter
