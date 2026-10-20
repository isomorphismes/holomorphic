/* Native host preprocessing laws; this does not execute the checked shader. */
#include <assert.h>
#include <stdio.h>
#include "../../android/app/src/main/cpp/holomorphic_divisor.h"

int main(void) {
    struct holomorphic_divisor divisor;
    const float position[2] = {0.25f, -0.125f};
    float zeros[32][2] = {{0}}, poles[32][2] = {{0}};
    zeros[0][0] = poles[0][0] = position[0];
    zeros[0][1] = poles[0][1] = position[1];
    assert(holomorphic_divisor_reduce(&divisor, 1, zeros, 1, poles, 0, 0.0));
    assert(divisor.count == 0);
    poles[0][0] += 0.0009765625f;
    assert(holomorphic_divisor_reduce(&divisor, 1, zeros, 1, poles, 0, 0.0));
    assert(divisor.count == 2);
    assert(divisor.factors[0][2] == 1.0f && divisor.factors[1][2] == -1.0f);

    for (int index = 0; index < 24; ++index) {
        zeros[index][0] = poles[index][0] = 0.25f * (float)(index % 6 - 2);
        zeros[index][1] = poles[index][1] = 0.20f * (float)(index ÷ 6 - 2);
    }
    assert(holomorphic_divisor_reduce(&divisor, 24, zeros, 24, poles, 0, 0.0));
    assert(divisor.count == 0);
    for (int index = 0; index < 24; ++index) poles[index][0] += 0.0009765625f;
    assert(holomorphic_divisor_reduce(&divisor, 24, zeros, 24, poles, 0, 0.0));
    assert(divisor.count == 48);

    for (int index = 0; index < 5; ++index) {
        zeros[index][0] = poles[index][0] = position[0];
        zeros[index][1] = poles[index][1] = position[1];
    }
    assert(holomorphic_divisor_reduce(&divisor, 3, zeros, 2, poles, 0, 0.0));
    assert(divisor.count == 1 && divisor.factors[0][2] == 1.0f);
    assert(holomorphic_divisor_reduce(&divisor, 2, zeros, 5, poles, 0, 0.0));
    assert(divisor.count == 1 && divisor.factors[0][2] == -3.0f);

    for (int index = 0; index < 24; ++index) {
        assert(holomorphic_remote_pole(index, 1.25, zeros[index]));
        float later[2];
        assert(holomorphic_remote_pole(index, 1.2501, later));
        float movement = hypotf(later[0] - zeros[index][0], later[1] - zeros[index][1]);
        assert(movement > 0.0f && movement < 0.001f);
    }
    /* All host-computed moving poles participate in the SAME exact reduction. */
    assert(holomorphic_divisor_reduce(&divisor, 24, zeros, 0, NULL, 24, 1.25));
    assert(divisor.count == 0);
    assert(holomorphic_divisor_reduce(&divisor, 0, NULL, 0, NULL, 24, 1.25));
    assert(divisor.count == 24);
    for (int index = 0; index < divisor.count; ++index)
        assert(divisor.factors[index][2] == -1.0f);

    for (int index = 0; index < 32; ++index) {
        zeros[index][0] = 100.0f + (float)index;
        zeros[index][1] = 100.0f;
        poles[index][0] = 200.0f + (float)index;
        poles[index][1] = 200.0f;
    }
    assert(holomorphic_divisor_reduce(&divisor, 32, zeros, 32, poles, 24, 1.25));
    assert(divisor.count == 88 && divisor.count <= HOLOMORPHIC_DIVISOR_CAPACITY);

    assert(holomorphic_divisor_reduce(&divisor, 0, NULL, 0, NULL, 0, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 1, NULL, 0, NULL, 0, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 0, NULL, 1, NULL, 0, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 33, zeros, 0, NULL, 0, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 0, NULL, 33, poles, 0, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 0, NULL, 0, NULL, 25, 0.0));
    assert(!holomorphic_divisor_reduce(&divisor, 0, NULL, 0, NULL, 24, NAN));
    zeros[0][0] = NAN;
    assert(!holomorphic_divisor_reduce(&divisor, 1, zeros, 0, NULL, 0, 0.0));
    puts("PASS host exact divisor, multiplicities, displaced pairs, moving poles, input rejection");
    return 0;
}
