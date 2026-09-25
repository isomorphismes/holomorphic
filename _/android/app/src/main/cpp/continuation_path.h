#ifndef WEGERT_CONTINUATION_PATH_H
#define WEGERT_CONTINUATION_PATH_H

#include <math.h>
#include <stdbool.h>

#define MAX_CONTINUATION_STEPS 24
#define CONTINUATION_UNBOUNDED_RADIUS (-1.0f)

struct continuation_path {
    float centers[MAX_CONTINUATION_STEPS][2];
    float radii[MAX_CONTINUATION_STEPS];
    int count;
};

static float continuation_distance(float first_x, float first_y, float second_x, float second_y) {
    return hypotf(first_x - second_x, first_y - second_y);
}

static bool continuation_same_factor(const float first[2], const float second[2]) {
    return first[0] == second[0] && first[1] == second[1];
}

/*
 * Repeated equal factors encode multiplicity.  The nth pole at a location is
 * cancelled exactly when there are at least n zero factors at that location.
 */
static bool continuation_pole_is_cancelled(
    float zeros[MAX_FACTORS][2],
    int zero_count,
    float poles[MAX_FACTORS][2],
    int pole_index
) {
    int matching_zeros = 0;
    for (int zero_index = 0; zero_index < zero_count; ++zero_index) {
        if (continuation_same_factor(zeros[zero_index], poles[pole_index])) {
            matching_zeros += 1;
        }
    }

    int pole_multiplicity = 0;
    for (int earlier_pole = 0; earlier_pole <= pole_index; ++earlier_pole) {
        if (continuation_same_factor(poles[earlier_pole], poles[pole_index])) {
            pole_multiplicity += 1;
        }
    }
    return pole_multiplicity <= matching_zeros;
}

static float continuation_radius_at(
    float center_x,
    float center_y,
    float zeros[MAX_FACTORS][2],
    int zero_count,
    float poles[MAX_FACTORS][2],
    int pole_count
) {
    float nearest = CONTINUATION_UNBOUNDED_RADIUS;
    for (int pole_index = 0; pole_index < pole_count; ++pole_index) {
        if (continuation_pole_is_cancelled(zeros, zero_count, poles, pole_index)) {
            continue;
        }

        float distance = continuation_distance(
            center_x,
            center_y,
            poles[pole_index][0],
            poles[pole_index][1]
        );
        if (nearest < 0.0f || distance < nearest) {
            nearest = distance;
        }
    }
    return nearest;
}

static bool continuation_center_is_regular(float radius) {
    return radius < 0.0f || radius > 0.0f;
}

static void continuation_path_clear(struct continuation_path *path) {
    path->count = 0;
}

static bool continuation_path_seed(
    struct continuation_path *path,
    float center_x,
    float center_y,
    float zeros[MAX_FACTORS][2],
    int zero_count,
    float poles[MAX_FACTORS][2],
    int pole_count
) {
    float radius = continuation_radius_at(
        center_x,
        center_y,
        zeros,
        zero_count,
        poles,
        pole_count
    );
    if (!continuation_center_is_regular(radius)) {
        return false;
    }

    path->centers[0][0] = center_x;
    path->centers[0][1] = center_y;
    path->radii[0] = radius;
    path->count = 1;
    return true;
}

static bool continuation_path_append(
    struct continuation_path *path,
    float center_x,
    float center_y,
    float zeros[MAX_FACTORS][2],
    int zero_count,
    float poles[MAX_FACTORS][2],
    int pole_count
) {
    if (path->count <= 0 || path->count >= MAX_CONTINUATION_STEPS) {
        return false;
    }

    int previous = path->count - 1;
    float previous_radius = path->radii[previous];
    float distance_from_previous = continuation_distance(
        center_x,
        center_y,
        path->centers[previous][0],
        path->centers[previous][1]
    );

    /* A Taylor disc is open; a point on its singular boundary is not valid. */
    if (previous_radius >= 0.0f && distance_from_previous >= previous_radius) {
        return false;
    }

    float radius = continuation_radius_at(
        center_x,
        center_y,
        zeros,
        zero_count,
        poles,
        pole_count
    );
    if (!continuation_center_is_regular(radius)) {
        return false;
    }

    int next = path->count;
    path->centers[next][0] = center_x;
    path->centers[next][1] = center_y;
    path->radii[next] = radius;
    path->count += 1;
    return true;
}

static bool continuation_path_add_center(
    struct continuation_path *path,
    float center_x,
    float center_y,
    float zeros[MAX_FACTORS][2],
    int zero_count,
    float poles[MAX_FACTORS][2],
    int pole_count
) {
    if (path->count == 0) {
        return continuation_path_seed(
            path,
            center_x,
            center_y,
            zeros,
            zero_count,
            poles,
            pole_count
        );
    }
    return continuation_path_append(
        path,
        center_x,
        center_y,
        zeros,
        zero_count,
        poles,
        pole_count
    );
}

#endif

