#include "scenario.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t capacity, const char *message) {
    if (error == NULL || capacity == 0) return;
    snprintf(error, capacity, "%s", message);
}

static char *trim(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) ++text;
    char *finish = text + strlen(text);
    while (finish > text && isspace((unsigned char)finish[-1])) --finish;
    *finish = '\0';
    return text;
}

static bool parse_float_value(const char *text, float *value) {
    char *finish = NULL;
    float parsed = strtof(text, &finish);
    if (finish == text) return false;
    while (*finish != '\0' && isspace((unsigned char)*finish)) ++finish;
    if (*finish != '\0') return false;
    *value = parsed;
    return true;
}

static bool parse_bool_value(const char *text, bool *value) {
    if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0 || strcmp(text, "yes") == 0) {
        *value = true;
        return true;
    }
    if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0 || strcmp(text, "no") == 0) {
        *value = false;
        return true;
    }
    return false;
}

static bool parse_factor_kind(const char *text, enum scene_factor_kind *kind) {
    if (strcmp(text, "zero") == 0 || strcmp(text, "hole") == 0) {
        *kind = SCENE_FACTOR_ZERO;
        return true;
    }
    if (strcmp(text, "pole") == 0) {
        *kind = SCENE_FACTOR_POLE;
        return true;
    }
    return false;
}

static bool parse_point(const char *text, float *x, float *y) {
    char extra = '\0';
    return sscanf(text, " %f , %f %c", x, y, &extra) == 2;
}

static bool parse_exchange(
    struct scenario *scenario,
    const char *text,
    char *error,
    size_t error_capacity
) {
    char first_kind_text[16] = {0};
    char second_kind_text[16] = {0};
    char turn_text[16] = {0};
    int first_index = -1;
    int second_index = -1;
    float start_seconds = 0.0f;
    float duration_seconds = 0.0f;

    int matched = sscanf(
        text,
        " %15[^,] , %d , %15[^,] , %d , %f , %f , %15s",
        first_kind_text,
        &first_index,
        second_kind_text,
        &second_index,
        &start_seconds,
        &duration_seconds,
        turn_text
    );
    if (matched != 7) {
        set_error(
            error,
            error_capacity,
            "exchange must be kind,index,kind,index,start,duration,cw|ccw"
        );
        return false;
    }

    enum scene_factor_kind first_kind;
    enum scene_factor_kind second_kind;
    char *first_trimmed = trim(first_kind_text);
    char *second_trimmed = trim(second_kind_text);
    char *turn_trimmed = trim(turn_text);
    if (
        !parse_factor_kind(first_trimmed, &first_kind) ||
        !parse_factor_kind(second_trimmed, &second_kind)
    ) {
        set_error(error, error_capacity, "exchange factor kind must be zero/hole or pole");
        return false;
    }

    enum motion_turn turn;
    if (strcmp(turn_trimmed, "cw") == 0) {
        turn = MOTION_TURN_CLOCKWISE;
    } else if (strcmp(turn_trimmed, "ccw") == 0) {
        turn = MOTION_TURN_COUNTERCLOCKWISE;
    } else {
        set_error(error, error_capacity, "exchange turn must be cw or ccw");
        return false;
    }

    if (!motion_program_add_exchange(
            &scenario->motion,
            (struct motion_factor_ref){first_kind, first_index},
            (struct motion_factor_ref){second_kind, second_index},
            start_seconds,
            duration_seconds,
            turn
        )) {
        set_error(error, error_capacity, "invalid or excessive exchange event");
        return false;
    }
    return true;
}

void scenario_initialize_interactive(struct scenario *scenario) {
    memset(scenario, 0, sizeof(*scenario));
    snprintf(scenario->name, sizeof(scenario->name), "%s", "interactive");
    scene_initialize_default(&scenario->scene);
    motion_program_initialize(&scenario->motion);
    presentation_config_interactive(&scenario->presentation);
    scenario->field_background = FIELD_BACKGROUND_ENTIRE;
    scenario->field_speed = 0.30f;
    scenario->field_budget = HOLOMORPHIC_WALK_DEFAULT_COEFFICIENT_BUDGET;
}

