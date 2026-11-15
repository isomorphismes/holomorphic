#ifndef HOLOMORPHIC_DIVISOR_H
#define HOLOMORPHIC_DIVISOR_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

#define HOLOMORPHIC_REMOTE_POLE_COUNT 24
#define HOLOMORPHIC_DIVISOR_CAPACITY 96
/* World units, independent of resolution, zoom, pan, and presentation. */
#define HOLOMORPHIC_ORBIT_WORLD_SCALE 2.0

struct holomorphic_divisor {
    float factors[HOLOMORPHIC_DIVISOR_CAPACITY][3];
    int count;
};

static inline double holomorphic_orbit_hash(double index) {
    double value = sin(index * 127.1 + 31.7) * 43758.5453123;
    return value - floor(value);
}

/* The previous smooth orbit family, with its camera-dependent radius replaced
 * by one fixed world scale. Calculate once per frame on the host: every GPU
 * receives exactly the same factor positions rather than a GPU-specific hash. */
static inline bool holomorphic_remote_pole(int index, double time, float position[2]) {
    if (index < 0 || index >= HOLOMORPHIC_REMOTE_POLE_COUNT || !isfinite(time))
        return false;
    double initial_angle = 6.28318530717958647692 * holomorphic_orbit_hash(index + 0.11);
    double handedness = holomorphic_orbit_hash(index + 7.91) < 0.5 ? -1.0 : 1.0;
    double speed = 0.11 + 0.08 * holomorphic_orbit_hash(index + 4.37);
    double angle = initial_angle + handedness * speed * time;
    double bend_phase = 6.28318530717958647692 * holomorphic_orbit_hash(index + 11.23);
    double radius = 2.75 + 1.25 * holomorphic_orbit_hash(index + 1.73)
        + 0.22 * sin(2.0 * angle + bend_phase);
    double ellipticity = -0.08 + 0.16 * holomorphic_orbit_hash(index + 14.67);
    position[0] = (float)(HOLOMORPHIC_ORBIT_WORLD_SCALE * radius
        * (1.0 + ellipticity) * cos(angle));
    position[1] = (float)(HOLOMORPHIC_ORBIT_WORLD_SCALE * radius
        * (1.0 - ellipticity) * sin(angle));
    return isfinite(position[0]) && isfinite(position[1]);
}

/* Equality is exact equality of the actual host->shader coordinates. A nearby
 * displaced pair is never an exact pair. Group signed integer multiplicities
 * before the shader evaluates a distance, phase, logarithm, or division. */
static inline bool holomorphic_divisor_insert(
    struct holomorphic_divisor *divisor, const float position[2], int order
) {
    if (!isfinite(position[0]) || !isfinite(position[1]) || (order != 1 && order != -1))
        return false;
    for (int index = 0; index < divisor->count; ++index) {
        if (divisor->factors[index][0] == position[0]
                && divisor->factors[index][1] == position[1]) {
            divisor->factors[index][2] += (float)order;
            return true;
        }
    }
    if (divisor->count >= HOLOMORPHIC_DIVISOR_CAPACITY) return false;
    divisor->factors[divisor->count][0] = position[0];
    divisor->factors[divisor->count][1] = position[1];
    divisor->factors[divisor->count][2] = (float)order;
    ++divisor->count;
    return true;
}

static inline bool holomorphic_divisor_reduce_positions(
    struct holomorphic_divisor *divisor,
    int zero_count, const float (*zeros)[2],
    int pole_count, const float (*poles)[2],
    int remote_count, const float (*remote_positions)[2]
) {
    divisor->count = 0;
    if (zero_count < 0 || zero_count > 32 || pole_count < 0 || pole_count > 32
            || remote_count < 0 || remote_count > HOLOMORPHIC_REMOTE_POLE_COUNT
            || (zero_count > 0 && zeros == NULL)
            || (pole_count > 0 && poles == NULL)
            || (remote_count > 0 && remote_positions == NULL)) return false;
    for (int index = 0; index < zero_count; ++index)
        if (!holomorphic_divisor_insert(divisor, zeros[index], 1)) return false;
    for (int index = 0; index < pole_count; ++index)
        if (!holomorphic_divisor_insert(divisor, poles[index], -1)) return false;
    for (int index = 0; index < remote_count; ++index) {
        if (!holomorphic_divisor_insert(divisor, remote_positions[index], -1)) return false;
    }
    int kept = 0;
    for (int index = 0; index < divisor->count; ++index) {
        if (divisor->factors[index][2] == 0.0f) continue;
        for (int component = 0; component < 3; ++component)
            divisor->factors[kept][component] = divisor->factors[index][component];
        ++kept;
    }
    divisor->count = kept;
    return true;
}

/* Convenience entry point for numerical corpus consumers. The application
 * computes the same positions once and calls reduce_positions, so the marker
 * and mathematical divisor always consume one authoritative world-space set. */
static inline bool holomorphic_divisor_reduce(
    struct holomorphic_divisor *divisor,
    int zero_count, const float (*zeros)[2],
    int pole_count, const float (*poles)[2],
    int remote_count, double time
) {
    divisor->count = 0;
    if (remote_count < 0 || remote_count > HOLOMORPHIC_REMOTE_POLE_COUNT
            || !isfinite(time)) return false;
    float remote_positions[HOLOMORPHIC_REMOTE_POLE_COUNT][2];
    for (int index = 0; index < remote_count; ++index)
        if (!holomorphic_remote_pole(index, time, remote_positions[index])) return false;
    return holomorphic_divisor_reduce_positions(divisor, zero_count, zeros,
        pole_count, poles, remote_count, remote_positions);
}

#endif
