#ifndef ANALYTIC_CONTINUATION_SCENARIO_H
#define ANALYTIC_CONTINUATION_SCENARIO_H

#include <stdbool.h>
#include <stddef.h>

#include "field_evolution.h"
#include "motion.h"
#include "presentation.h"
#include "scene.h"

#define SCENARIO_NAME_CAPACITY 64

struct scenario {
    char name[SCENARIO_NAME_CAPACITY];
    struct scene scene;
    struct motion_program motion;
    struct presentation_config presentation;
    enum field_background_mode field_background;
    float field_speed;
    float field_budget;
};

void scenario_initialize_interactive(struct scenario *scenario);

bool scenario_parse_text(
    struct scenario *scenario,
    const char *text,
    char *error,
    size_t error_capacity
);

#endif
