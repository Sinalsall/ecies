#include "bigint.h"
#include <stdio.h>
#include <string.h>

// --- CONSTANTS ---
static const uint64_t SECP256K1_K_LOW = 0x1000003D1ULL; 

// P = 2^256 - 0x1000003D1
// In limbs (Little Endian order for calculation):
// Limbs[0] = 0xFFFFFFFEFFFFFC2F
// Limbs[1] = 0xFFFFFFFFFFFFFFFF
// Limbs[2] = 0xFFFFFFFFFFFFFFFF
// Limbs[3] = 0xFFFFFFFFFFFFFFFF
static const bigint256_t SECP256K1_P = {{
    0xFFFFFFFEFFFFFC2FULL, 
    0xFFFFFFFFFFFFFFFFULL, 
    0xFFFFFFFFFFFFFFFFULL, 
    0xFFFFFFFFFFFFFFFFULL
}};

// --- HELPERS ---

void bigint_set_hex(bigint256_t *dest, const char *hex_str) {
    for (int i = 0; i < NUM_LIMBS; i++) dest->limbs[i] = 0;
    int len = strlen(hex_str);
    int limb_idx = 0;
    int bit_shift = 0;
    for (int i = len - 1; i >= 0; i--) {
        char c = hex_str[i];
        uint64_t val = 0;
        if (c >= '0' && c <= '9') val = c - '0';
        else if (c >= 'a' && c <= 'f') val = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val = c - 'A' + 10;
        else continue;
        dest->limbs[limb_idx] |= (val << bit_shift);
        bit_shift += 4;
        if (bit_shift >= 64) {
            bit_shift = 0;
            limb_idx++;
            if (limb_idx >= NUM_LIMBS) break;
        }
    }
}

void bigint_print(const bigint256_t *src) {
    printf("0x");
    for (int i = NUM_LIMBS - 1; i >= 0; i--) printf("%016lx", src->limbs[i]);
    printf("\n");
}

void bigint_print_512(const bigint512_t *src) {
    printf("0x");
    for (int i = (NUM_LIMBS * 2) - 1; i >= 0; i--) printf("%016lx", src->limbs[i]);
    printf("\n");
}

// --- ARITHMETIC ---

limb_t bigint_add(bigint256_t *res, const bigint256_t *a, const bigint256_t *b) {
    limb_t carry = 0;
    for (int i = 0; i < NUM_LIMBS; i++) {
        dlimb_t sum = (dlimb_t)a->limbs[i] + b->limbs[i] + carry;
        res->limbs[i] = (limb_t)sum;
        carry = (limb_t)(sum >> 64);
    }
    return carry;
}

limb_t bigint_sub(bigint256_t *res, const bigint256_t *a, const bigint256_t *b) {
    limb_t borrow = 0;
    for (int i = 0; i < NUM_LIMBS; i++) {
        dlimb_t diff = (dlimb_t)a->limbs[i] - b->limbs[i] - borrow;
        res->limbs[i] = (limb_t)diff;
        borrow = (limb_t)((diff >> 64) & 1);
    }
    return borrow;
}

void bigint_mul(bigint512_t *res, const bigint256_t *a, const bigint256_t *b) {
    for (int i = 0; i < NUM_LIMBS * 2; i++) res->limbs[i] = 0;
    for (int i = 0; i < NUM_LIMBS; i++) {
        if (a->limbs[i] == 0) continue; 
        limb_t carry = 0;
        for (int j = 0; j < NUM_LIMBS; j++) {
            int k = i + j;
            dlimb_t product = (dlimb_t)a->limbs[i] * b->limbs[j];
            dlimb_t sum = (dlimb_t)res->limbs[k] + product + carry;
            res->limbs[k] = (limb_t)sum;
            carry = (limb_t)(sum >> 64);
        }
        res->limbs[i + NUM_LIMBS] += carry;
    }
}

// --- MODULAR OPERATIONS ---

// Internal helper to check if a >= b
static int bigint_ge(const bigint256_t *a, const bigint256_t *b) {
    for (int i = NUM_LIMBS - 1; i >= 0; i--) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return 0;
    }
    return 1; // Equal
}

void bigint_mod_p(bigint256_t *dest, const bigint512_t *src) {
    bigint512_t temp = *src;
    int safety_ctr = 0;

    // 1. Fold the top 256 bits down (Lazy Reduction)
    while ((temp.limbs[4] || temp.limbs[5] || temp.limbs[6] || temp.limbs[7]) && safety_ctr < 20) {
        safety_ctr++;
        for (int i = 4; i < 8; i++) {
            limb_t val = temp.limbs[i];
            if (val == 0) continue;
            temp.limbs[i] = 0;
            dlimb_t product = (dlimb_t)val * SECP256K1_K_LOW;
            limb_t lo = (limb_t)product;
            limb_t hi = (limb_t)(product >> 64);
            int pos = i - 4;
            dlimb_t sum = (dlimb_t)temp.limbs[pos] + lo;
            temp.limbs[pos] = (limb_t)sum;
            limb_t carry = (limb_t)(sum >> 64);
            if (pos + 1 < 8) {
                sum = (dlimb_t)temp.limbs[pos + 1] + hi + carry;
                temp.limbs[pos + 1] = (limb_t)sum;
                carry = (limb_t)(sum >> 64);
            }
            int k = pos + 2;
            while (carry > 0 && k < 8) {
                sum = (dlimb_t)temp.limbs[k] + carry;
                temp.limbs[k] = (limb_t)sum;
                carry = (limb_t)(sum >> 64);
                k++;
            }
        }
    }

    // 2. Extract the lower 256 bits
    for (int i = 0; i < 4; i++) dest->limbs[i] = temp.limbs[i];

    // 3. Strict Check: If result >= P, subtract P
    while (bigint_ge(dest, &SECP256K1_P)) {
        bigint_sub(dest, dest, &SECP256K1_P);
    }
}

void bigint_inv_mod_p(bigint256_t *dest, const bigint256_t *src) {
    bigint256_t p_minus_2;
    bigint_set_hex(&p_minus_2, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2D");

    bigint256_t res;
    bigint_set_hex(&res, "1");
    
    bigint256_t base = *src;
    bigint512_t temp_mul;

    for (int i = 255; i >= 0; i--) {
        bigint_mul(&temp_mul, &res, &res);
        bigint_mod_p(&res, &temp_mul);

        int limb_idx = i / 64;
        int bit_idx  = i % 64;
        if ((p_minus_2.limbs[limb_idx] >> bit_idx) & 1) {
            bigint_mul(&temp_mul, &res, &base);
            bigint_mod_p(&res, &temp_mul);
        }
    }
    *dest = res;
}