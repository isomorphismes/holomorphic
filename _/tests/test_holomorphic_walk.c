#define _POSIX_C_SOURCE 200809L

#include "holomorphic_walk.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static void sleep_milliseconds(long milliseconds) {
    struct timespec delay = {
        .tv_sec = milliseconds / 1000,
        .tv_nsec = (milliseconds % 1000) * 1000000L,
    };
    nanosleep(&delay, NULL);
}

static float derivative_norm(
    const float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    float norm = 0.0f;
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        norm += (float)(index + 2) * hypotf(direction[index][0], direction[index][1]);
    }
    return norm;
}

int main(void) {
    _Static_assert(HOLOMORPHIC_WALK_WORKER_COUNT == 3, "the deformation must keep three workers");

    float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2] = {{0.0f, 0.0f}};
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float score = INFINITY;

    if (!holomorphic_walk_start()) {
        fputs("holomorphic_walk_start failed\n", stderr);
        return 1;
    }

    holomorphic_walk_publish(coefficients);

    bool found = false;
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (holomorphic_walk_best_direction(direction, &score)) {
            found = true;
            break;
        }
        sleep_milliseconds(5);
    }

    holomorphic_walk_stop();

    if (!found) {
        fputs("workers never produced a direction\n", stderr);
        return 1;
    }
    if (!isfinite(score)) {
        fputs("worker score is not finite\n", stderr);
        return 1;
    }

    float norm = derivative_norm(direction);
    if (!isfinite(norm) || fabsf(norm - 1.0f) > 1.0e-3f) {
        fprintf(stderr, "worker direction is not normalized: %.9g\n", norm);
        return 1;
    }

    printf("holomorphic workers ready: count=%d score=%.9g norm=%.9g\n",
           HOLOMORPHIC_WALK_WORKER_COUNT, score, norm);
    return 0;
}
