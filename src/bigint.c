#include "bigint.h"
#include <stdio.h>
#include <string.h>

// --- CONSTANTS ---
static const uint64_t SECP256K1_K_LOW = 0x1000003D1ULL; 

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

// --- OPTIMIZED MODULAR OPERATIONS ---

static int bigint_ge(const bigint256_t *a, const bigint256_t *b) {
    for (int i = NUM_LIMBS - 1; i >= 0; i--) {
        if (a->limbs[i] > b->limbs[i]) return 1;
        if (a->limbs[i] < b->limbs[i]) return 0;
    }
    return 1;
}

void bigint_mod_p(bigint256_t *dest, const bigint512_t *src) {
    bigint512_t temp = *src;
    int safety_ctr = 0;

    // Fold the top 256 bits down (Lazy Reduction)
    while ((temp.limbs[4] || temp.limbs[5] || temp.limbs[6] || temp.limbs[7]) && safety_ctr < 20) {
        safety_ctr++;
        
        limb_t hi[4];
        hi[0] = temp.limbs[4]; hi[1] = temp.limbs[5]; hi[2] = temp.limbs[6]; hi[3] = temp.limbs[7];
        temp.limbs[4] = 0; temp.limbs[5] = 0; temp.limbs[6] = 0; temp.limbs[7] = 0;

        limb_t carry = 0;
        for (int i = 0; i < 4; i++) {
            dlimb_t product = (dlimb_t)hi[i] * SECP256K1_K_LOW;
            dlimb_t sum = (dlimb_t)temp.limbs[i] + product + carry;
            temp.limbs[i] = (limb_t)sum;
            carry = (limb_t)(sum >> 64);
        }
        temp.limbs[4] = carry;

        // Fold the overflow in limb[4] if it exists
        if (temp.limbs[4] != 0) {
            limb_t val = temp.limbs[4];
            temp.limbs[4] = 0;
            dlimb_t product = (dlimb_t)val * SECP256K1_K_LOW;
            
            limb_t lo = (limb_t)product;
            limb_t hi_part = (limb_t)(product >> 64);

            dlimb_t sum = (dlimb_t)temp.limbs[0] + lo;
            temp.limbs[0] = (limb_t)sum;
            carry = (limb_t)(sum >> 64);

            sum = (dlimb_t)temp.limbs[1] + hi_part + carry;
            temp.limbs[1] = (limb_t)sum;
            carry = (limb_t)(sum >> 64);

            int k = 2;
            while (carry > 0 && k < 4) {
                sum = (dlimb_t)temp.limbs[k] + carry;
                temp.limbs[k] = (limb_t)sum;
                carry = (limb_t)(sum >> 64);
                k++;
            }
        }
    }

    for (int i = 0; i < 4; i++) dest->limbs[i] = temp.limbs[i];
    int loop_limit = 0;
    while (bigint_ge(dest, &SECP256K1_P) && loop_limit < 5) {
        bigint_sub(dest, dest, &SECP256K1_P);
        loop_limit++;
    }
}

// --- OPTIMIZED INVERSION (Fully Inlined Binary GCD) ---
static int bigint_is_zero(const bigint256_t *a) {
    return (a->limbs[0] | a->limbs[1] | a->limbs[2] | a->limbs[3]) == 0;
}

