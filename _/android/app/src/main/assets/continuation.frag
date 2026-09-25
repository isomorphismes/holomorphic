#version 300 es
precision highp float;
precision highp int;

#define MAX_ZEROS 32
#define LASSO_COEFFICIENT_COUNT 5

in vec2 v_ndc;
out vec4 frag_color;

uniform vec2 u_resolution;
uniform int u_zero_count;
uniform vec2 u_zero_preimages[MAX_ZEROS];
uniform vec2 u_zero_positions[MAX_ZEROS];
uniform vec2 u_lasso_coefficients[LASSO_COEFFICIENT_COUNT];
uniform float u_phase;
uniform int u_paused;

const float TAU = 6.28318530717958647692;
const float LOG_10 = 2.30258509299404568402;

vec2 complex_multiply(vec2 left, vec2 right) {
    return vec2(
        left.x * right.x - left.y * right.y,
        left.x * right.y + left.y * right.x
    );
}

vec2 complex_divide(vec2 numerator, vec2 denominator) {
    float scale = max(dot(denominator, denominator), 1.0e-10);
    return vec2(
        numerator.x * denominator.x + numerator.y * denominator.y,
        numerator.y * denominator.x - numerator.x * denominator.y
    ) / scale;
}

vec2 lasso_map_and_derivative(vec2 w, float amount, out vec2 derivative) {
    vec2 mapped = w;
    derivative = vec2(1.0, 0.0);
    vec2 power = w;

    for (int index = 0; index < LASSO_COEFFICIENT_COUNT; ++index) {
        float degree = float(index + 2);
        vec2 next_power = complex_multiply(power, w);
        vec2 coefficient = amount * u_lasso_coefficients[index];

        mapped += complex_multiply(coefficient, next_power);
        derivative += degree * complex_multiply(coefficient, power);
        power = next_power;
    }

    return mapped;
}

vec2 inverse_lasso(vec2 z) {
    vec2 w = z;

    for (int stage = 1; stage <= 4; ++stage) {
        float amount = 0.25 * float(stage);

        for (int iteration = 0; iteration < 2; ++iteration) {
            vec2 derivative;
            vec2 mapped = lasso_map_and_derivative(w, amount, derivative);
            vec2 correction = complex_divide(mapped - z, derivative);
            float correction_length = length(correction);
            if (correction_length > 0.55) {
                correction *= 0.55 / correction_length;
            }
            w -= correction;
        }
    }

    return w;
}

float positive_fract(float value) {
    return value - floor(value);
}

vec3 hue_rgb(float hue) {
    vec3 wave = abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0;
    return clamp(wave, 0.0, 1.0);
}

vec3 domain_color(float hue, float modulus_band) {
    vec3 saturated = hue_rgb(hue);
    float value = 0.72 + 0.16 * modulus_band + 0.04 * positive_fract(hue * 3.6);
    return value * mix(vec3(1.0), saturated, 0.68);
}

float circle_mask(vec2 point, vec2 center, float radius) {
    return 1.0 - smoothstep(radius - 1.25, radius + 1.25, length(point - center));
}

float line_mask(vec2 point, vec2 start, vec2 finish, float half_width) {
    vec2 segment = finish - start;
    float denominator = max(dot(segment, segment), 1.0e-6);
    float along = clamp(dot(point - start, segment) / denominator, 0.0, 1.0);
    float distance_to_line = length(point - (start + along * segment));
    return 1.0 - smoothstep(half_width - 1.0, half_width + 1.0, distance_to_line);
}

