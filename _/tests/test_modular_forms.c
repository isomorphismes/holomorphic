#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>

#include "../android/app/src/main/cpp/modular_forms.h"

static void assert_close(double complex actual, double complex expected, double tolerance) {
    if (cabs(actual - expected) > tolerance) {
        fprintf(stderr, "expected %.12g%+.12gi, got %.12g%+.12gi\n",
                creal(expected), cimag(expected), creal(actual), cimag(actual));
        assert(0);
    }
}

int main(void) {
    const double sqrt_three = 1.7320508075688772935;
    struct modular_forms_value value;

    assert(!modular_forms_evaluate(0.0, 12, &value));
    assert(!modular_forms_evaluate(-I, 12, &value));

    assert(modular_forms_evaluate(I, 12, &value));
    assert_close(value.j, 1728.0, 1e-8);
    assert(cabs(value.e6) < 1e-12);

    assert(modular_forms_evaluate(-0.5 + I * sqrt_three / 2.0, 12, &value));
    assert(cabs(value.j) < 1e-20);
    assert(cabs(value.e4) < 1e-12);

    assert(modular_forms_evaluate(2.0 * I, 12, &value));
    assert_close(value.j, 287496.0, 1e-6);

    // j is invariant under tau -> tau + 1 and tau -> -1/tau.
    struct modular_forms_value shifted;
    struct modular_forms_value inverted;
    double complex tau = 0.2 + 1.4 * I;
    assert(modular_forms_evaluate(tau, 12, &value));
    assert(modular_forms_evaluate(tau + 1.0, 12, &shifted));
    assert(modular_forms_evaluate(-1.0 / tau, 12, &inverted));
    assert_close(shifted.j, value.j, 1e-8);
    assert_close(inverted.j, value.j, 1e-8);

    puts("modular form evaluator ok");
    return 0;
}
