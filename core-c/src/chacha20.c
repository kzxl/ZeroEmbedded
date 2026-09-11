/**
 * @file chacha20.c
 * @brief RFC 8439 ChaCha20 stream cipher implementation
 */

#include "zero/crypto/chacha20.h"
#include <string.h>

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define QR(a, b, c, d) \
    do { \
        a += b; d ^= a; d = ROTL32(d, 16); \
        c += d; b ^= c; b = ROTL32(b, 12); \
        a += b; d ^= a; d = ROTL32(d, 8);  \
        c += d; b ^= c; b = ROTL32(b, 7);  \
    } while (0)

static uint32_t load_le32(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           (((uint32_t)p[1]) << 8) |
           (((uint32_t)p[2]) << 16) |
           (((uint32_t)p[3]) << 24);
}

static void store_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

void fw_chacha20_init(fw_chacha20_ctx_t *ctx,
                      const uint8_t key[FW_CHACHA20_KEY_SIZE],
                      const uint8_t nonce[FW_CHACHA20_NONCE_SIZE],
                      uint32_t counter) {
    if (ctx == FW_NULL || key == FW_NULL || nonce == FW_NULL) return;

    /* Constant "expand 32-byte k" */
    ctx->state[0] = 0x61707865;
    ctx->state[1] = 0x3320646e;
    ctx->state[2] = 0x79622d32;
    ctx->state[3] = 0x6b206574;

    /* 256-bit Key */
    for (int i = 0; i < 8; ++i) {
        ctx->state[4 + i] = load_le32(key + i * 4);
    }

    /* Block Counter */
    ctx->state[12] = counter;

    /* 96-bit Nonce */
    ctx->state[13] = load_le32(nonce + 0);
    ctx->state[14] = load_le32(nonce + 4);
    ctx->state[15] = load_le32(nonce + 8);
}

static void chacha20_block(fw_chacha20_ctx_t *ctx, uint8_t output[64]) {
    uint32_t x[16];
    memcpy(x, ctx->state, sizeof(x));

    for (int i = 0; i < 10; ++i) {
        /* Column Round */
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);

        /* Diagonal Round */
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }

    for (int i = 0; i < 16; ++i) {
        store_le32(output + i * 4, x[i] + ctx->state[i]);
    }

    ctx->state[12]++;
}

void fw_chacha20_crypt(fw_chacha20_ctx_t *ctx,
                       const uint8_t *in,
                       uint8_t *out,
                       fw_size_t length) {
    if (ctx == FW_NULL || in == FW_NULL || out == FW_NULL || length == 0) return;

    uint8_t keystream[64];

    while (length > 0) {
        chacha20_block(ctx, keystream);

        fw_size_t chunk = (length > 64) ? 64 : length;
        for (fw_size_t i = 0; i < chunk; ++i) {
            out[i] = in[i] ^ keystream[i];
        }

        in     += chunk;
        out    += chunk;
        length -= chunk;
    }
}
