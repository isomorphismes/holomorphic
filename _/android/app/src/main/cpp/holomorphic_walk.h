#ifndef ANALYTIC_CONTINUATION_HOLOMORPHIC_WALK_H
#define ANALYTIC_CONTINUATION_HOLOMORPHIC_WALK_H

#include <stdbool.h>

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

#endif
