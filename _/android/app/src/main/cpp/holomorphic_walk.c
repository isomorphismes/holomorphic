#include "holomorphic_walk.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define SEARCH_CANDIDATES 128
#define DISTURBANCE_SAMPLE_COUNT 19

struct walk_worker {
    pthread_t thread;
    int index;
    uint64_t result_generation;
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float score;
};

struct walk_state {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    bool running;
    bool stop;
    uint64_t generation;
    float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    struct walk_worker workers[HOLOMORPHIC_WALK_WORKER_COUNT];
};

static struct walk_state walk = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .changed = PTHREAD_COND_INITIALIZER
};

static const float disturbance_samples[DISTURBANCE_SAMPLE_COUNT][2] = {
    { 0.00f,  0.00f},
    { 0.18f,  0.07f},
    {-0.13f,  0.26f},
    { 0.31f, -0.19f},
    {-0.37f, -0.11f},
    { 0.08f,  0.48f},
    { 0.49f,  0.17f},
    {-0.28f,  0.49f},
    {-0.52f, -0.29f},
    { 0.33f, -0.55f},
    { 0.66f,  0.08f},
    {-0.61f,  0.24f},
    { 0.14f,  0.71f},
    {-0.18f, -0.73f},
    { 0.73f, -0.31f},
    {-0.70f, -0.36f},
    { 0.46f,  0.72f},
    {-0.48f,  0.69f},
    { 0.79f,  0.43f}
};

static uint32_t random_u32(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static float random_signed(uint32_t *state) {
    return 2.0f * ((float)(random_u32(state) & 0x00ffffffu) / 16777215.0f) - 1.0f;
}

static void complex_multiply(
    float left_x,
    float left_y,
    float right_x,
    float right_y,
    float output[2]
) {
    output[0] = left_x * right_x - left_y * right_y;
    output[1] = left_x * right_y + left_y * right_x;
}

static float normalize_direction(
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    float derivative_norm = 0.0f;
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        float degree = (float)(index + 2);
        derivative_norm += degree * hypotf(direction[index][0], direction[index][1]);
    }
    if (derivative_norm < 1.0e-7f) {
        return 0.0f;
    }
    float inverse = 1.0f / derivative_norm;
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        direction[index][0] *= inverse;
        direction[index][1] *= inverse;
    }
    return derivative_norm;
}

static void random_direction(
    uint32_t *random_state,
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    do {
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            direction[index][0] = random_signed(random_state);
            direction[index][1] = random_signed(random_state);
        }
    } while (normalize_direction(direction) == 0.0f);
}

static void direction_at(
    const float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float w_x,
    float w_y,
    float displacement[2],
    float derivative[2]
) {
    displacement[0] = 0.0f;
    displacement[1] = 0.0f;
    derivative[0] = 0.0f;
    derivative[1] = 0.0f;

    float power[2] = {w_x, w_y};
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        int degree = index + 2;
        float next_power[2];
        complex_multiply(power[0], power[1], w_x, w_y, next_power);

        float term[2];
        complex_multiply(
            direction[index][0], direction[index][1],
            next_power[0], next_power[1], term
        );
        displacement[0] += term[0];
        displacement[1] += term[1];

        complex_multiply(
            direction[index][0], direction[index][1],
            power[0], power[1], term
        );
        derivative[0] += (float)degree * term[0];
        derivative[1] += (float)degree * term[1];

        power[0] = next_power[0];
        power[1] = next_power[1];
    }
}

static float coefficient_budget(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    float budget = 0.0f;
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        budget += (float)(index + 2) * hypotf(
            coefficients[index][0], coefficients[index][1]
        );
    }
    return budget;
}

static float outward_budget_slope(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    const float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    float slope = 0.0f;
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        float radius = hypotf(coefficients[index][0], coefficients[index][1]);
        if (radius < 1.0e-5f) {
            continue;
        }
        float along = (
            coefficients[index][0] * direction[index][0] +
            coefficients[index][1] * direction[index][1]
        ) / radius;
        slope += (float)(index + 2) * along;
    }
    return slope;
}

static float disturbance_score(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    const float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    float score = 0.0f;
    for (int sample = 0; sample < DISTURBANCE_SAMPLE_COUNT; ++sample) {
        float displacement[2];
        float derivative[2];
        direction_at(
            direction,
            disturbance_samples[sample][0], disturbance_samples[sample][1],
            displacement, derivative
        );
        float radius_squared =
            disturbance_samples[sample][0] * disturbance_samples[sample][0] +
            disturbance_samples[sample][1] * disturbance_samples[sample][1];
        float weight = 0.65f + 0.55f * radius_squared;
        score += weight * (
            displacement[0] * displacement[0] +
            displacement[1] * displacement[1]
        );
        score += 0.075f * weight * (
            derivative[0] * derivative[0] + derivative[1] * derivative[1]
        );
    }
    score /= (float)DISTURBANCE_SAMPLE_COUNT;

    float budget = coefficient_budget(coefficients);
    float slope = outward_budget_slope(coefficients, direction);
    if (budget > 0.62f && slope > 0.0f) {
        float closeness = (budget - 0.62f) / 0.26f;
        score += 0.7f * closeness * closeness * slope * slope;
    }
    return score;
}

