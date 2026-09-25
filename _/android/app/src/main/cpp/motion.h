#ifndef ANALYTIC_CONTINUATION_MOTION_H
#define ANALYTIC_CONTINUATION_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "scene.h"

#define MOTION_MAX_EXCHANGES 8

struct motion_factor_ref {
    enum scene_factor_kind kind;
    int index;
};

enum motion_turn {
    MOTION_TURN_CLOCKWISE = -1,
    MOTION_TURN_COUNTERCLOCKWISE = 1
};

struct motion_exchange {
    struct motion_factor_ref first;
    struct motion_factor_ref second;
    float start_seconds;
    float duration_seconds;
    enum motion_turn turn;

    bool started;
    bool completed;
    float first_start[2];
    float second_start[2];
};

struct motion_program {
    bool enabled;
    uint32_t seed;
    float wander_speed;
    float wander_bound;
    float elapsed_seconds;
    int exchange_count;
    struct motion_exchange exchanges[MOTION_MAX_EXCHANGES];
};

void motion_program_initialize(struct motion_program *program);

bool motion_program_add_exchange(
    struct motion_program *program,
    struct motion_factor_ref first,
    struct motion_factor_ref second,
    float start_seconds,
    float duration_seconds,
    enum motion_turn turn
);

bool motion_program_validate(
    const struct motion_program *program,
    const struct scene *scene
);

bool motion_program_advance(
    struct motion_program *program,
    struct scene *scene,
    float dt
);

int motion_program_completed_exchange_count(const struct motion_program *program);
bool motion_program_complete(const struct motion_program *program);

#endif
