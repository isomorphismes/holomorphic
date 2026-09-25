#include "motion.h"

#include <math.h>
#include <string.h>

static const float MOTION_PI = 3.14159265358979323846f;

static float smooth_progress(float value) {
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    return value * value * (3.0f - 2.0f * value);
}

static uint32_t mix_bits(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float hash_unit(uint32_t seed, uint32_t lane) {
    uint32_t value = mix_bits(seed ^ (0x9e3779b9u * (lane + 1u)));
    return (float)(value & 0x00ffffffu) / 16777215.0f;
}

static bool same_ref(struct motion_factor_ref left, struct motion_factor_ref right) {
    return left.kind == right.kind && left.index == right.index;
}

static float *factor_point(struct scene *scene, struct motion_factor_ref ref) {
    if (!scene_factor_valid(scene, ref.kind, ref.index)) return NULL;
    float (*positions)[2] = scene_factor_positions(scene, ref.kind);
    return positions[ref.index];
}

static void mark_active(
    bool active_zero[SCENE_MAX_FACTORS],
    bool active_pole[SCENE_MAX_FACTORS],
    struct motion_factor_ref ref
) {
    if (ref.kind == SCENE_FACTOR_ZERO && ref.index >= 0 && ref.index < SCENE_MAX_FACTORS) {
        active_zero[ref.index] = true;
    }
    if (ref.kind == SCENE_FACTOR_POLE && ref.index >= 0 && ref.index < SCENE_MAX_FACTORS) {
        active_pole[ref.index] = true;
    }
}

static void rotate_from_start(
    float output[2],
    const float center[2],
    const float start[2],
    float angle
) {
    float x = start[0] - center[0];
    float y = start[1] - center[1];
    float cosine = cosf(angle);
    float sine = sinf(angle);
    output[0] = center[0] + cosine * x - sine * y;
    output[1] = center[1] + sine * x + cosine * y;
}

static void advance_exchange(
    struct motion_exchange *exchange,
    struct scene *scene,
    float elapsed,
    bool active_zero[SCENE_MAX_FACTORS],
    bool active_pole[SCENE_MAX_FACTORS],
    bool *changed
) {
    if (exchange->completed || elapsed < exchange->start_seconds) {
        return;
    }

    float *first = factor_point(scene, exchange->first);
    float *second = factor_point(scene, exchange->second);
    if (first == NULL || second == NULL) {
        return;
    }

    if (!exchange->started) {
        exchange->first_start[0] = first[0];
        exchange->first_start[1] = first[1];
        exchange->second_start[0] = second[0];
        exchange->second_start[1] = second[1];
        exchange->started = true;
    }

    mark_active(active_zero, active_pole, exchange->first);
    mark_active(active_zero, active_pole, exchange->second);

    float raw = (elapsed - exchange->start_seconds) / exchange->duration_seconds;
    if (raw >= 1.0f) {
        first[0] = exchange->second_start[0];
        first[1] = exchange->second_start[1];
        second[0] = exchange->first_start[0];
        second[1] = exchange->first_start[1];
        exchange->completed = true;
        *changed = true;
        return;
    }

    float center[2] = {
        0.5f * (exchange->first_start[0] + exchange->second_start[0]),
        0.5f * (exchange->first_start[1] + exchange->second_start[1])
    };
    float angle = (float)exchange->turn * MOTION_PI * smooth_progress(raw);

    rotate_from_start(first, center, exchange->first_start, angle);
    rotate_from_start(second, center, exchange->second_start, angle);
    *changed = true;
}

static float restoring_velocity(float position, float bound, float speed) {
    if (bound <= 0.0f) return 0.0f;
    float soft = 0.78f * bound;
    float magnitude = fabsf(position);
    if (magnitude <= soft) return 0.0f;

    float excess = (magnitude - soft) / fmaxf(bound - soft, 1.0e-4f);
    if (excess > 1.5f) excess = 1.5f;
    return -copysignf(speed * 1.8f * excess, position);
}

static void wander_factor(
    struct motion_program *program,
    float point[2],
    enum scene_factor_kind kind,
    int index,
    float dt
) {
    uint32_t base = (uint32_t)(index + 1) * 17u;
    if (kind == SCENE_FACTOR_POLE) base += 0x51u;

    float phase_x1 = 2.0f * MOTION_PI * hash_unit(program->seed, base + 0u);
    float phase_x2 = 2.0f * MOTION_PI * hash_unit(program->seed, base + 1u);
    float phase_y1 = 2.0f * MOTION_PI * hash_unit(program->seed, base + 2u);
    float phase_y2 = 2.0f * MOTION_PI * hash_unit(program->seed, base + 3u);

    float frequency_x1 = 0.31f + 0.29f * hash_unit(program->seed, base + 4u);
    float frequency_x2 = 0.11f + 0.17f * hash_unit(program->seed, base + 5u);
    float frequency_y1 = 0.27f + 0.33f * hash_unit(program->seed, base + 6u);
    float frequency_y2 = 0.09f + 0.19f * hash_unit(program->seed, base + 7u);

    float t = program->elapsed_seconds;
    float vx = program->wander_speed * (
        0.68f * sinf(frequency_x1 * t + phase_x1) +
        0.32f * sinf(frequency_x2 * t + phase_x2)
    );
    float vy = program->wander_speed * (
        0.68f * sinf(frequency_y1 * t + phase_y1) +
        0.32f * sinf(frequency_y2 * t + phase_y2)
    );

    vx += restoring_velocity(point[0], program->wander_bound, program->wander_speed);
    vy += restoring_velocity(point[1], program->wander_bound, program->wander_speed);

    point[0] += dt * vx;
    point[1] += dt * vy;
}

void motion_program_initialize(struct motion_program *program) {
    memset(program, 0, sizeof(*program));
    program->seed = 1u;
    program->wander_bound = 1.8f;
}

bool motion_program_add_exchange(
    struct motion_program *program,
    struct motion_factor_ref first,
    struct motion_factor_ref second,
    float start_seconds,
    float duration_seconds,
    enum motion_turn turn
) {
    if (
        program->exchange_count >= MOTION_MAX_EXCHANGES ||
        same_ref(first, second) ||
        start_seconds < 0.0f ||
        duration_seconds <= 0.0f ||
        (turn != MOTION_TURN_CLOCKWISE && turn != MOTION_TURN_COUNTERCLOCKWISE)
    ) {
        return false;
    }

    struct motion_exchange *exchange = &program->exchanges[program->exchange_count++];
    memset(exchange, 0, sizeof(*exchange));
    exchange->first = first;
    exchange->second = second;
    exchange->start_seconds = start_seconds;
    exchange->duration_seconds = duration_seconds;
    exchange->turn = turn;
    program->enabled = true;
    return true;
}

bool motion_program_validate(
    const struct motion_program *program,
    const struct scene *scene
) {
    if (program->wander_speed < 0.0f || program->wander_bound < 0.0f) {
        return false;
    }

    for (int index = 0; index < program->exchange_count; ++index) {
        const struct motion_exchange *exchange = &program->exchanges[index];
        if (
            !scene_factor_valid(scene, exchange->first.kind, exchange->first.index) ||
            !scene_factor_valid(scene, exchange->second.kind, exchange->second.index) ||
            same_ref(exchange->first, exchange->second) ||
            exchange->start_seconds < 0.0f || exchange->duration_seconds <= 0.0f
        ) {
            return false;
        }
    }
    return true;
}

bool motion_program_advance(
    struct motion_program *program,
    struct scene *scene,
    float dt
) {
    if (!program->enabled || dt <= 0.0f) {
        return false;
    }

    /*
     * The event timeline follows wall-clock time. A slow frame must not make a
     * six-second exchange take twenty seconds. Only the integrated random wander
     * is bounded per update; exchange positions are evaluated directly on their
     * continuous half-circle trajectory at the current timeline time.
     */
    program->elapsed_seconds += dt;
    float integration_dt = dt > 0.05f ? 0.05f : dt;

    bool active_zero[SCENE_MAX_FACTORS] = {false};
    bool active_pole[SCENE_MAX_FACTORS] = {false};
    bool changed = false;

    for (int index = 0; index < program->exchange_count; ++index) {
        advance_exchange(
            &program->exchanges[index],
            scene,
            program->elapsed_seconds,
            active_zero,
            active_pole,
            &changed
        );
    }

    if (program->wander_speed <= 0.0f) {
        return changed;
    }

    for (int index = 0; index < scene->zero_count; ++index) {
        if (!active_zero[index]) {
            wander_factor(
                program,
                scene->zero_positions[index],
                SCENE_FACTOR_ZERO,
                index,
                integration_dt
            );
            changed = true;
        }
    }
    for (int index = 0; index < scene->pole_count; ++index) {
        if (!active_pole[index]) {
            wander_factor(
                program,
                scene->pole_positions[index],
                SCENE_FACTOR_POLE,
                index,
                integration_dt
            );
            changed = true;
        }
    }

    return changed;
}

int motion_program_completed_exchange_count(const struct motion_program *program) {
    int completed = 0;
    for (int index = 0; index < program->exchange_count; ++index) {
        if (program->exchanges[index].completed) ++completed;
    }
    return completed;
}

bool motion_program_complete(const struct motion_program *program) {
    if (program->wander_speed > 0.0f) return false;
    return motion_program_completed_exchange_count(program) == program->exchange_count;
}
