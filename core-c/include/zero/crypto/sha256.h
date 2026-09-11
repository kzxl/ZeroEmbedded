#ifndef ZERO_CRYPTO_SHA256_H
#define ZERO_CRYPTO_SHA256_H

/**
 * @file sha256.h
 * @brief Zero-Heap NIST FIPS 180-4 SHA-256 Hash Algorithm
 * Deterministic cryptographic hash function with zero dynamic memory allocation.
 * Used for secure boot image validation, OTA signature verification, and packet integrity.
 */

#include "zero/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_SHA256_DIGEST_SIZE 32
#define FW_SHA256_BLOCK_SIZE  64

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t  buffer[FW_SHA256_BLOCK_SIZE];
} fw_sha256_ctx_t;

/** Initializes SHA-256 context with initial state constants */
void fw_sha256_init(fw_sha256_ctx_t *ctx);

/** Incrementally processes input data chunks */
void fw_sha256_update(fw_sha256_ctx_t *ctx, const void *data, fw_size_t len);

/** Pads final block and produces 32-byte digest */
void fw_sha256_final(fw_sha256_ctx_t *ctx, uint8_t digest[FW_SHA256_DIGEST_SIZE]);

/** Computes SHA-256 digest in a single one-shot call */
void fw_sha256_digest(const void *data, fw_size_t len, uint8_t digest[FW_SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_CRYPTO_SHA256_H */
