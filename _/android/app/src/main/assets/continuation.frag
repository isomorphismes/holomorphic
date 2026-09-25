#version 300 es
precision highp float;
precision highp int;

in vec2 v_ndc;
out vec4 frag_color;

uniform vec2 u_center;
uniform float u_half_height;
uniform float u_aspect;
uniform vec2 u_resolution;

const float TWO_PI = 6.28318530717958647692;
const float LOG_10 = 2.30258509299404568402;
const int J_Q_TERMS = 11;

// j(q) = q^-1 + 744 + sum_(n>=1) c_n q^n.
// These exact integer coefficients are generated on the cas-precompute branch.
const float J_COEFFICIENTS[J_Q_TERMS] = float[J_Q_TERMS](
    196884.0,
    21493760.0,
    864299970.0,
    20245856256.0,
    333202640600.0,
    4252023300096.0,
    44656994071935.0,
    401490886656000.0,
    3176440229784420.0,
    22567393309593600.0,
    146211911499519294.0
);

vec2 complex_multiply(vec2 left, vec2 right) {
    return vec2(
        left.x * right.x - left.y * right.y,
        left.x * right.y + left.y * right.x
    );
}

vec2 complex_divide(vec2 numerator, vec2 denominator) {
    float norm_squared = dot(denominator, denominator);
    return vec2(
        numerator.x * denominator.x + numerator.y * denominator.y,
        numerator.y * denominator.x - numerator.x * denominator.y
    ) / norm_squared;
}

vec2 reduce_to_fundamental_domain(vec2 tau) {
    // j is invariant under tau -> tau + 1 and tau -> -1/tau. Reduction makes
    // |q| <= exp(-pi*sqrt(3)) ~= 0.00434, so 11 positive q powers suffice.
    for (int step = 0; step < 16; ++step) {
        tau.x -= floor(tau.x + 0.5);
        float norm_squared = dot(tau, tau);
        if (norm_squared < 1.0) {
            tau = vec2(-tau.x, tau.y) / norm_squared;
            continue;
        }
        break;
    }
    return tau;
}

vec2 modular_j_from_reduced_tau(vec2 reduced_tau) {
    float q_modulus = exp(-TWO_PI * reduced_tau.y);
    float q_phase = TWO_PI * reduced_tau.x;
    vec2 q = q_modulus * vec2(cos(q_phase), sin(q_phase));

    vec2 value = complex_divide(vec2(1.0, 0.0), q) + vec2(744.0, 0.0);
    vec2 q_power = q;
    for (int index = 0; index < J_Q_TERMS; ++index) {
        value += J_COEFFICIENTS[index] * q_power;
        q_power = complex_multiply(q_power, q);
    }
    return value;
}

float positive_fract(float value) {
    return value - floor(value);
}

float srgb_component(float linear_value) {
    float value = max(linear_value, 0.0);
    if (value <= 0.0031308) {
        return 12.92 * value;
    }
    return 1.055 * pow(value, 1.0 / 2.4) - 0.055;
}

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

void main() {
    vec2 tau = u_center + vec2(
        v_ndc.x * u_half_height * u_aspect,
        v_ndc.y * u_half_height
    );

    // The classical modular-form domain is the upper half-plane.
    if (tau.y <= 0.0) {
        float weave = 0.5 + 0.5 * cos((gl_FragCoord.x + gl_FragCoord.y) * TWO_PI / 28.0);
        frag_color = vec4(vec3(0.075 + 0.015 * smoothstep(0.92, 1.0, weave)), 1.0);
        return;
    }

    vec2 reduced_tau = reduce_to_fundamental_domain(tau);
    float phase;
    float log_modulus;
    if (reduced_tau.y > 8.0) {
        // At a cusp j = q^-1 + O(1). Stay in log-polar form instead of
        // overflowing highp float by constructing q^-1.
        phase = -TWO_PI * reduced_tau.x;
        log_modulus = TWO_PI * reduced_tau.y;
    } else {
        vec2 value = modular_j_from_reduced_tau(reduced_tau);
        phase = atan(value.y, value.x);
        log_modulus = log(max(length(value), 1.0e-20));
    }
    float hue_degrees = 360.0 * positive_fract(phase / TWO_PI);
    float log_modulus_band = positive_fract(log_modulus / LOG_10);
    float lightness = 66.0
        + 4.0 * log_modulus_band
        + 3.0 * positive_fract(hue_degrees / 100.0);

    frag_color = vec4(hcl_to_srgb(hue_degrees, 45.0, lightness), 1.0);
}
