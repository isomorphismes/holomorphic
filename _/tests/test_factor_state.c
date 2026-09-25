#include <assert.h>
#include <stdio.h>

#define MAX_FACTORS 64
#include "../android/app/src/main/cpp/factor_state.h"

int main(void) {
    float zeros[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    float poles[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    int zero_count = 0;
    int pole_count = 0;

    assert(factor_insert_reduced(poles, &pole_count, zeros, &zero_count, 2.0f, 3.0f) ==
        FACTOR_APPENDED);
    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 2.0f, 3.0f) ==
        FACTOR_CANCELLED_OPPOSITE);
    assert(zero_count == 0 && pole_count == 0);

    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 1.0f, 0.0f) ==
        FACTOR_APPENDED);
    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 1.0f, 0.0f) ==
        FACTOR_APPENDED);
    assert(factor_insert_reduced(poles, &pole_count, zeros, &zero_count, 1.0f, 0.0f) ==
        FACTOR_CANCELLED_OPPOSITE);
    assert(zero_count == 1 && pole_count == 0);

    assert(factor_insert_reduced(poles, &pole_count, zeros, &zero_count, 1.000001f, 0.0f) ==
        FACTOR_APPENDED);
    assert(zero_count == 1 && pole_count == 1);

    assert(factor_insert_reduced(poles, &pole_count, zeros, &zero_count, 7.0f, -2.0f) ==
        FACTOR_APPENDED);
    assert(factor_insert_reduced(poles, &pole_count, zeros, &zero_count, 7.0f, -2.0f) ==
        FACTOR_APPENDED);
    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 7.0f, -2.0f) ==
        FACTOR_CANCELLED_OPPOSITE);
    assert(zero_count == 1 && pole_count == 2);

    zero_count = MAX_FACTORS;
    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 7.0f, -2.0f) ==
        FACTOR_CANCELLED_OPPOSITE);
    assert(zero_count == MAX_FACTORS && pole_count == 1);

    assert(factor_insert_reduced(zeros, &zero_count, poles, &pole_count, 9.0f, 9.0f) ==
        FACTOR_UNCHANGED);

    puts("factor state tests passed");
    return 0;
}
