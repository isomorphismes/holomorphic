#include "field_evolution.h"

#include <math.h>
#include <string.h>

static void publish_if_due(struct field_evolution *field, double now) {
    if (!field->workers_started) return;
    if (field->last_publish == 0.0 || now - field->last_publish >= 0.200) {
        holomorphic_walk_publish(field->coefficients);
        field->last_publish = now;
    }
}

void field_evolution_initialize(
    struct field_evolution *field,
    enum field_background_mode background_mode,
    float speed,
    float coefficient_budget,
    double now
) {
    memset(field, 0, sizeof(*field));
    field->background_mode = background_mode;
    field->speed = speed;
    field->coefficient_budget = coefficient_budget;
    field->last_time = now;
}

bool field_evolution_start(struct field_evolution *field, double now) {
    field->workers_started = holomorphic_walk_start(field->coefficient_budget);
    field->last_time = now;
    if (!field->workers_started) return false;

    holomorphic_walk_publish(field->coefficients);
    field->last_publish = now;
    return true;
}

void field_evolution_stop(struct field_evolution *field) {
    if (!field->workers_started) return;
    holomorphic_walk_stop();
    field->workers_started = false;
}

void field_evolution_reset_clock(struct field_evolution *field, double now) {
    field->last_time = now;
}

void field_evolution_publish_now(struct field_evolution *field, double now) {
    if (!field->workers_started) return;
    holomorphic_walk_publish(field->coefficients);
    field->last_publish = now;
}

bool field_evolution_advance(
    struct field_evolution *field,
    double now,
    bool allow_motion
) {
    float wall_dt = (float)(now - field->last_time);
    field->last_time = now;
    if (wall_dt <= 0.0f) return false;

    publish_if_due(field, now);
    if (!allow_motion) return false;

    /*
     * Remote poles are analytic trajectories parameterized by real runtime time,
     * so a slow rendered frame skips samples rather than slowing the orbit. The
     * coefficient walk is an integrated numerical process and keeps its bounded
     * per-update step to prevent a delayed frame from causing a large q jump.
     */
    bool changed = false;
    if (field->background_mode == FIELD_BACKGROUND_WANDERING_OFFSCREEN_POLES) {
        field->remote_pole_time += wall_dt;
        changed = true;
    }

    float dt = wall_dt > 0.05f ? 0.05f : wall_dt;
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float score = field->last_score;
    if (
        field->workers_started &&
        holomorphic_walk_best_direction(direction, &score)
    ) {
        float blend = 1.0f - expf(-4.0f * dt);
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            field->velocity[index][0] =
                (1.0f - blend) * field->velocity[index][0] +
                blend * field->speed * direction[index][0];
            field->velocity[index][1] =
                (1.0f - blend) * field->velocity[index][1] +
                blend * field->speed * direction[index][1];
        }
        field->direction_ready = true;
        field->last_score = score;
    }

    if (!field->direction_ready) return changed;

    float candidate[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        candidate[index][0] = field->coefficients[index][0] + dt * field->velocity[index][0];
        candidate[index][1] = field->coefficients[index][1] + dt * field->velocity[index][1];
    }

    if (holomorphic_walk_coefficient_budget(candidate) <= field->coefficient_budget) {
        memcpy(field->coefficients, candidate, sizeof(field->coefficients));
        field->accepted_steps += 1;
        return true;
    }

    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        field->velocity[index][0] *= -0.30f;
        field->velocity[index][1] *= -0.30f;
    }
    field_evolution_publish_now(field, now);
    return changed;
}
