#include "perturbation_workers.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PERTURBATION_COORDINATOR_PERIOD_NS 16000000L
#define PERTURBATION_MIN_PHASE 0.055f
#define PERTURBATION_MAX_PHASE 0.135f
#define PERTURBATION_MIN_DECAY_SECONDS 0.70f
#define PERTURBATION_MAX_DECAY_SECONDS 1.20f
#define PERTURBATION_RECYCLE_PHASE 0.0045f
#define PERTURBATION_DISK_MARGIN 1.05f

struct perturbation_view {
    float center[2];
    float half_height;
    float aspect;
    uint64_t generation;
};

struct perturbation_slot {
    struct perturbation_descriptor descriptor;
    float initial_phase_amplitude;
    float decay_seconds;
    uint64_t born_nanoseconds;
    bool ready;
    bool request_new;
};

struct worker_context {
    struct perturbation_system *system;
    int worker_index;
    uint32_t random_state;
};

struct perturbation_system {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool running;

    struct perturbation_view view;
    struct perturbation_slot slots[PERTURBATION_WORKER_COUNT];
    struct perturbation_snapshot published;

    pthread_t workers[PERTURBATION_WORKER_COUNT];
    pthread_t coordinator;
    struct worker_context worker_contexts[PERTURBATION_WORKER_COUNT];
    int workers_started;
    bool coordinator_started;
};

static uint64_t monotonic_nanoseconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * UINT64_C(1000000000)
        + (uint64_t)now.tv_nsec;
}

