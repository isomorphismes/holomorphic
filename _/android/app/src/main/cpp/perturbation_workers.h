#ifndef PERTURBATION_WORKERS_H
#define PERTURBATION_WORKERS_H

#include <stdbool.h>

#define PERTURBATION_WORKER_COUNT 3

struct perturbation_descriptor {
    float disk_center[2];
    float inverse_radius;
    float anchor[2];
    float kernel_scale;
    float phase_amplitude;
};

struct perturbation_snapshot {
    int count;
    struct perturbation_descriptor items[PERTURBATION_WORKER_COUNT];
};

struct perturbation_system;

bool perturbation_system_start(struct perturbation_system **system_out);
void perturbation_system_stop(struct perturbation_system *system);
void perturbation_system_set_view(
    struct perturbation_system *system,
    float center_x,
    float center_y,
    float half_height,
    float aspect
);
void perturbation_system_copy_snapshot(
    struct perturbation_system *system,
    struct perturbation_snapshot *snapshot_out
);

#endif
