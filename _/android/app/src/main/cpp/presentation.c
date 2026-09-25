#include "presentation.h"

void presentation_config_interactive(struct presentation_config *config) {
    config->show_controls = true;
    config->interaction_enabled = true;
    config->marker_radius_px = 9.0f;
    config->marker_stroke_px = 4.0f;
}

void presentation_config_clean(struct presentation_config *config) {
    config->show_controls = false;
    config->interaction_enabled = false;
    config->marker_radius_px = 5.5f;
    config->marker_stroke_px = 2.2f;
}
