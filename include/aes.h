#ifndef AES_H
#define AES_H

#include <stdint.h>
#include <stddef.h>

// AES-128 Context
typedef struct {
    uint32_t round_keys[44]; // 11 round keys * 4 words
} aes_ctx_t;

void aes_init(aes_ctx_t *ctx, const uint8_t *key);
void aes_encrypt_block(aes_ctx_t *ctx, const uint8_t *in, uint8_t *out);

// CTR Mode: Encrypts buffer in place
void aes_ctr_encrypt(aes_ctx_t *ctx, uint8_t *nonce, uint8_t *buf, size_t len);

#endif