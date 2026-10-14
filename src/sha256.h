#ifndef CVAULT_SHA256_H
#define CVAULT_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} sha256_ctx;

void sha256_init(sha256_ctx *ctx);
void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *ctx, uint8_t hash[32]);

/* Converts 32-byte raw hash into a 64-char lowercase null-terminated hex string */
void sha256_hash_to_hex(const uint8_t hash[32], char hex[65]);

/* Convenience function: computes SHA-256 of a memory buffer directly */
void sha256_buffer(const uint8_t *data, size_t len, char hex[65]);

/* Convenience function: computes SHA-256 of an open file from current position */
int sha256_file(const char *filepath, char hex[65]);

#ifdef __cplusplus
}
#endif

#endif /* CVAULT_SHA256_H */

