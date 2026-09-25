#version 300 es
precision highp float;
precision highp int;

#define MAX_FACTORS 64
#define MAX_CONTINUATION_STEPS 24
#define MAX_PERTURBATIONS 3

in vec2 v_ndc;
out vec4 frag_color;

uniform vec2 u_center;
uniform float u_half_height;
uniform float u_aspect;
uniform vec2 u_resolution;
uniform int u_zero_count;
uniform int u_pole_count;
uniform vec2 u_zeros[MAX_FACTORS];
uniform vec2 u_poles[MAX_FACTORS];
uniform int u_view_kind;
uniform int u_continuation_count;
uniform vec2 u_continuation_centers[MAX_CONTINUATION_STEPS];
uniform float u_continuation_radii[MAX_CONTINUATION_STEPS];
uniform int u_perturbation_count;
uniform vec2 u_perturbation_disk_centers[MAX_PERTURBATIONS];
uniform float u_perturbation_inverse_radii[MAX_PERTURBATIONS];
uniform vec2 u_perturbation_anchors[MAX_PERTURBATIONS];
uniform float u_perturbation_kernel_scales[MAX_PERTURBATIONS];
uniform float u_perturbation_phase_amplitudes[MAX_PERTURBATIONS];

const float TAU = 6.28318530717958647692;
const float LOG_10 = 2.30258509299404568402;

float positive_fract(float value) {
    return value - floor(value);
}

vec2 complex_multiply(vec2 left, vec2 right) {
    return vec2(
        left.x * right.x - left.y * right.y,
        left.x * right.y + left.y * right.x
    );
}

