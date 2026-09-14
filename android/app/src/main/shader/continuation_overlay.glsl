// Handwritten presentation/interaction layer.
// The mathematical field is computed by generated_holomorphic_field() above.

uniform int u_placement_kind;

float circle_mask(vec2 point, vec2 center, float radius) {
    return 1.0 - smoothstep(radius - 1.25, radius + 1.25, length(point - center));
}

float ring_mask(vec2 point, vec2 center, float outer_radius, float inner_radius) {
    return max(
        0.0,
        circle_mask(point, center, outer_radius) -
        circle_mask(point, center, inner_radius)
    );
}

float line_mask(vec2 point, vec2 start, vec2 finish, float half_width) {
    vec2 segment = finish - start;
    float denominator = max(dot(segment, segment), 1.0e-6);
    float along = clamp(dot(point - start, segment) / denominator, 0.0, 1.0);
    float distance_to_line = length(point - (start + along * segment));
    return 1.0 - smoothstep(half_width - 1.0, half_width + 1.0, distance_to_line);
}

void main() {
    generated_holomorphic_field();

    float phase = _idris_fragColor.x;
    float log_modulus = _idris_fragColor.y;
    vec3 color = wegert_color_from_phase_log_modulus(phase, log_modulus);

    float pixel_radius = 0.42 * min(u_resolution.x, u_resolution.y) * u_zoom;

    // Interaction overlay: not part of the mathematical field.
    for (int index = 0; index < 32; ++index) {
        if (index >= u_zero_count) break;
        vec2 center = 0.5 * u_resolution + u_zero_positions[index] * pixel_radius;
        float mark = ring_mask(gl_FragCoord.xy, center, 9.0, 5.0);
        color = mix(color, vec3(0.04), mark);
        float highlight = ring_mask(gl_FragCoord.xy, center, 6.8, 5.5);
        color = mix(color, vec3(0.98), highlight);
    }

    for (int index = 0; index < 32; ++index) {
        if (index >= u_pole_count) break;
        vec2 center = 0.5 * u_resolution + u_pole_positions[index] * pixel_radius;
        float pole_mark = max(
            line_mask(
                gl_FragCoord.xy,
                center + vec2(-7.0, -7.0),
                center + vec2(7.0, 7.0),
                2.4
            ),
            line_mask(
                gl_FragCoord.xy,
                center + vec2(-7.0, 7.0),
                center + vec2(7.0, -7.0),
                2.4
            )
        );
        color = mix(color, vec3(0.04), pole_mark);
    }

    float placement_radius = clamp(0.048 * min(u_resolution.x, u_resolution.y), 26.0, 38.0);
    vec2 zero_center = vec2(placement_radius + 16.0, placement_radius + 16.0);
    vec2 pole_center = vec2(
        zero_center.x + 2.0 * placement_radius + 14.0,
        zero_center.y
    );

    bool zero_selected = u_placement_kind == 0;
    bool pole_selected = u_placement_kind == 1;

    float zero_disk = circle_mask(gl_FragCoord.xy, zero_center, placement_radius);
    color = mix(color, zero_selected ? vec3(0.94) : vec3(0.04), zero_disk * 0.86);
    vec3 zero_mark_color = zero_selected ? vec3(0.05) : vec3(0.96);
    float zero_mark = ring_mask(
        gl_FragCoord.xy,
        zero_center,
        placement_radius * 0.40,
        placement_radius * 0.25
    );
    color = mix(color, zero_mark_color, zero_mark);

    float pole_disk = circle_mask(gl_FragCoord.xy, pole_center, placement_radius);
    color = mix(color, pole_selected ? vec3(0.94) : vec3(0.04), pole_disk * 0.86);
    vec3 pole_mark_color = pole_selected ? vec3(0.05) : vec3(0.96);
    float pole_mark = max(
        line_mask(
            gl_FragCoord.xy,
            pole_center + vec2(-9.0, -9.0),
            pole_center + vec2(9.0, 9.0),
            2.2
        ),
        line_mask(
            gl_FragCoord.xy,
            pole_center + vec2(-9.0, 9.0),
            pole_center + vec2(9.0, -9.0),
            2.2
        )
    );
    color = mix(color, pole_mark_color, pole_mark);

    _idris_fragColor = vec4(color, 1.0);
}
