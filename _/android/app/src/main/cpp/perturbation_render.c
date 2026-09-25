#include "perturbation_render.h"
#include "perturbation_workers.h"

#include <string.h>

static struct perturbation_system *perturbation_system_instance = NULL;

static GLint perturbation_count_location = -1;
static GLint perturbation_disk_centers_location = -1;
static GLint perturbation_inverse_radii_location = -1;
static GLint perturbation_anchors_location = -1;
static GLint perturbation_kernel_scales_location = -1;
static GLint perturbation_phase_amplitudes_location = -1;

bool perturbation_render_bind(GLuint program) {
    perturbation_count_location = glGetUniformLocation(
        program,
        "u_perturbation_count"
    );
    perturbation_disk_centers_location = glGetUniformLocation(
        program,
        "u_perturbation_disk_centers[0]"
    );
    perturbation_inverse_radii_location = glGetUniformLocation(
        program,
        "u_perturbation_inverse_radii[0]"
    );
    perturbation_anchors_location = glGetUniformLocation(
        program,
        "u_perturbation_anchors[0]"
    );
    perturbation_kernel_scales_location = glGetUniformLocation(
        program,
        "u_perturbation_kernel_scales[0]"
    );
    perturbation_phase_amplitudes_location = glGetUniformLocation(
        program,
        "u_perturbation_phase_amplitudes[0]"
    );

    if (perturbation_system_instance == NULL) {
        if (!perturbation_system_start(&perturbation_system_instance)) {
            return false;
        }
    }
    return true;
}

bool perturbation_render_is_running(void) {
    return perturbation_system_instance != NULL;
}

void perturbation_render_upload(
    float center_x,
    float center_y,
    float half_height,
    float aspect
) {
    if (perturbation_system_instance == NULL) {
        return;
    }

    perturbation_system_set_view(
        perturbation_system_instance,
        center_x,
        center_y,
        half_height,
        aspect
    );

    struct perturbation_snapshot snapshot;
    perturbation_system_copy_snapshot(
        perturbation_system_instance,
        &snapshot
    );

    float disk_centers[PERTURBATION_WORKER_COUNT][2];
    float inverse_radii[PERTURBATION_WORKER_COUNT];
    float anchors[PERTURBATION_WORKER_COUNT][2];
    float kernel_scales[PERTURBATION_WORKER_COUNT];
    float phase_amplitudes[PERTURBATION_WORKER_COUNT];

    memset(disk_centers, 0, sizeof(disk_centers));
    memset(inverse_radii, 0, sizeof(inverse_radii));
    memset(anchors, 0, sizeof(anchors));
    memset(kernel_scales, 0, sizeof(kernel_scales));
    memset(phase_amplitudes, 0, sizeof(phase_amplitudes));

    for (int index = 0; index < snapshot.count; ++index) {
        const struct perturbation_descriptor *descriptor = &snapshot.items[index];
        disk_centers[index][0] = descriptor->disk_center[0];
        disk_centers[index][1] = descriptor->disk_center[1];
        inverse_radii[index] = descriptor->inverse_radius;
        anchors[index][0] = descriptor->anchor[0];
        anchors[index][1] = descriptor->anchor[1];
        kernel_scales[index] = descriptor->kernel_scale;
        phase_amplitudes[index] = descriptor->phase_amplitude;
    }

    glUniform1i(perturbation_count_location, snapshot.count);
    glUniform2fv(
        perturbation_disk_centers_location,
        PERTURBATION_WORKER_COUNT,
        &disk_centers[0][0]
    );
    glUniform1fv(
        perturbation_inverse_radii_location,
        PERTURBATION_WORKER_COUNT,
        inverse_radii
    );
    glUniform2fv(
        perturbation_anchors_location,
        PERTURBATION_WORKER_COUNT,
        &anchors[0][0]
    );
    glUniform1fv(
        perturbation_kernel_scales_location,
        PERTURBATION_WORKER_COUNT,
        kernel_scales
    );
    glUniform1fv(
        perturbation_phase_amplitudes_location,
        PERTURBATION_WORKER_COUNT,
        phase_amplitudes
    );
}

void perturbation_render_shutdown(void) {
    if (perturbation_system_instance == NULL) {
        return;
    }
    perturbation_system_stop(perturbation_system_instance);
    perturbation_system_instance = NULL;
}

__attribute__((destructor))
static void perturbation_render_destructor(void) {
    perturbation_render_shutdown();
}
