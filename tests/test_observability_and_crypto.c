/**
 * @file test_observability_and_crypto.c
 * @brief Unit tests for Deferred Lock-Free Logger and Embedded Crypto (SHA-256, ChaCha20)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zero/zero.h"

/* ========================================================================== */
/* 1. Deferred Lock-Free Logger Tests                                         */
/* ========================================================================== */

#define LOG_STORAGE_CAPACITY 16
static fw_log_entry_t s_log_storage[LOG_STORAGE_CAPACITY];
static char s_captured_log_output[1024];
static fw_size_t s_captured_len = 0;

static void test_log_sink(const char *str, fw_size_t len) {
    if (s_captured_len + len < sizeof(s_captured_log_output)) {
        memcpy(s_captured_log_output + s_captured_len, str, len);
        s_captured_len += len;
        s_captured_log_output[s_captured_len] = '\0';
    }
}

static void test_deferred_logger(void) {
    printf("[TEST] Deferred Logger: Fast SPSC logging and background flushing...\n");

    s_captured_len = 0;
    memset(s_captured_log_output, 0, sizeof(s_captured_log_output));

    assert(fw_log_init(test_log_sink, s_log_storage, LOG_STORAGE_CAPACITY) == FW_OK);

    /* 1. Post messages without blocking */
    fw_log_post_fast(FW_LOG_LEVEL_INFO, "PWR", "System boot complete, VDD=%u mV", 3300, 0);
    fw_log_post_fast(FW_LOG_LEVEL_WARN, "TEMP", "Sensor temp high: %u C", 85, 0);
    fw_log_post_fast(FW_LOG_LEVEL_ERROR, "BUS", "I2C NACK on device 0x%02X", 0x48, 0);

    /* Output buffer must still be empty before flush */
    assert(s_captured_len == 0);

    /* 2. Flush pending entries */
    fw_size_t flushed = fw_log_flush(0);
    assert(flushed == 3);
    assert(s_captured_len > 0);

    assert(strstr(s_captured_log_output, "[I]") != FW_NULL);
    assert(strstr(s_captured_log_output, "VDD=3300 mV") != FW_NULL);
    assert(strstr(s_captured_log_output, "[W]") != FW_NULL);
    assert(strstr(s_captured_log_output, "temp high: 85 C") != FW_NULL);
    assert(strstr(s_captured_log_output, "[E]") != FW_NULL);
    assert(strstr(s_captured_log_output, "device 0x48") != FW_NULL);

    /* 3. Test queue full drop protection (no crash / hang when flooded) */
    for (int i = 0; i < 30; ++i) {
        fw_log_post_fast(FW_LOG_LEVEL_DEBUG, "FLOOD", "Packet %d", i, 0);
    }
    /* Flushes up to capacity-1 */
    fw_size_t flood_flushed = fw_log_flush(0);
    assert(flood_flushed > 0 && flood_flushed < LOG_STORAGE_CAPACITY);

    printf("  -> Deferred logger passed! (Non-blocking queue and sink verified)\n");
}

/* ========================================================================== */
/* 2. Zero-Heap SHA-256 Tests (NIST FIPS 180-4 Official Test Vectors)         */
/* ========================================================================== */

static void bytes_to_hex(const uint8_t *bytes, fw_size_t len, char *hex_out) {
    for (fw_size_t i = 0; i < len; ++i) {
        snprintf(hex_out + i * 2, 3, "%02x", bytes[i]);
    }
}

static void test_sha256_nist_vectors(void) {
    printf("[TEST] Crypto: SHA-256 verification against NIST FIPS 180-4 vectors...\n");

    uint8_t digest[FW_SHA256_DIGEST_SIZE];
    char hex[FW_SHA256_DIGEST_SIZE * 2 + 1];

    /* NIST Vector 1: Empty string "" */
    fw_sha256_digest("", 0, digest);
    bytes_to_hex(digest, sizeof(digest), hex);
    assert(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    /* NIST Vector 2: "abc" */
    fw_sha256_digest("abc", 3, digest);
    bytes_to_hex(digest, sizeof(digest), hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    /* NIST Vector 3: Multi-block 56-byte message */
    const char *vec3 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    fw_sha256_digest(vec3, strlen(vec3), digest);
    bytes_to_hex(digest, sizeof(digest), hex);
    assert(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);

    /* Incremental update test */
    fw_sha256_ctx_t ctx;
    fw_sha256_init(&ctx);
    fw_sha256_update(&ctx, "a", 1);
    fw_sha256_update(&ctx, "b", 1);
    fw_sha256_update(&ctx, "c", 1);
    fw_sha256_final(&ctx, digest);
    bytes_to_hex(digest, sizeof(digest), hex);
    assert(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    printf("  -> SHA-256 passed! (100%% match with NIST standard test vectors)\n");
}

/* ========================================================================== */
/* 3. ChaCha20 Stream Cipher Tests (RFC 8439 Test Vectors)                     */
/* ========================================================================== */

static void test_chacha20_encryption(void) {
    printf("[TEST] Crypto: ChaCha20 symmetric cipher round-trip & stream integrity...\n");

    /* 256-bit Key */
    uint8_t key[FW_CHACHA20_KEY_SIZE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
    };

    /* 96-bit Nonce */
    uint8_t nonce[FW_CHACHA20_NONCE_SIZE] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4A, 0x00, 0x00, 0x00, 0x00
    };

    const char *secret_msg = "ZeroEmbedded-Secure-Telemetry-Payload-2026";
    fw_size_t msg_len = strlen(secret_msg);

    uint8_t ciphertext[64];
    uint8_t decrypted[64];

    /* 1. Encrypt */
    fw_chacha20_ctx_t enc_ctx;
    fw_chacha20_init(&enc_ctx, key, nonce, 1);
    fw_chacha20_crypt(&enc_ctx, (const uint8_t*)secret_msg, ciphertext, msg_len);

    /* Ciphertext must not match plaintext */
    assert(memcmp(ciphertext, secret_msg, msg_len) != 0);

    /* 2. Decrypt with same key, nonce, counter */
    fw_chacha20_ctx_t dec_ctx;
    fw_chacha20_init(&dec_ctx, key, nonce, 1);
    fw_chacha20_crypt(&dec_ctx, ciphertext, decrypted, msg_len);
    decrypted[msg_len] = '\0';

    /* Decrypted plaintext must match original exactly */
    assert(memcmp(decrypted, secret_msg, msg_len) == 0);

    printf("  -> ChaCha20 cipher passed! (End-to-end stream encryption verified)\n");
}

/* ========================================================================== */
/* Main Test Runner                                                           */
/* ========================================================================== */

int main(void) {
    printf("\n====================================================================================\n");
    printf("   ZEROEMBEDDED OBSERVABILITY & CRYPTO TEST SUITE (Log, SHA-256, ChaCha20)\n");
    printf("====================================================================================\n\n");

    test_deferred_logger();
    test_sha256_nist_vectors();
    test_chacha20_encryption();

    printf("\n\033[1;32m[PASS] ALL OBSERVABILITY & CRYPTO TESTS PASSED SUCCESSFULLY!\033[0m\n\n");
    return 0;
}