float srgb_component(float linear_value) {
    float value = max(linear_value, 0.0);
    if (value <= 0.0031308) {
        return 12.92 * value;
    }
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

// R's hcl() convention is polar CIE L*u*v*.  These constants use D65.
vec3 hcl_to_srgb(float hue_degrees, float chroma, float lightness) {
    float hue = radians(hue_degrees);
    float u_star = chroma * cos(hue);
    float v_star = chroma * sin(hue);

    const float white_u_prime = 0.19783982482140777;
    const float white_v_prime = 0.46833630293240974;

    float y = lightness > 8.0
        ? pow((lightness + 16.0) / 116.0, 3.0)
        : lightness / 903.2962962962963;

    float u_prime = u_star / (13.0 * lightness) + white_u_prime;
    float v_prime = v_star / (13.0 * lightness) + white_v_prime;

    float x = (9.0 * y * u_prime) / (4.0 * v_prime);
    float z = y * (12.0 - 3.0 * u_prime - 20.0 * v_prime) / (4.0 * v_prime);

    float linear_r =  3.2404542 * x - 1.5371385 * y - 0.4985314 * z;
    float linear_g = -0.9692660 * x + 1.8760108 * y + 0.0415560 * z;
    float linear_b =  0.0556434 * x - 0.2040259 * y + 1.0572252 * z;

    return clamp(vec3(
        srgb_component(linear_r),
        srgb_component(linear_g),
        srgb_component(linear_b)
    ), 0.0, 1.0);
}

float distance_to_segment(vec2 point, vec2 start, vec2 finish) {
    vec2 segment = finish - start;
    float length_squared = dot(segment, segment);
    if (length_squared < 1.0e-20) {
        return length(point - start);
    }
    float along = clamp(dot(point - start, segment) / length_squared, 0.0, 1.0);
    return length(point - (start + along * segment));
}

void main() {
    vec2 z = u_center + vec2(
        v_ndc.x * u_half_height * u_aspect,
        v_ndc.y * u_half_height
    );

    float phase = 0.0;
    float log_modulus = 0.0;

    for (int index = 0; index < MAX_FACTORS; ++index) {
        if (index >= u_zero_count) {
            break;
        }
        vec2 delta = z - u_zeros[index];
        phase += atan(delta.y, delta.x);
        log_modulus += log(max(length(delta), 1.0e-12));
    }

    for (int index = 0; index < MAX_FACTORS; ++index) {
        if (index >= u_pole_count) {
            break;
        }
        vec2 delta = z - u_poles[index];
        phase -= atan(delta.y, delta.x);
        log_modulus -= log(max(length(delta), 1.0e-12));
    }

    // Each ARM worker supplies one normalized Bergman-kernel perturbation.
    // If phi_a(a) = 1 and eta = i*alpha, then
    //     f_new = f * exp(eta * phi_a)
    // is holomorphic on the worker's containing disk and rotates the chosen
    // point by alpha without changing its modulus there.  Domain coloring only
    // needs Re(eta*phi_a) and Im(eta*phi_a), so no complex exponential is
    // evaluated per pixel.
    for (int index = 0; index < MAX_PERTURBATIONS; ++index) {
        if (index >= u_perturbation_count) {
            break;
        }

        vec2 normalized_z = (z - u_perturbation_disk_centers[index])
            * u_perturbation_inverse_radii[index];
        vec2 anchor = u_perturbation_anchors[index];

        // denominator = 1 - conj(anchor) * normalized_z
        vec2 denominator = vec2(
            1.0 - dot(anchor, normalized_z),
            normalized_z.x * anchor.y - normalized_z.y * anchor.x
        );
        float denominator_squared = max(dot(denominator, denominator), 1.0e-12);
        vec2 inverse_denominator = vec2(denominator.x, -denominator.y)
            / denominator_squared;
        vec2 kernel = complex_multiply(inverse_denominator, inverse_denominator)
            * u_perturbation_kernel_scales[index];

        float alpha = u_perturbation_phase_amplitudes[index];
        log_modulus -= alpha * kernel.y;
        phase += alpha * kernel.x;
    }

    float hue_degrees = 360.0 * positive_fract(phase / TAU);
    float log_modulus_band = positive_fract(log_modulus / LOG_10);

    // Established Wegert constants: chroma 45; L = 66 + 4*band + 3*hue-band.
    float lightness = 66.0
        + 4.0 * log_modulus_band
        + 3.0 * positive_fract(hue_degrees / 100.0);

    vec3 color = hcl_to_srgb(hue_degrees, 45.0, lightness);

    // Keep factors visible without replacing the phase portrait with UI chrome.
    float world_per_pixel = (2.0 * u_half_height) / max(u_resolution.y, 1.0);
    vec3 marker_dark = vec3(0.09411765);
    vec3 marker_light = vec3(0.96078431, 0.94901961, 0.92156863);

    if (u_view_kind == 1) {
        float revealed = 0.0;
        float boundary = 0.0;
        float path_line = 0.0;
        float center_mark = 0.0;

        for (int index = 0; index < MAX_CONTINUATION_STEPS; ++index) {
            if (index >= u_continuation_count) {
                break;
            }

            vec2 disc_center = u_continuation_centers[index];
            float disc_radius = u_continuation_radii[index];
            float distance_from_center = length(z - disc_center);
            if (disc_radius < 0.0) {
                revealed = 1.0;
            } else {
                float edge_width = 1.25 * world_per_pixel;
                revealed = max(
                    revealed,
                    1.0 - smoothstep(disc_radius - edge_width, disc_radius + edge_width, distance_from_center)
                );
                boundary = max(
                    boundary,
                    1.0 - smoothstep(
                        1.0 * world_per_pixel,
                        2.5 * world_per_pixel,
                        abs(distance_from_center - disc_radius)
                    )
                );
            }

            center_mark = max(
                center_mark,
                1.0 - smoothstep(3.0, 4.5, distance_from_center / world_per_pixel)
            );

            if (index > 0) {
                float segment_distance = distance_to_segment(
                    z,
                    u_continuation_centers[index - 1],
                    disc_center
                );
                path_line = max(
                    path_line,
                    1.0 - smoothstep(1.5, 3.0, segment_distance / world_per_pixel)
                );
            }
        }

        // The unrevealed area must not leak values of the rational function.
        // Use a low-contrast woven charcoal texture based only on screen pixels.
        float diagonal_a = 0.5 + 0.5 * cos(
            (gl_FragCoord.x + gl_FragCoord.y) * TAU / 28.0
        );
        float diagonal_b = 0.5 + 0.5 * cos(
            (gl_FragCoord.x - gl_FragCoord.y) * TAU / 28.0
        );
        float weave = 0.5 * (
            smoothstep(0.92, 1.0, diagonal_a) +
            smoothstep(0.92, 1.0, diagonal_b)
        );
        vec3 unrevealed = vec3(0.080, 0.086, 0.096)
            + weave * vec3(0.018, 0.020, 0.024);
        color = mix(unrevealed, color, revealed);
        color = mix(color, vec3(0.98, 0.95, 0.76), boundary * 0.86);
        color = mix(color, marker_dark, path_line);
        color = mix(color, marker_light, center_mark);
    }

    for (int index = 0; index < MAX_FACTORS; ++index) {
        if (index >= u_zero_count) {
            break;
        }
        float radius = length(z - u_zeros[index]) / world_per_pixel;
        if (radius < 6.0) {
            color = radius < 4.2 ? marker_light : marker_dark;
        }
    }

    for (int index = 0; index < MAX_FACTORS; ++index) {
        if (index >= u_pole_count) {
            break;
        }
        vec2 offset = (z - u_poles[index]) / world_per_pixel;
        float left_loop = abs(length(offset - vec2(-2.2, 0.0)) - 2.7);
        float right_loop = abs(length(offset - vec2(2.2, 0.0)) - 2.7);
        float infinity_mark = min(left_loop, right_loop);
        if (infinity_mark < 1.05) color = marker_dark;
    }

    frag_color = vec4(color, 1.0);
}
