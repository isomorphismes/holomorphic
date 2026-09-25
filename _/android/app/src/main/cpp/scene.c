#include "scene.h"

#include <string.h>

void scene_initialize_default(struct scene *scene) {
    memset(scene, 0, sizeof(*scene));

    scene->zero_count = 1;
    scene->zero_positions[0][0] = -0.34f;
    scene->zero_positions[0][1] = 0.0f;

    scene->pole_count = 1;
    scene->pole_positions[0][0] = 0.34f;
    scene->pole_positions[0][1] = 0.0f;

    scene->zoom = 1.0f;
}

void scene_clear_factors(struct scene *scene) {
    scene->zero_count = 0;
    scene->pole_count = 0;
    memset(scene->zero_positions, 0, sizeof(scene->zero_positions));
    memset(scene->pole_positions, 0, sizeof(scene->pole_positions));
}

int scene_factor_count(const struct scene *scene, enum scene_factor_kind kind) {
    if (kind == SCENE_FACTOR_ZERO) return scene->zero_count;
    if (kind == SCENE_FACTOR_POLE) return scene->pole_count;
    return 0;
}

float (*scene_factor_positions(struct scene *scene, enum scene_factor_kind kind))[2] {
    if (kind == SCENE_FACTOR_ZERO) return scene->zero_positions;
    if (kind == SCENE_FACTOR_POLE) return scene->pole_positions;
    return NULL;
}

const float (*scene_factor_positions_const(
    const struct scene *scene,
    enum scene_factor_kind kind
))[2] {
    if (kind == SCENE_FACTOR_ZERO) return scene->zero_positions;
    if (kind == SCENE_FACTOR_POLE) return scene->pole_positions;
    return NULL;
}

bool scene_factor_valid(
    const struct scene *scene,
    enum scene_factor_kind kind,
    int index
) {
    return index >= 0 && index < scene_factor_count(scene, kind);
}

bool scene_add_factor(
    struct scene *scene,
    enum scene_factor_kind kind,
    float x,
    float y,
    int *new_index
) {
    int *count = NULL;
    float (*positions)[2] = scene_factor_positions(scene, kind);

    if (kind == SCENE_FACTOR_ZERO) count = &scene->zero_count;
    if (kind == SCENE_FACTOR_POLE) count = &scene->pole_count;
    if (count == NULL || positions == NULL || *count >= SCENE_MAX_FACTORS) {
        return false;
    }

    int index = *count;
    positions[index][0] = x;
    positions[index][1] = y;
    *count += 1;
    if (new_index != NULL) *new_index = index;
    return true;
}

bool scene_move_factor(
    struct scene *scene,
    enum scene_factor_kind kind,
    int index,
    float x,
    float y
) {
    if (!scene_factor_valid(scene, kind, index)) {
        return false;
    }

    float (*positions)[2] = scene_factor_positions(scene, kind);
    positions[index][0] = x;
    positions[index][1] = y;
    return true;
}
