#ifndef BIGINT_H
#define BIGINT_H

#include <stdint.h>
#include <stddef.h>

#define NUM_LIMBS 4 
#define LIMB_BITS 64

typedef uint64_t limb_t;
typedef unsigned __int128 dlimb_t;

typedef struct {
    limb_t limbs[NUM_LIMBS];
} bigint256_t;

typedef struct {
    limb_t limbs[NUM_LIMBS * 2];
} bigint512_t;

void bigint_set_hex(bigint256_t *dest, const char *hex_str);
void bigint_print(const bigint256_t *src);
void bigint_print_512(const bigint512_t *src);

limb_t bigint_add(bigint256_t *res, const bigint256_t *a, const bigint256_t *b);
limb_t bigint_sub(bigint256_t *res, const bigint256_t *a, const bigint256_t *b);
void bigint_mul(bigint512_t *res, const bigint256_t *a, const bigint256_t *b);

void bigint_mod_p(bigint256_t *dest, const bigint512_t *src);
void bigint_inv_mod_p(bigint256_t *dest, const bigint256_t *src);

#endif