#include "ec.h"
#include <stdio.h>
#include <string.h>

static int bigint_eq(const bigint256_t *a, const bigint256_t *b) {
    for(int i=0; i<NUM_LIMBS; i++) if(a->limbs[i] != b->limbs[i]) return 0;
    return 1;
}

static void set_p(bigint256_t *p) {
    bigint_set_hex(p, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F");
}

void ec_init_g(ec_point_t *g) {
    bigint_set_hex(&g->x, "79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
    bigint_set_hex(&g->y, "483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8");
    g->is_infinity = 0;
}

void ec_print(const char *name, const ec_point_t *p) {
    printf("%s:\n", name);
    if (p->is_infinity) {
        printf("  (POINT_AT_INFINITY)\n");
    } else {
        printf("  x: "); bigint_print(&p->x);
        printf("  y: "); bigint_print(&p->y);
    }
}

void ec_double(ec_point_t *res, const ec_point_t *p) {
    if (p->is_infinity) { res->is_infinity = 1; return; }

    bigint512_t tmp_mul;
    bigint256_t x2, num, den, lambda;
    bigint256_t prime;
    set_p(&prime);

    bigint_mul(&tmp_mul, &p->x, &p->x);
    bigint_mod_p(&x2, &tmp_mul);

    bigint256_t three;
    bigint_set_hex(&three, "3");
    bigint_mul(&tmp_mul, &three, &x2);
    bigint_mod_p(&num, &tmp_mul);

    bigint256_t two;
    bigint_set_hex(&two, "2");
    bigint_mul(&tmp_mul, &two, &p->y);
    bigint_mod_p(&den, &tmp_mul);

    bigint256_t inv_den;
    bigint_inv_mod_p(&inv_den, &den);
    bigint_mul(&tmp_mul, &num, &inv_den);
    bigint_mod_p(&lambda, &tmp_mul);

    bigint256_t x3;
    bigint_mul(&tmp_mul, &lambda, &lambda); 
    bigint_mod_p(&x3, &tmp_mul);

    if (bigint_sub(&x3, &x3, &p->x)) bigint_add(&x3, &x3, &prime);
    if (bigint_sub(&x3, &x3, &p->x)) bigint_add(&x3, &x3, &prime);

    bigint256_t y3, dx;
    dx = p->x;
    if (bigint_sub(&dx, &dx, &x3)) bigint_add(&dx, &dx, &prime);

    bigint_mul(&tmp_mul, &lambda, &dx);
    bigint_mod_p(&y3, &tmp_mul);

    if (bigint_sub(&y3, &y3, &p->y)) bigint_add(&y3, &y3, &prime);

    res->x = x3;
    res->y = y3;
    res->is_infinity = 0;
}

void ec_add(ec_point_t *res, const ec_point_t *p, const ec_point_t *q) {
    if (p->is_infinity) { *res = *q; return; }
    if (q->is_infinity) { *res = *p; return; }

    if (bigint_eq(&p->x, &q->x)) {
        if (bigint_eq(&p->y, &q->y)) {
            ec_double(res, p);
            return;
        } else {
            res->is_infinity = 1;
            return;
        }
    }

    bigint256_t prime;
    set_p(&prime);

    bigint256_t num, den, lambda;
    num = q->y;
    if (bigint_sub(&num, &num, &p->y)) bigint_add(&num, &num, &prime);

    den = q->x;
    if (bigint_sub(&den, &den, &p->x)) bigint_add(&den, &den, &prime);

    bigint256_t inv_den;
    bigint_inv_mod_p(&inv_den, &den);

    bigint512_t tmp_mul;
    bigint_mul(&tmp_mul, &num, &inv_den);
    bigint_mod_p(&lambda, &tmp_mul);

    bigint256_t x3;
    bigint_mul(&tmp_mul, &lambda, &lambda);
    bigint_mod_p(&x3, &tmp_mul);

    if (bigint_sub(&x3, &x3, &p->x)) bigint_add(&x3, &x3, &prime);
    if (bigint_sub(&x3, &x3, &q->x)) bigint_add(&x3, &x3, &prime);

    bigint256_t y3, dx;
    dx = p->x;
    if (bigint_sub(&dx, &dx, &x3)) bigint_add(&dx, &dx, &prime);

    bigint_mul(&tmp_mul, &lambda, &dx);
    bigint_mod_p(&y3, &tmp_mul);

    if (bigint_sub(&y3, &y3, &p->y)) bigint_add(&y3, &y3, &prime);

    res->x = x3;
    res->y = y3;
    res->is_infinity = 0;
}

// Result = k * P (Double-and-Add Algorithm)
void ec_mul(ec_point_t *res, const bigint256_t *k, const ec_point_t *p) {
    ec_point_t temp;
    temp.is_infinity = 1; // Start at Infinity (Zero)
    
    // Scan bits from 255 down to 0
    for (int i = 255; i >= 0; i--) {
        // Double
        ec_double(&temp, &temp);

        // Add (if bit is 1)
        int limb_idx = i / 64;
        int bit_idx  = i % 64;
        if ((k->limbs[limb_idx] >> bit_idx) & 1) {
            ec_add(&temp, &temp, p);
        }
    }
    *res = temp;
}