/* Independent numerical witness for the CPU polynomial-direction evaluator.
 * Production holomorphic_walk.c is injected by the compile command's -include;
 * its static direction_at() is therefore the actual function being exercised.
 * This is not a renderer/GPU/device-parity test, nor a choice of sample policy.
 */
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define CASES_SEED UINT32_C(0x51a7c3d9)

static double complex independent_power(double complex base, int degree) {
    if (degree == 0) return 1.0 + 0.0 * I;
    if (creal(base) == 0.0 && cimag(base) == 0.0) return 0.0 + 0.0 * I;
    return cpow(base, (double)degree);
}

static double complex independent_polynomial(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    double complex u, int derivative_order
) {
    double complex result = 0.0 + 0.0 * I;
    for (int k = 1; k <= HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++k) {
        double complex coefficient = coefficients[k - 1][0] + I * (double)coefficients[k - 1][1];
        if (derivative_order == 0) {
            result += coefficient * independent_power(u, k);
        } else {
            result += (double)k * coefficient * independent_power(u, k - 1);
        }
    }
    return result;
}

static int check_complex(const char *case_name, const char *quantity,
                         double complex expected, const float observed[2]) {
    const double absolute_tolerance = 2.0e-5;
    const double relative_tolerance = 1.0e-4;
    const double tolerance = absolute_tolerance + relative_tolerance * cabs(expected);
    const double error = cabs((double)observed[0] + I * (double)observed[1] - expected);
    if (!isfinite(observed[0]) || !isfinite(observed[1]) ||
        !isfinite(error) || error > tolerance) {
        fprintf(stderr,
            "SEMANTIC-MISMATCH case=%s quantity=%s expected=(%.17g,%.17g) "
            "actual=(%.9g,%.9g) error=%.9g tolerance=%.9g\n",
            case_name, quantity, creal(expected), cimag(expected),
            observed[0], observed[1], error, tolerance);
        return 1;
    }
    return 0;
}

static int check_case(const char *name,
                      const float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
                      double complex u) {
    float value[2];
    float derivative_u[2];
    direction_at(direction, (float)creal(u), (float)cimag(u), value, derivative_u);
    int bad = 0;
    bad += check_complex(name, "delta_q(u)", independent_polynomial(direction, u, 0), value);
    bad += check_complex(name, "d(delta_q)/du", independent_polynomial(direction, u, 1), derivative_u);
    return bad;
}

static uint32_t next_random(uint32_t *state) {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static float signed_random(uint32_t *state) {
    return ((float)(next_random(state) & UINT32_C(0xffff)) / 65535.0f) * 1.6f - 0.8f;
}

int main(void) {
    _Static_assert(HOLOMORPHIC_WALK_COEFFICIENT_COUNT == 5,
                   "the tested five-term approximation changed");
    int failures = 0;
    float directions[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2] = {{0}};

    /* Anchors independent of the reference loop: q(z)= (z/3)^5, z=1.
     * d q / d z = 5/243; d q / d u = 5/81 at u=1/3.
     * A CPU scoring derivative is currently d/du, not d/dz.
     */
    directions[4][0] = 1.0f;
    if (fabs(creal(independent_polynomial(directions, 1.0 / 3.0, 0)) - 1.0 / 243.0) > 1.0e-14 ||
        fabs(creal(independent_polynomial(directions, 1.0 / 3.0, 1)) / 3.0 - 5.0 / 243.0) > 1.0e-14) {
        fputs("INDEPENDENT-ORACLE-BROKEN fifth-degree anchor\n", stderr);
        return 2;
    }
    failures += check_case("fifth-degree-u=1/3", directions, 1.0 / 3.0);
    failures += check_case("fifth-degree-complex", directions, 0.33 + 0.47 * I);

    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        directions[index][0] = (float)(0.17 * (index + 1));
        directions[index][1] = (float)(0.11 * (index % 2 ? -index - 1 : index + 1));
    }
    failures += check_case("complex-u=0", directions, 0.0);
    failures += check_case("complex-u=negative-real", directions, -0.71 + 0.42 * I);
    failures += check_case("complex-u=boundary", directions, 0.79 + 0.43 * I);
    failures += check_case("complex-u=pure-imaginary", directions, -0.55 * I);

    uint32_t random_state = CASES_SEED;
    for (int attempt = 0; attempt < 128; ++attempt) {
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            directions[index][0] = signed_random(&random_state);
            directions[index][1] = signed_random(&random_state);
        }
        char label[64];
        snprintf(label, sizeof(label), "seed-%08x-case-%03d", CASES_SEED, attempt);
        failures += check_case(label, directions,
                               (double)signed_random(&random_state) + I * (double)signed_random(&random_state));
    }
    if (failures) {
        fprintf(stderr, "SEMANTIC-FAILURES count=%d seed=%08x\n", failures, CASES_SEED);
        return 1;
    }
    printf("SEMANTIC-PASS cases=134 seed=%08x oracle=complex-binary64-cpow "
           "candidate=holomorphic_walk.c::direction_at derivative=d/du\n", CASES_SEED);
    return 0;
}
