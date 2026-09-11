#ifndef ZERO_CRYPTO_CHACHA20_H
#define ZERO_CRYPTO_CHACHA20_H

/**
 * @file chacha20.h
 * @brief RFC 8439 ChaCha20 Lightweight Symmetric Stream Cipher
 * Zero-heap, timing-attack immune encryption/decryption for embedded communications
 * (RF LoRa, BLE, RS-485, and authenticated telemetry packets).
 */

#include "zero/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_CHACHA20_KEY_SIZE   32
#define FW_CHACHA20_NONCE_SIZE 12

typedef struct {
    uint32_t state[16];
} fw_chacha20_ctx_t;

/**
 * @brief Initializes ChaCha20 cipher state with a 256-bit key, 96-bit nonce, and block counter.
 *
 * @param ctx Pointer to ChaCha20 context.
 * @param key 32-byte secret key.
 * @param nonce 12-byte initialization vector / nonce.
 * @param counter Initial block counter (typically 0 or 1).
 */
void fw_chacha20_init(fw_chacha20_ctx_t *ctx,
                      const uint8_t key[FW_CHACHA20_KEY_SIZE],
                      const uint8_t nonce[FW_CHACHA20_NONCE_SIZE],
                      uint32_t counter);

/**
 * @brief Encrypts or decrypts a buffer in-place or out-of-place (symmetric stream XOR).
 *
 * @param ctx Pointer to initialized ChaCha20 context.
 * @param in Input plaintext (or ciphertext).
 * @param out Output ciphertext (or plaintext). Can be identical to `in`.
 * @param length Byte length of data to process.
 */
void fw_chacha20_crypt(fw_chacha20_ctx_t *ctx,
                       const uint8_t *in,
                       uint8_t *out,
                       fw_size_t length);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_CRYPTO_CHACHA20_H */
