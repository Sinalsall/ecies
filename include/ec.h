#ifndef EC_H
#define EC_H

#include "bigint.h"

// The Curve Equation: y^2 = x^3 + 7
// (a = 0, b = 7)

typedef struct {
    bigint256_t x;
    bigint256_t y;
    int is_infinity; // 1 if this is the Point at Infinity (Zero), 0 otherwise
} ec_point_t;

// --- FUNCTIONS ---

// 1. Initialize to the Generator Point G (defined in standards)
void ec_init_g(ec_point_t *g);

// 2. Point Addition: res = P + Q
void ec_add(ec_point_t *res, const ec_point_t *p, const ec_point_t *q);

// 3. Point Doubling: res = P + P
void ec_double(ec_point_t *res, const ec_point_t *p);

// 4. Scalar Multiplication: res = k * P
void ec_mul(ec_point_t *res, const bigint256_t *k, const ec_point_t *p);

// Helper to print a point
void ec_print(const char *name, const ec_point_t *p);

#endif // EC_H