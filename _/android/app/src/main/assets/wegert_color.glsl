// Renderer-independent Wegert domain-coloring core.
//
// Consumers provide either a complex function value as vec2(real, imag), or
// an accumulated phase and logarithmic modulus.  This file deliberately has
// no #version, precision declaration, uniforms, camera assumptions, or
// zero/pole representation so it can be inserted into another GLSL shader.

const float WEGERT_TAU = 6.28318530717958647692;
const float WEGERT_LOG_10 = 2.30258509299404568402;

float wegert_positive_fract(float value) {
    return value - floor(value);
}

float wegert_srgb_component(float linear_value) {
    float value = max(linear_value, 0.0);
    if (value <= 0.0031308) {
        return 12.92 * value;
    }
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

// R's hcl() convention is polar CIE L*u*v*.  These constants use D65.
vec3 wegert_hcl_to_srgb(float hue_degrees, float chroma, float lightness) {
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
        wegert_srgb_component(linear_r),
        wegert_srgb_component(linear_g),
        wegert_srgb_component(linear_b)
    ), 0.0, 1.0);
}

vec3 wegert_color_from_phase_log_modulus(float phase, float log_modulus) {
    float hue_degrees = 360.0 * wegert_positive_fract(phase / WEGERT_TAU);
    float log_modulus_band = wegert_positive_fract(log_modulus / WEGERT_LOG_10);

    // Established Wegert constants: chroma 45; L = 66 + 4*band + 3*hue-band.
    float lightness = 66.0
        + 4.0 * log_modulus_band
        + 3.0 * wegert_positive_fract(hue_degrees / 100.0);

    return wegert_hcl_to_srgb(hue_degrees, 45.0, lightness);
}

vec3 wegert_color_complex(vec2 value) {
    float phase = atan(value.y, value.x);
    float log_modulus = log(max(length(value), 1.0e-12));
    return wegert_color_from_phase_log_modulus(phase, log_modulus);
}
