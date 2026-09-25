#ifndef ANALYTIC_CONTINUATION_SCENE_H
#define ANALYTIC_CONTINUATION_SCENE_H

#include <stdbool.h>

#define SCENE_MAX_FACTORS 32

enum scene_factor_kind {
    SCENE_FACTOR_NONE = -1,
    SCENE_FACTOR_ZERO = 0,
    SCENE_FACTOR_POLE = 1
};

struct scene {
    float zero_positions[SCENE_MAX_FACTORS][2];
    int zero_count;
    float pole_positions[SCENE_MAX_FACTORS][2];
    int pole_count;
    float zoom;
};

void scene_initialize_default(struct scene *scene);
void scene_clear_factors(struct scene *scene);

int scene_factor_count(const struct scene *scene, enum scene_factor_kind kind);
float (*scene_factor_positions(struct scene *scene, enum scene_factor_kind kind))[2];
const float (*scene_factor_positions_const(
    const struct scene *scene,
    enum scene_factor_kind kind
))[2];

bool scene_factor_valid(
    const struct scene *scene,
    enum scene_factor_kind kind,
    int index
);

bool scene_add_factor(
    struct scene *scene,
    enum scene_factor_kind kind,
    float x,
    float y,
    int *new_index
);

bool scene_move_factor(
    struct scene *scene,
    enum scene_factor_kind kind,
    int index,
    float x,
    float y
);

#endif
