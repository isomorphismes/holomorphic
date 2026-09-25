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

int main(void) {
    _Static_assert(HOLOMORPHIC_WALK_WORKER_COUNT == 3, "keep three search workers for the current CPU prototype");

    float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2] = {{0.0f, 0.0f}};
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float score = INFINITY;

    if (!holomorphic_walk_start(HOLOMORPHIC_WALK_DEFAULT_COEFFICIENT_BUDGET)) {
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

    float norm = holomorphic_walk_coefficient_budget(direction);
    if (!isfinite(norm) || fabsf(norm - 1.0f) > 1.0e-3f) {
        fprintf(stderr, "worker direction is not normalized: %.9g\n", norm);
        return 1;
    }

    if (
        holomorphic_walk_coefficient_budget(coefficients) >
        HOLOMORPHIC_WALK_DEFAULT_COEFFICIENT_BUDGET
    ) {
        fputs("zero coefficient state somehow exceeds the budget\n", stderr);
        return 1;
    }

    printf("holomorphic workers ready: count=%d score=%.9g norm=%.9g\n",
           HOLOMORPHIC_WALK_WORKER_COUNT, score, norm);
    return 0;
}