void main() {
    float pixel_radius = 0.42 * min(u_resolution.x, u_resolution.y);
    vec2 pixel = gl_FragCoord.xy - 0.5 * u_resolution;
    vec2 z = pixel / pixel_radius;

    vec2 w = inverse_lasso(z);
    float w_radius = length(w);

    // Multiply the Blaschke factors directly. This preserves the phase and
    // modulus while avoiding two atan() and two log() calls per zero, which
    // is substantially easier on older PowerVR Android drivers.
    vec2 phase_product = vec2(1.0, 0.0);
    float modulus_product = 1.0;

    for (int index = 0; index < MAX_ZEROS; ++index) {
        if (index >= u_zero_count) {
            break;
        }

        vec2 a = u_zero_preimages[index];
        vec2 numerator = w - a;
        vec2 denominator = vec2(
            1.0 - a.x * w.x - a.y * w.y,
            -a.x * w.y + a.y * w.x
        );
        vec2 factor = complex_divide(numerator, denominator);
        float factor_radius = max(length(factor), 1.0e-8);

        phase_product = complex_multiply(phase_product, factor / factor_radius);
        modulus_product *= clamp(factor_radius, 1.0e-4, 1.0e4);
        modulus_product = clamp(modulus_product, 1.0e-12, 1.0e12);
    }

    float phase = atan(phase_product.y, phase_product.x) + u_phase;
    float log_modulus = log(max(modulus_product, 1.0e-12));
    float hue = positive_fract(phase / TAU);
    float log_modulus_band = positive_fract(log_modulus / LOG_10);
    vec3 color = domain_color(hue, log_modulus_band);

    // psi maps |w|=1 to the deformable lasso. The accepted CPU-side
    // deformation still enforces the same injectivity and zero constraints.
    vec2 unit_w = w_radius > 1.0e-6 ? w / w_radius : vec2(1.0, 0.0);
    vec2 boundary_derivative;
    lasso_map_and_derivative(unit_w, 1.0, boundary_derivative);
    float edge_distance_pixels =
        abs(w_radius - 1.0) * max(length(boundary_derivative), 0.15) * pixel_radius;
    float boundary = 1.0 - smoothstep(1.2, 3.2, edge_distance_pixels);
    color = mix(color, vec3(0.97, 0.97, 0.94), boundary);

    for (int index = 0; index < MAX_ZEROS; ++index) {
        if (index >= u_zero_count) {
            break;
        }
        float zero_distance_pixels = length(z - u_zero_positions[index]) * pixel_radius;
        float outer = 1.0 - smoothstep(7.0, 9.0, zero_distance_pixels);
        float inner = 1.0 - smoothstep(3.0, 4.5, zero_distance_pixels);
        color = mix(color, vec3(0.04), outer);
        color = mix(color, vec3(0.98), inner);
    }

    // Keep app controls clear of Android's right-edge navigation strip.
    float control_radius = clamp(0.052 * min(u_resolution.x, u_resolution.y), 28.0, 42.0);
    vec2 pause_center = vec2(
        control_radius + 16.0,
        u_resolution.y - control_radius - 16.0
    );
    vec2 close_center = vec2(
        u_resolution.x - 4.0 * control_radius - 16.0,
        u_resolution.y - control_radius - 16.0
    );
    float pause_disk = circle_mask(gl_FragCoord.xy, pause_center, control_radius);
    float close_disk = circle_mask(gl_FragCoord.xy, close_center, control_radius);
    color = mix(color, vec3(0.04), max(pause_disk, close_disk) * 0.78);

    float mark = 0.0;
    if (u_paused == 0) {
        mark = max(
            line_mask(
                gl_FragCoord.xy,
                pause_center + vec2(-8.0, -11.0),
                pause_center + vec2(-8.0, 11.0),
                2.2
            ),
            line_mask(
                gl_FragCoord.xy,
                pause_center + vec2(8.0, -11.0),
                pause_center + vec2(8.0, 11.0),
                2.2
            )
        );
    } else {
        vec2 left = pause_center + vec2(-8.0, -12.0);
        vec2 top = pause_center + vec2(11.0, 0.0);
        vec2 bottom = pause_center + vec2(-8.0, 12.0);
        mark = max(
            line_mask(gl_FragCoord.xy, left, top, 2.2),
            line_mask(gl_FragCoord.xy, top, bottom, 2.2)
        );
    }

    mark = max(
        mark,
        line_mask(
            gl_FragCoord.xy,
            close_center + vec2(-10.0, -10.0),
            close_center + vec2(10.0, 10.0),
            2.2
        )
    );
    mark = max(
        mark,
        line_mask(
            gl_FragCoord.xy,
            close_center + vec2(-10.0, 10.0),
            close_center + vec2(10.0, -10.0),
            2.2
        )
    );
    color = mix(color, vec3(0.98), mark);

    frag_color = vec4(color, 1.0);
}
