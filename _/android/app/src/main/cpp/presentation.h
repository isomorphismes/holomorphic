#ifndef ANALYTIC_CONTINUATION_PRESENTATION_H
#define ANALYTIC_CONTINUATION_PRESENTATION_H

#include <stdbool.h>

struct presentation_config {
    bool show_controls;
    bool interaction_enabled;
    float marker_radius_px;
    float marker_stroke_px;
};

void presentation_config_interactive(struct presentation_config *config);
void presentation_config_clean(struct presentation_config *config);

#endif
