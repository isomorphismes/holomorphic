#include <assert.h>
#include <math.h>
#include <stdio.h>

#define MAX_FACTORS 64
#include "../android/app/src/main/cpp/continuation_path.h"

static bool nearly_equal(float first, float second) {
    return fabsf(first - second) < 1.0e-5f;
}

int main(void) {
    float zeros[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    float poles[MAX_FACTORS][2] = {{0.0f, 0.0f}};
    struct continuation_path path = {0};

    assert(continuation_radius_at(0.0f, 0.0f, zeros, 0, poles, 0) < 0.0f);
    assert(continuation_path_seed(&path, 0.0f, 0.0f, zeros, 0, poles, 0));
    assert(path.count == 1);
    assert(path.radii[0] == CONTINUATION_UNBOUNDED_RADIUS);
    assert(continuation_path_append(&path, 1000.0f, -1000.0f, zeros, 0, poles, 0));

    continuation_path_clear(&path);
    poles[0][0] = 3.0f;
    poles[0][1] = 4.0f;
    assert(nearly_equal(continuation_radius_at(0.0f, 0.0f, zeros, 0, poles, 1), 5.0f));
    assert(continuation_path_seed(&path, 0.0f, 0.0f, zeros, 0, poles, 1));
    assert(!continuation_path_append(&path, 3.0f, 4.0f, zeros, 0, poles, 1));
    assert(!continuation_path_append(&path, 5.0f, 0.0f, zeros, 0, poles, 1));
    assert(!continuation_path_append(&path, 6.0f, 0.0f, zeros, 0, poles, 1));
    assert(continuation_path_append(&path, 2.0f, 0.0f, zeros, 0, poles, 1));
    assert(nearly_equal(path.radii[1], sqrtf(17.0f)));

    continuation_path_clear(&path);
    assert(!continuation_path_add_center(&path, 3.0f, 4.0f, zeros, 0, poles, 1));
    assert(path.count == 0);
    assert(continuation_path_add_center(&path, 3.000001f, 4.0f, zeros, 0, poles, 1));
    assert(path.count == 1);
    assert(path.radii[0] > 0.0f && path.radii[0] < 1.0e-5f);

    zeros[0][0] = 3.0f;
    zeros[0][1] = 4.0f;
    assert(continuation_radius_at(0.0f, 0.0f, zeros, 1, poles, 1) < 0.0f);

    zeros[0][0] = 3.000001f;
    assert(nearly_equal(continuation_radius_at(0.0f, 0.0f, zeros, 1, poles, 1), 5.0f));
    zeros[0][0] = 3.0f;

    poles[1][0] = 3.0f;
    poles[1][1] = 4.0f;
    assert(nearly_equal(continuation_radius_at(0.0f, 0.0f, zeros, 1, poles, 2), 5.0f));

    continuation_path_clear(&path);
    assert(continuation_path_seed(&path, 0.0f, 0.0f, zeros, 0, poles, 0));
    for (int index = 1; index < MAX_CONTINUATION_STEPS; ++index) {
        assert(continuation_path_append(
            &path,
            (float)index,
            0.0f,
            zeros,
            0,
            poles,
            0
        ));
    }
    assert(path.count == MAX_CONTINUATION_STEPS);
    assert(!continuation_path_append(&path, 100.0f, 0.0f, zeros, 0, poles, 0));

    puts("continuation path tests passed");
    return 0;
}
