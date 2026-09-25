#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_FACTORS 64
#include "../android/app/src/main/cpp/continuation_path.h"
#include "../android/app/src/main/cpp/factor_snap.h"
#include "../android/app/src/main/cpp/factor_state.h"

int main(void) {
    float factors[MAX_FACTORS][2] = {
        {10.0f, -3.0f},
        {1.0f, 2.0f}
    };

    float near_second[2] = {1.3f, 2.4f};
    assert(factor_snap_to_nearest(near_second, factors, 2, 0.25f, 4.0f) == 1);
    assert(near_second[0] == factors[1][0]);
    assert(near_second[1] == factors[1][1]);

    float outside[2] = {2.1f, 2.0f};
    assert(factor_snap_to_nearest(outside, factors, 2, 0.25f, 4.0f) < 0);
    assert(outside[0] == 2.1f && outside[1] == 2.0f);

    float no_scale[2] = {1.0f, 2.0f};
    assert(factor_snap_to_nearest(no_scale, factors, 2, 0.0f, 4.0f) < 0);

    float inserted_zeros[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    float inserted_poles[MAX_FACTORS][2] = {{3.0f, 4.0f}};
    int inserted_zero_count = 0;
    int inserted_pole_count = 1;
    float near_opposite[2] = {3.000001f, 4.0f};
    assert(factor_snap_to_nearest(
        near_opposite,
        inserted_poles,
        inserted_pole_count,
        1.0e-6f,
        2.0f
    ) == 0);
    assert(factor_insert_reduced(
        inserted_zeros,
        &inserted_zero_count,
        inserted_poles,
        &inserted_pole_count,
        near_opposite[0],
        near_opposite[1]
    ) == FACTOR_CANCELLED_OPPOSITE);
    assert(inserted_zero_count == 0 && inserted_pole_count == 0);

    float zeros[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    float poles[MAX_FACTORS][2] = {{3.0f, 4.0f}};
    struct continuation_path path = {0};
    continuation_path_clear(&path);
    float near_pole[2] = {3.000001f, 4.0f};
    assert(factor_snap_to_nearest(near_pole, poles, 1, 1.0e-6f, 2.0f) == 0);
    assert(near_pole[0] == 3.0f && near_pole[1] == 4.0f);
    assert(!continuation_path_add_center(
        &path,
        near_pole[0],
        near_pole[1],
        zeros,
        0,
        poles,
        1
    ));

    float programmatic_near_miss[2] = {3.000001f, 4.0f};
    assert(continuation_path_add_center(
        &path,
        programmatic_near_miss[0],
        programmatic_near_miss[1],
        zeros,
        0,
        poles,
        1
    ));
    assert(path.radii[0] > 0.0f);

    puts("factor snap tests passed");
    return 0;
}
