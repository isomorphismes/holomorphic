#ifndef ANALYTIC_CONTINUATION_HOLOMORPHIC_WALK_H
#define ANALYTIC_CONTINUATION_HOLOMORPHIC_WALK_H

#include <stdbool.h>
#include <stdint.h>

#define HOLOMORPHIC_WALK_COEFFICIENT_COUNT 5
#define HOLOMORPHIC_WALK_WORKER_COUNT 3
#define HOLOMORPHIC_WALK_COEFFICIENT_BUDGET 6.00f

bool holomorphic_walk_start(void);
void holomorphic_walk_stop(void);

void holomorphic_walk_publish(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
);

bool holomorphic_walk_best_direction(
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float *score
);

float holomorphic_walk_coefficient_budget(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
);

/* Per-search CPU thread time is separate from GPU and EGL wall time. */
struct holomorphic_walk_cpu_performance {
    uint64_t searches;
    uint64_t measured;
    uint64_t invalid;
    double cpu_thread_ms;
};
void holomorphic_walk_take_cpu_performance(struct holomorphic_walk_cpu_performance *result);

#endif