void bigint_inv_mod_p(bigint256_t *dest, const bigint256_t *src) {
    if (bigint_is_zero(src)) { memset(dest, 0, sizeof(bigint256_t)); return; }

    bigint256_t u = *src;
    bigint256_t v = SECP256K1_P;
    bigint256_t x1, x2;
    memset(&x1, 0, sizeof(bigint256_t)); x1.limbs[0] = 1;
    memset(&x2, 0, sizeof(bigint256_t));

    // We process until u or v is 0. 
    // Inlining shifts and adds to remove function call overhead.
    while (!bigint_is_zero(&u) && !bigint_is_zero(&v)) {
        
        // 1. Shift u while even
        while ((u.limbs[0] & 1) == 0) {
            // u = u >> 1
            limb_t carry = 0;
            // Unrolled loop for 4 limbs
            limb_t n3 = (u.limbs[3] & 1) << 63; u.limbs[3] = (u.limbs[3] >> 1) | carry; carry = n3;
            limb_t n2 = (u.limbs[2] & 1) << 63; u.limbs[2] = (u.limbs[2] >> 1) | carry; carry = n2;
            limb_t n1 = (u.limbs[1] & 1) << 63; u.limbs[1] = (u.limbs[1] >> 1) | carry; carry = n1;
            /* n0 */                            u.limbs[0] = (u.limbs[0] >> 1) | carry;

            // If x1 even: x1 = x1 >> 1
            if ((x1.limbs[0] & 1) == 0) {
                carry = 0;
                n3 = (x1.limbs[3] & 1) << 63; x1.limbs[3] = (x1.limbs[3] >> 1) | carry; carry = n3;
                n2 = (x1.limbs[2] & 1) << 63; x1.limbs[2] = (x1.limbs[2] >> 1) | carry; carry = n2;
                n1 = (x1.limbs[1] & 1) << 63; x1.limbs[1] = (x1.limbs[1] >> 1) | carry; carry = n1;
                                              x1.limbs[0] = (x1.limbs[0] >> 1) | carry;
            } else {
                // x1 = (x1 + P) >> 1
                limb_t add_carry = 0;
                // Inline Add P
                dlimb_t sum = (dlimb_t)x1.limbs[0] + SECP256K1_P.limbs[0]; x1.limbs[0] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x1.limbs[1] + SECP256K1_P.limbs[1] + add_carry; x1.limbs[1] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x1.limbs[2] + SECP256K1_P.limbs[2] + add_carry; x1.limbs[2] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x1.limbs[3] + SECP256K1_P.limbs[3] + add_carry; x1.limbs[3] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                
                // Now Shift
                carry = add_carry << 63; // The overflow bit from addition becomes MSB
                n3 = (x1.limbs[3] & 1) << 63; x1.limbs[3] = (x1.limbs[3] >> 1) | carry; carry = n3;
                n2 = (x1.limbs[2] & 1) << 63; x1.limbs[2] = (x1.limbs[2] >> 1) | carry; carry = n2;
                n1 = (x1.limbs[1] & 1) << 63; x1.limbs[1] = (x1.limbs[1] >> 1) | carry; carry = n1;
                                              x1.limbs[0] = (x1.limbs[0] >> 1) | carry;
            }
        }

        // 2. Shift v while even
        while ((v.limbs[0] & 1) == 0) {
            // v = v >> 1
            limb_t carry = 0;
            limb_t n3 = (v.limbs[3] & 1) << 63; v.limbs[3] = (v.limbs[3] >> 1) | carry; carry = n3;
            limb_t n2 = (v.limbs[2] & 1) << 63; v.limbs[2] = (v.limbs[2] >> 1) | carry; carry = n2;
            limb_t n1 = (v.limbs[1] & 1) << 63; v.limbs[1] = (v.limbs[1] >> 1) | carry; carry = n1;
                                                v.limbs[0] = (v.limbs[0] >> 1) | carry;

            if ((x2.limbs[0] & 1) == 0) {
                carry = 0;
                n3 = (x2.limbs[3] & 1) << 63; x2.limbs[3] = (x2.limbs[3] >> 1) | carry; carry = n3;
                n2 = (x2.limbs[2] & 1) << 63; x2.limbs[2] = (x2.limbs[2] >> 1) | carry; carry = n2;
                n1 = (x2.limbs[1] & 1) << 63; x2.limbs[1] = (x2.limbs[1] >> 1) | carry; carry = n1;
                                              x2.limbs[0] = (x2.limbs[0] >> 1) | carry;
            } else {
                limb_t add_carry = 0;
                dlimb_t sum = (dlimb_t)x2.limbs[0] + SECP256K1_P.limbs[0]; x2.limbs[0] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x2.limbs[1] + SECP256K1_P.limbs[1] + add_carry; x2.limbs[1] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x2.limbs[2] + SECP256K1_P.limbs[2] + add_carry; x2.limbs[2] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                sum = (dlimb_t)x2.limbs[3] + SECP256K1_P.limbs[3] + add_carry; x2.limbs[3] = (limb_t)sum; add_carry = (limb_t)(sum >> 64);
                
                carry = add_carry << 63;
                n3 = (x2.limbs[3] & 1) << 63; x2.limbs[3] = (x2.limbs[3] >> 1) | carry; carry = n3;
                n2 = (x2.limbs[2] & 1) << 63; x2.limbs[2] = (x2.limbs[2] >> 1) | carry; carry = n2;
                n1 = (x2.limbs[1] & 1) << 63; x2.limbs[1] = (x2.limbs[1] >> 1) | carry; carry = n1;
                                              x2.limbs[0] = (x2.limbs[0] >> 1) | carry;
            }
        }

        // 3. Subtract
        if (bigint_ge(&u, &v)) {
            bigint_sub(&u, &u, &v);
            if (bigint_sub(&x1, &x1, &x2)) bigint_add(&x1, &x1, &SECP256K1_P);
        } else {
            bigint_sub(&v, &v, &u);
            if (bigint_sub(&x2, &x2, &x1)) bigint_add(&x2, &x2, &SECP256K1_P);
        }
    }

    if (bigint_is_zero(&v)) *dest = x1;
    else *dest = x2;
}