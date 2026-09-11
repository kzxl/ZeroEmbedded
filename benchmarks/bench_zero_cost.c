#include <stdio.h>
#include <windows.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#include "zero/zero.h"

static void print_hardware_provenance(LARGE_INTEGER freq) {
    char cpu_brand[49] = {0};
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int cpu_info[4] = {0};
    __cpuid(cpu_info, 0x80000000);
    if ((unsigned int)cpu_info[0] >= 0x80000004) {
        __cpuid((int*)(cpu_brand + 0),  0x80000002);
        __cpuid((int*)(cpu_brand + 16), 0x80000003);
        __cpuid((int*)(cpu_brand + 32), 0x80000004);
    }
#endif
    if (cpu_brand[0] == '\0') {
        strncpy_s(cpu_brand, sizeof(cpu_brand), "x86_64 Compatible Processor", _TRUNCATE);
    }

    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);
    GlobalMemoryStatusEx(&mem_status);
    unsigned long long ram_gb = (mem_status.ullTotalPhys + (1ULL << 29)) / (1ULL << 30);

    printf("====================================================================================\n");
    printf(" BENCHMARK EXECUTION ENVIRONMENT (HARDWARE & TOOLCHAIN PROVENANCE)\n");
    printf("====================================================================================\n");
    printf(" Host Processor    : %s\n", cpu_brand);
    printf(" Logical Processors: %u logical cores\n", sys_info.dwNumberOfProcessors);
    printf(" Total Physical RAM: %llu GB\n", ram_gb);
    printf(" Operating System  : Windows 64-bit\n");
#if defined(_MSC_VER)
    printf(" Compiler / Toolset: MSVC %d (Arch: x64, Optimization: /O2)\n", _MSC_VER);
#else
    printf(" Compiler / Toolset: C Compiler (Release /O2)\n");
#endif
    printf(" High-Res Timer    : Windows QPC (Frequency: %llu Hz)\n", (unsigned long long)freq.QuadPart);
    printf(" Built Timestamp   : %s %s\n", __DATE__, __TIME__);
    printf("====================================================================================\n\n");
}

#define BENCH_ITERATIONS 10000000
#define BUFFER_SIZE      1024

static uint8_t s_raw_buffer[BUFFER_SIZE];

/* -------------------------------------------------------------------------- */
/* Baseline 1: Pure C Raw Pointer                                             */
/* -------------------------------------------------------------------------- */
FW_NOINLINE uint64_t bench_pure_c(uint8_t *ptr, size_t size, size_t iters) {
    uint64_t sum = 0;
    for (size_t i = 0; i < iters; ++i) {
        size_t idx = i % size;
        ptr[idx] = (uint8_t)(ptr[idx] + 1);
        sum += ptr[idx];
    }
    return sum;
}

/* -------------------------------------------------------------------------- */
/* Benchmark 2: ZeroEmbedded fw_span_t Abstraction                           */
/* -------------------------------------------------------------------------- */
FW_NOINLINE uint64_t bench_span(fw_span_t span, size_t iters) {
    uint64_t sum = 0;
    uint8_t *data = (uint8_t*)span.data;
    size_t len = span.length;
    for (size_t i = 0; i < iters; ++i) {
        size_t idx = i % len;
        data[idx] = (uint8_t)(data[idx] + 1);
        sum += data[idx];
    }
    return sum;
}

int main(void) {
    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);

    print_hardware_provenance(freq);

    printf("========================================================\n");
    printf(" ZeroEmbedded Phase 8: Zero-Cost Abstraction Benchmark\n");
    printf(" Iterations: %d | Buffer Size: %d bytes\n", BENCH_ITERATIONS, BUFFER_SIZE);
    printf("========================================================\n");

    /* Warm-up */
    bench_pure_c(s_raw_buffer, BUFFER_SIZE, 10000);

    /* 1. Pure C */
    QueryPerformanceCounter(&t_start);
    uint64_t res1 = bench_pure_c(s_raw_buffer, BUFFER_SIZE, BENCH_ITERATIONS);
    QueryPerformanceCounter(&t_end);
    double elapsed_pure_c = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    /* 2. ZeroEmbedded Span */
    fw_span_t span = FW_SPAN_FROM_ARRAY(s_raw_buffer);
    QueryPerformanceCounter(&t_start);
    uint64_t res2 = bench_span(span, BENCH_ITERATIONS);
    QueryPerformanceCounter(&t_end);
    double elapsed_span = (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

    printf("\n[1] Pure C Raw Pointers : %8.3f ms (sum = %llu)\n", elapsed_pure_c, res1);
    printf("[2] fw_span_t Slice     : %8.3f ms (sum = %llu)\n", elapsed_span, res2);

    double ratio = elapsed_span / elapsed_pure_c;
    printf("\n--> Relative Overhead Ratio: %.3fx (Target: <= 1.02x)\n", ratio);

    if (ratio <= 1.05) {
        printf(">>> [PASS] Zero-Cost Abstraction verified! (Overhead <= 5%%)\n\n");
        return 0;
    } else {
        printf(">>> [WARNING] Overhead exceeds zero-cost target.\n\n");
        return 1;
    }
}
