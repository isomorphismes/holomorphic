#ifndef ANALYTIC_CONTINUATION_MODULAR_FORMS_H
#define ANALYTIC_CONTINUATION_MODULAR_FORMS_H

#include <complex.h>
#include <math.h>
#include <stdbool.h>

#define MODULAR_FORMS_MAX_Q_TERMS 16

struct modular_forms_value {
    double complex reduced_tau;
    double complex q;
    double complex e4;
    double complex e6;
    double complex delta;
    double complex j;
};

static double modular_divisor_power_sum(int n, int power) {
    double sum = 0.0;
    for (int divisor = 1; divisor <= n; ++divisor) {
        if (n % divisor != 0) {
            continue;
        }
        double term = 1.0;
        for (int k = 0; k < power; ++k) {
            term *= (double)divisor;
        }
        sum += term;
    }
    return sum;
}

static double complex modular_complex_power_24(double complex value) {
    double complex square = value * value;
    double complex fourth = square * square;
    double complex eighth = fourth * fourth;
    double complex sixteenth = eighth * eighth;
    return sixteenth * eighth;
}

static double complex modular_reduce_tau(double complex tau) {
    for (int step = 0; step < 16; ++step) {
        double shift = nearbyint(creal(tau));
        tau -= shift;
        if (cabs(tau) < 1.0) {
            tau = -1.0 / tau;
            continue;
        }
        break;
    }
    return tau;
}

static bool modular_forms_evaluate(
    double complex tau,
    int q_terms,
    struct modular_forms_value *out
) {
    if (out == NULL || !isfinite(creal(tau)) || !isfinite(cimag(tau)) || cimag(tau) <= 0.0) {
        return false;
    }
    if (q_terms < 1) {
        q_terms = 1;
    }
    if (q_terms > MODULAR_FORMS_MAX_Q_TERMS) {
        q_terms = MODULAR_FORMS_MAX_Q_TERMS;
    }

    const double two_pi = 6.283185307179586476925286766559;
    double complex reduced_tau = modular_reduce_tau(tau);
    double complex q = cexp(I * two_pi * reduced_tau);

    double complex e4 = 1.0;
    double complex e6 = 1.0;
    double complex q_power = 1.0;
    for (int n = 1; n <= q_terms; ++n) {
        q_power *= q;
        e4 += 240.0 * modular_divisor_power_sum(n, 3) * q_power;
        e6 -= 504.0 * modular_divisor_power_sum(n, 5) * q_power;
    }

    // Product form avoids subtracting nearly equal E4^3 and E6^2 near the cusp.
    double complex delta = q;
    q_power = q;
    for (int n = 1; n <= q_terms; ++n) {
        delta *= modular_complex_power_24(1.0 - q_power);
        q_power *= q;
    }

    out->reduced_tau = reduced_tau;
    out->q = q;
    out->e4 = e4;
    out->e6 = e6;
    out->delta = delta;
    out->j = e4 * e4 * e4 / delta;
    return true;
}

#endif