static uint32_t xorshift32(uint32_t *state) {
    uint32_t value = *state;
    if (value == 0u) {
        value = 0x6d2b79f5u;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static float random_unit(uint32_t *state) {
    return (float)(xorshift32(state) >> 8) * (1.0f / 16777216.0f);
}

static float random_range(uint32_t *state, float low, float high) {
    return low + (high - low) * random_unit(state);
}

static struct perturbation_descriptor make_descriptor(
    const struct perturbation_view *view,
    uint32_t *random_state,
    float *initial_phase_out,
    float *decay_seconds_out
) {
    struct perturbation_descriptor descriptor;
    memset(&descriptor, 0, sizeof(descriptor));

    float half_height = fmaxf(view->half_height, 1.0e-4f);
    float aspect = fmaxf(view->aspect, 1.0e-3f);
    float ndc_x = random_range(random_state, -1.0f, 1.0f);
    float ndc_y = random_range(random_state, -1.0f, 1.0f);

    float disk_radius = PERTURBATION_DISK_MARGIN
        * half_height
        * hypotf(aspect, 1.0f);
    float inverse_radius = 1.0f / fmaxf(disk_radius, 1.0e-6f);

    float point_x = view->center[0] + ndc_x * half_height * aspect;
    float point_y = view->center[1] + ndc_y * half_height;
    float anchor_x = (point_x - view->center[0]) * inverse_radius;
    float anchor_y = (point_y - view->center[1]) * inverse_radius;
    float one_minus_anchor_squared = fmaxf(
        1.0f - anchor_x * anchor_x - anchor_y * anchor_y,
        1.0e-4f
    );

    float magnitude = random_range(
        random_state,
        PERTURBATION_MIN_PHASE,
        PERTURBATION_MAX_PHASE
    );
    float sign = (xorshift32(random_state) & 1u) != 0u ? 1.0f : -1.0f;

    descriptor.disk_center[0] = view->center[0];
    descriptor.disk_center[1] = view->center[1];
    descriptor.inverse_radius = inverse_radius;
    descriptor.anchor[0] = anchor_x;
    descriptor.anchor[1] = anchor_y;
    descriptor.kernel_scale = one_minus_anchor_squared * one_minus_anchor_squared;
    descriptor.phase_amplitude = sign * magnitude;

    *initial_phase_out = descriptor.phase_amplitude;
    *decay_seconds_out = random_range(
        random_state,
        PERTURBATION_MIN_DECAY_SECONDS,
        PERTURBATION_MAX_DECAY_SECONDS
    );
    return descriptor;
}

static void *worker_main(void *argument) {
    struct worker_context *context = argument;
    struct perturbation_system *system = context->system;
    int worker_index = context->worker_index;

    while (true) {
        pthread_mutex_lock(&system->mutex);
        while (system->running && !system->slots[worker_index].request_new) {
            pthread_cond_wait(&system->condition, &system->mutex);
        }
        if (!system->running) {
            pthread_mutex_unlock(&system->mutex);
            return NULL;
        }

        system->slots[worker_index].request_new = false;
        struct perturbation_view view = system->view;
        pthread_mutex_unlock(&system->mutex);

        float initial_phase = 0.0f;
        float decay_seconds = 1.0f;
        struct perturbation_descriptor descriptor = make_descriptor(
            &view,
            &context->random_state,
            &initial_phase,
            &decay_seconds
        );

        pthread_mutex_lock(&system->mutex);
        if (system->running && view.generation == system->view.generation) {
            struct perturbation_slot *slot = &system->slots[worker_index];
            slot->descriptor = descriptor;
            slot->initial_phase_amplitude = initial_phase;
            slot->decay_seconds = decay_seconds;
            slot->born_nanoseconds = monotonic_nanoseconds();
            slot->ready = true;
        } else if (system->running) {
            // The camera changed while this descriptor was being built.
            // Discard it rather than allowing its exterior kernel pole to
            // become visible in the new panel.
            system->slots[worker_index].request_new = true;
        }
        pthread_mutex_unlock(&system->mutex);
    }
}

static void publish_snapshot_locked(
    struct perturbation_system *system,
    uint64_t now_nanoseconds
) {
    struct perturbation_snapshot snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    for (int index = 0; index < PERTURBATION_WORKER_COUNT; ++index) {
        struct perturbation_slot *slot = &system->slots[index];
        if (!slot->ready) {
            if (!slot->request_new) {
                slot->request_new = true;
            }
            continue;
        }

        uint64_t age_nanoseconds = now_nanoseconds >= slot->born_nanoseconds
            ? now_nanoseconds - slot->born_nanoseconds
            : UINT64_C(0);
        float age_seconds = (float)age_nanoseconds * 1.0e-9f;
        float phase = slot->initial_phase_amplitude
            * expf(-age_seconds / fmaxf(slot->decay_seconds, 1.0e-3f));
        if (fabsf(phase) < PERTURBATION_RECYCLE_PHASE) {
            slot->ready = false;
            slot->request_new = true;
            continue;
        }

        struct perturbation_descriptor descriptor = slot->descriptor;
        descriptor.phase_amplitude = phase;
        snapshot.items[snapshot.count++] = descriptor;
    }

    system->published = snapshot;
}

static void *coordinator_main(void *argument) {
    struct perturbation_system *system = argument;
    const struct timespec sleep_time = {
        .tv_sec = 0,
        .tv_nsec = PERTURBATION_COORDINATOR_PERIOD_NS
    };

    while (true) {
        pthread_mutex_lock(&system->mutex);
        if (!system->running) {
            pthread_mutex_unlock(&system->mutex);
            return NULL;
        }

        publish_snapshot_locked(system, monotonic_nanoseconds());
        pthread_cond_broadcast(&system->condition);
        pthread_mutex_unlock(&system->mutex);
        nanosleep(&sleep_time, NULL);
    }
}

bool perturbation_system_start(struct perturbation_system **system_out) {
    if (system_out == NULL) {
        return false;
    }
    *system_out = NULL;

    struct perturbation_system *system = calloc(1, sizeof(*system));
    if (system == NULL) {
        return false;
    }

    if (pthread_mutex_init(&system->mutex, NULL) != 0) {
        free(system);
        return false;
    }
    if (pthread_cond_init(&system->condition, NULL) != 0) {
        pthread_mutex_destroy(&system->mutex);
        free(system);
        return false;
    }

    system->running = true;
    system->view.center[0] = 0.0f;
    system->view.center[1] = 0.0f;
    system->view.half_height = 3.5f;
    system->view.aspect = 1.0f;
    system->view.generation = UINT64_C(1);

    uint64_t seed_time = monotonic_nanoseconds();
    uint32_t seed = (uint32_t)(seed_time ^ (seed_time >> 32));
    seed ^= (uint32_t)(uintptr_t)system;

    for (int index = 0; index < PERTURBATION_WORKER_COUNT; ++index) {
        system->slots[index].request_new = true;
        system->worker_contexts[index].system = system;
        system->worker_contexts[index].worker_index = index;
        system->worker_contexts[index].random_state = seed
            ^ (0x9e3779b9u * (uint32_t)(index + 1));

        if (pthread_create(
                &system->workers[index],
                NULL,
                worker_main,
                &system->worker_contexts[index]
            ) != 0) {
            perturbation_system_stop(system);
            return false;
        }
        system->workers_started += 1;
    }

    if (pthread_create(&system->coordinator, NULL, coordinator_main, system) != 0) {
        perturbation_system_stop(system);
        return false;
    }
    system->coordinator_started = true;

    pthread_mutex_lock(&system->mutex);
    pthread_cond_broadcast(&system->condition);
    pthread_mutex_unlock(&system->mutex);

    *system_out = system;
    return true;
}

void perturbation_system_stop(struct perturbation_system *system) {
    if (system == NULL) {
        return;
    }

    pthread_mutex_lock(&system->mutex);
    system->running = false;
    pthread_cond_broadcast(&system->condition);
    pthread_mutex_unlock(&system->mutex);

    for (int index = 0; index < system->workers_started; ++index) {
        pthread_join(system->workers[index], NULL);
    }
    if (system->coordinator_started) {
        pthread_join(system->coordinator, NULL);
    }

    pthread_cond_destroy(&system->condition);
    pthread_mutex_destroy(&system->mutex);
    free(system);
}

void perturbation_system_set_view(
    struct perturbation_system *system,
    float center_x,
    float center_y,
    float half_height,
    float aspect
) {
    if (system == NULL) {
        return;
    }

    float new_half_height = fmaxf(half_height, 1.0e-4f);
    float new_aspect = fmaxf(aspect, 1.0e-3f);

    pthread_mutex_lock(&system->mutex);
    bool changed = center_x != system->view.center[0]
        || center_y != system->view.center[1]
        || new_half_height != system->view.half_height
        || new_aspect != system->view.aspect;

    if (changed) {
        system->view.center[0] = center_x;
        system->view.center[1] = center_y;
        system->view.half_height = new_half_height;
        system->view.aspect = new_aspect;
        system->view.generation += UINT64_C(1);

        memset(&system->published, 0, sizeof(system->published));
        for (int index = 0; index < PERTURBATION_WORKER_COUNT; ++index) {
            system->slots[index].ready = false;
            system->slots[index].request_new = true;
        }
        pthread_cond_broadcast(&system->condition);
    }
    pthread_mutex_unlock(&system->mutex);
}

void perturbation_system_copy_snapshot(
    struct perturbation_system *system,
    struct perturbation_snapshot *snapshot_out
) {
    if (snapshot_out == NULL) {
        return;
    }
    memset(snapshot_out, 0, sizeof(*snapshot_out));
    if (system == NULL) {
        return;
    }

    pthread_mutex_lock(&system->mutex);
    *snapshot_out = system->published;
    pthread_mutex_unlock(&system->mutex);
}