bool scenario_parse_text(
    struct scenario *scenario,
    const char *text,
    char *error,
    size_t error_capacity
) {
    scenario_initialize_interactive(scenario);
    if (error != NULL && error_capacity > 0) error[0] = '\0';
    if (text == NULL) {
        set_error(error, error_capacity, "scenario text is null");
        return false;
    }

    bool custom_factors = false;
    const char *cursor = text;
    int line_number = 0;

    while (*cursor != '\0') {
        ++line_number;
        const char *line_end = strchr(cursor, '\n');
        size_t length = line_end == NULL ? strlen(cursor) : (size_t)(line_end - cursor);
        if (length >= 256) {
            set_error(error, error_capacity, "scenario line is too long");
            return false;
        }

        char line[256];
        memcpy(line, cursor, length);
        line[length] = '\0';
        cursor = line_end == NULL ? cursor + length : line_end + 1;

        char *comment = strchr(line, '#');
        if (comment != NULL) *comment = '\0';
        char *trimmed = trim(line);
        if (*trimmed == '\0') continue;

        char *equals = strchr(trimmed, '=');
        if (equals == NULL) {
            char message[96];
            snprintf(message, sizeof(message), "scenario line %d has no =", line_number);
            set_error(error, error_capacity, message);
            return false;
        }
        *equals = '\0';
        char *key = trim(trimmed);
        char *value = trim(equals + 1);

        if (strcmp(key, "name") == 0) {
            if (*value == '\0' || strlen(value) >= sizeof(scenario->name)) {
                set_error(error, error_capacity, "invalid scenario name");
                return false;
            }
            snprintf(scenario->name, sizeof(scenario->name), "%s", value);
        } else if (strcmp(key, "zero") == 0 || strcmp(key, "pole") == 0 || strcmp(key, "hole") == 0) {
            if (!custom_factors) {
                scene_clear_factors(&scenario->scene);
                custom_factors = true;
            }
            float x = 0.0f;
            float y = 0.0f;
            if (!parse_point(value, &x, &y)) {
                set_error(error, error_capacity, "factor position must be x,y");
                return false;
            }
            enum scene_factor_kind kind = strcmp(key, "pole") == 0
                ? SCENE_FACTOR_POLE
                : SCENE_FACTOR_ZERO;
            if (!scene_add_factor(&scenario->scene, kind, x, y, NULL)) {
                set_error(error, error_capacity, "too many factors in scenario");
                return false;
            }
        } else if (strcmp(key, "zoom") == 0) {
            if (!parse_float_value(value, &scenario->scene.zoom) || scenario->scene.zoom <= 0.0f) {
                set_error(error, error_capacity, "zoom must be positive");
                return false;
            }
        } else if (strcmp(key, "presentation") == 0) {
            if (strcmp(value, "interactive") == 0) {
                presentation_config_interactive(&scenario->presentation);
            } else if (strcmp(value, "clean") == 0) {
                presentation_config_clean(&scenario->presentation);
            } else {
                set_error(error, error_capacity, "presentation must be interactive or clean");
                return false;
            }
        } else if (strcmp(key, "show_controls") == 0) {
            if (!parse_bool_value(value, &scenario->presentation.show_controls)) {
                set_error(error, error_capacity, "show_controls must be boolean");
                return false;
            }
        } else if (strcmp(key, "interaction") == 0) {
            if (!parse_bool_value(value, &scenario->presentation.interaction_enabled)) {
                set_error(error, error_capacity, "interaction must be boolean");
                return false;
            }
        } else if (strcmp(key, "marker_radius") == 0) {
            if (!parse_float_value(value, &scenario->presentation.marker_radius_px)) {
                set_error(error, error_capacity, "marker_radius must be numeric");
                return false;
            }
        } else if (strcmp(key, "marker_stroke") == 0) {
            if (!parse_float_value(value, &scenario->presentation.marker_stroke_px)) {
                set_error(error, error_capacity, "marker_stroke must be numeric");
                return false;
            }
        } else if (strcmp(key, "field") == 0) {
            if (strcmp(value, "entire") == 0) {
                scenario->field_background = FIELD_BACKGROUND_ENTIRE;
            } else if (
                strcmp(value, "wandering_offscreen_poles") == 0 ||
                strcmp(value, "remote_poles") == 0
            ) {
                scenario->field_background = FIELD_BACKGROUND_WANDERING_OFFSCREEN_POLES;
            } else {
                set_error(error, error_capacity, "field must be entire or wandering_offscreen_poles");
                return false;
            }
        } else if (strcmp(key, "field_speed") == 0) {
            if (!parse_float_value(value, &scenario->field_speed) || scenario->field_speed <= 0.0f) {
                set_error(error, error_capacity, "field_speed must be positive");
                return false;
            }
        } else if (strcmp(key, "field_budget") == 0) {
            if (!parse_float_value(value, &scenario->field_budget) || scenario->field_budget <= 0.53f) {
                set_error(error, error_capacity, "field_budget must be greater than 0.53");
                return false;
            }
        } else if (strcmp(key, "motion") == 0) {
            if (strcmp(value, "none") == 0) {
                scenario->motion.enabled = false;
                scenario->motion.wander_speed = 0.0f;
            } else if (strcmp(value, "wander") == 0) {
                scenario->motion.enabled = true;
            } else {
                set_error(error, error_capacity, "motion must be none or wander");
                return false;
            }
        } else if (strcmp(key, "seed") == 0) {
            char *finish = NULL;
            unsigned long parsed = strtoul(value, &finish, 10);
            while (*finish != '\0' && isspace((unsigned char)*finish)) ++finish;
            if (finish == value || *finish != '\0') {
                set_error(error, error_capacity, "seed must be an unsigned integer");
                return false;
            }
            scenario->motion.seed = (uint32_t)parsed;
        } else if (strcmp(key, "wander_speed") == 0) {
            if (!parse_float_value(value, &scenario->motion.wander_speed) || scenario->motion.wander_speed < 0.0f) {
                set_error(error, error_capacity, "wander_speed must be nonnegative");
                return false;
            }
            if (scenario->motion.wander_speed > 0.0f) scenario->motion.enabled = true;
        } else if (strcmp(key, "wander_bound") == 0) {
            if (!parse_float_value(value, &scenario->motion.wander_bound) || scenario->motion.wander_bound < 0.0f) {
                set_error(error, error_capacity, "wander_bound must be nonnegative");
                return false;
            }
        } else if (strcmp(key, "exchange") == 0) {
            if (!parse_exchange(scenario, value, error, error_capacity)) {
                return false;
            }
        } else {
            char message[128];
            snprintf(message, sizeof(message), "unknown scenario key on line %d: %s", line_number, key);
            set_error(error, error_capacity, message);
            return false;
        }
    }

    if (
        scenario->presentation.marker_radius_px <= 0.0f ||
        scenario->presentation.marker_stroke_px <= 0.0f ||
        scenario->presentation.marker_stroke_px >= scenario->presentation.marker_radius_px
    ) {
        set_error(error, error_capacity, "marker sizes must satisfy 0 < stroke < radius");
        return false;
    }

    if (scenario->field_speed <= 0.0f || scenario->field_budget <= 0.53f) {
        set_error(error, error_capacity, "field speed and budget are invalid");
        return false;
    }

    if (!motion_program_validate(&scenario->motion, &scenario->scene)) {
        set_error(error, error_capacity, "motion program references an invalid factor or parameter");
        return false;
    }

    return true;
}
