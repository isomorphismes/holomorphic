#ifndef WEGERT_FACTOR_SNAP_H
#define WEGERT_FACTOR_SNAP_H

/*
 * Convert a screen-space touch target to world units, then copy the nearest
 * stored factor exactly.  This is deliberately a UI operation: callers that
 * insert or evaluate programmatic coordinates do not pass through it.
 */
static int factor_snap_to_nearest(
    float point[2],
    float factors[MAX_FACTORS][2],
    int factor_count,
    float world_per_pixel,
    float touch_radius_pixels
) {
    if (factor_count <= 0 || world_per_pixel <= 0.0f || touch_radius_pixels < 0.0f) {
        return -1;
    }

    float world_radius = world_per_pixel * touch_radius_pixels;
    float nearest_distance_squared = world_radius * world_radius;
    int nearest_index = -1;

    for (int index = 0; index < factor_count; ++index) {
        float delta_x = point[0] - factors[index][0];
        float delta_y = point[1] - factors[index][1];
        float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (distance_squared <= nearest_distance_squared) {
            nearest_distance_squared = distance_squared;
            nearest_index = index;
        }
    }

    if (nearest_index >= 0) {
        point[0] = factors[nearest_index][0];
        point[1] = factors[nearest_index][1];
    }
    return nearest_index;
}

#endif

