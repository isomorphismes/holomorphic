#ifndef PERTURBATION_RENDER_H
#define PERTURBATION_RENDER_H

#include <stdbool.h>
#include <GLES3/gl3.h>

bool perturbation_render_bind(GLuint program);
bool perturbation_render_is_running(void);
void perturbation_render_upload(
    float center_x,
    float center_y,
    float half_height,
    float aspect
);
void perturbation_render_shutdown(void);

#endif