static void search_direction(
    int worker_index,
    uint64_t generation,
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float heading[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float best_direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float *best_score,
    uint32_t *random_state
) {
    (void)worker_index;
    (void)generation;

    *best_score = INFINITY;
    for (int candidate_index = 0; candidate_index < SEARCH_CANDIDATES; ++candidate_index) {
        float candidate[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
        float fresh[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
        random_direction(random_state, fresh);

        float old_weight = candidate_index % 11 == 0 ? 0.0f : 0.82f;
        float new_weight = candidate_index % 11 == 0 ? 1.0f : 0.18f;
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            candidate[index][0] = old_weight * heading[index][0] + new_weight * fresh[index][0];
            candidate[index][1] = old_weight * heading[index][1] + new_weight * fresh[index][1];
        }
        if (normalize_direction(candidate) == 0.0f) {
            continue;
        }

        float score = disturbance_score(coefficients, candidate);
        if (score < *best_score) {
            *best_score = score;
            memcpy(best_direction, candidate, sizeof(candidate));
        }
    }

    if (isfinite(*best_score)) {
        memcpy(heading, best_direction, sizeof(float) * HOLOMORPHIC_WALK_COEFFICIENT_COUNT * 2u);
    }
}

static void *worker_main(void *argument) {
    struct walk_worker *worker = argument;
    uint64_t seen_generation = 0;
    uint32_t random_state = 0x9e3779b9u ^ (0x85ebca6bu * (uint32_t)(worker->index + 1));
    float heading[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    random_direction(&random_state, heading);

    while (true) {
        float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
        uint64_t generation;

        pthread_mutex_lock(&walk.mutex);
        while (!walk.stop && walk.generation == seen_generation) {
            pthread_cond_wait(&walk.changed, &walk.mutex);
        }
        if (walk.stop) {
            pthread_mutex_unlock(&walk.mutex);
            return NULL;
        }
        generation = walk.generation;
        memcpy(coefficients, walk.coefficients, sizeof(coefficients));
        pthread_mutex_unlock(&walk.mutex);

        float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
        float score;
        search_direction(
            worker->index,
            generation,
            coefficients,
            heading,
            direction,
            &score,
            &random_state
        );

        pthread_mutex_lock(&walk.mutex);
        if (!walk.stop && generation >= worker->result_generation && isfinite(score)) {
            worker->result_generation = generation;
            worker->score = score;
            memcpy(worker->direction, direction, sizeof(direction));
        }
        seen_generation = generation;
        pthread_mutex_unlock(&walk.mutex);
    }
}

bool holomorphic_walk_start(void) {
    pthread_mutex_lock(&walk.mutex);
    if (walk.running) {
        pthread_mutex_unlock(&walk.mutex);
        return true;
    }
    walk.stop = false;
    walk.generation = 0;
    memset(walk.coefficients, 0, sizeof(walk.coefficients));
    for (int index = 0; index < HOLOMORPHIC_WALK_WORKER_COUNT; ++index) {
        walk.workers[index].index = index;
        walk.workers[index].result_generation = 0;
        walk.workers[index].score = INFINITY;
    }
    walk.running = true;
    pthread_mutex_unlock(&walk.mutex);

    int created = 0;
    for (int index = 0; index < HOLOMORPHIC_WALK_WORKER_COUNT; ++index) {
        if (pthread_create(
                &walk.workers[index].thread,
                NULL,
                worker_main,
                &walk.workers[index]
            ) != 0) {
            pthread_mutex_lock(&walk.mutex);
            walk.stop = true;
            pthread_cond_broadcast(&walk.changed);
            pthread_mutex_unlock(&walk.mutex);
            for (int joined = 0; joined < created; ++joined) {
                pthread_join(walk.workers[joined].thread, NULL);
            }
            pthread_mutex_lock(&walk.mutex);
            walk.running = false;
            pthread_mutex_unlock(&walk.mutex);
            return false;
        }
        created += 1;
    }
    return true;
}

void holomorphic_walk_stop(void) {
    pthread_mutex_lock(&walk.mutex);
    if (!walk.running) {
        pthread_mutex_unlock(&walk.mutex);
        return;
    }
    walk.stop = true;
    pthread_cond_broadcast(&walk.changed);
    pthread_mutex_unlock(&walk.mutex);

    for (int index = 0; index < HOLOMORPHIC_WALK_WORKER_COUNT; ++index) {
        pthread_join(walk.workers[index].thread, NULL);
    }

    pthread_mutex_lock(&walk.mutex);
    walk.running = false;
    pthread_mutex_unlock(&walk.mutex);
}

void holomorphic_walk_publish(
    const float coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2]
) {
    pthread_mutex_lock(&walk.mutex);
    if (walk.running && !walk.stop) {
        memcpy(walk.coefficients, coefficients, sizeof(walk.coefficients));
        walk.generation += 1;
        pthread_cond_broadcast(&walk.changed);
    }
    pthread_mutex_unlock(&walk.mutex);
}

bool holomorphic_walk_best_direction(
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2],
    float *score
) {
    bool found = false;
    float best_score = INFINITY;

    pthread_mutex_lock(&walk.mutex);
    uint64_t minimum_generation = walk.generation > 2 ? walk.generation - 2 : 1;
    for (int index = 0; index < HOLOMORPHIC_WALK_WORKER_COUNT; ++index) {
        const struct walk_worker *worker = &walk.workers[index];
        if (
            worker->result_generation >= minimum_generation &&
            worker->score < best_score
        ) {
            best_score = worker->score;
            memcpy(direction, worker->direction, sizeof(worker->direction));
            found = true;
        }
    }
    pthread_mutex_unlock(&walk.mutex);

    if (found && score != NULL) {
        *score = best_score;
    }
    return found;
}
