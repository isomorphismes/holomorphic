#ifndef WEGERT_POLYNOMIAL_OVERLAY_H
#define WEGERT_POLYNOMIAL_OVERLAY_H

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct overlay_complex {
    double real;
    double imag;
};

static const char *POLYNOMIAL_OVERLAY_FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec2 u_overlay_origin;\n"
    "uniform vec2 u_overlay_size;\n"
    "uniform sampler2D u_overlay;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec2 screen = vec2(gl_FragCoord.x, u_resolution.y - gl_FragCoord.y) - u_overlay_origin;\n"
    "    if (screen.x < 0.0 || screen.y < 0.0 || screen.x >= u_overlay_size.x || screen.y >= u_overlay_size.y) {\n"
    "        discard;\n"
    "    }\n"
    "    vec2 uv = (screen + vec2(0.5)) / u_overlay_size;\n"
    "    frag_color = texture(u_overlay, uv);\n"
    "}\n";

static bool overlay_near_zero(double value) {
    return fabs(value) < 1.0e-6;
}

static bool overlay_near_one(double value) {
    return fabs(value - 1.0) < 1.0e-6;
}

static void overlay_append(char *buffer, size_t capacity, size_t *used, const char *format, ...) {
    if (*used + 1u >= capacity) {
        return;
    }

    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);

    if (written < 0) {
        return;
    }

    size_t remaining = capacity - *used;
    if ((size_t)written >= remaining) {
        *used = capacity - 1u;
    } else {
        *used += (size_t)written;
    }
}

static void overlay_format_number(double value, char *output, size_t capacity) {
    double rounded = round(value);
    if (overlay_near_zero(rounded)) {
        snprintf(output, capacity, "0");
        return;
    }
    snprintf(output, capacity, "%.0f", rounded);
}

static void overlay_expand_roots(
    const float roots[MAX_FACTORS][2],
    int root_count,
    struct overlay_complex coefficients[MAX_FACTORS + 1]
) {
    for (int index = 0; index <= MAX_FACTORS; ++index) {
        coefficients[index].real = 0.0;
        coefficients[index].imag = 0.0;
    }
    coefficients[0].real = 1.0;

    int degree = 0;
    for (int root_index = 0; root_index < root_count; ++root_index) {
        struct overlay_complex next[MAX_FACTORS + 1] = {{0.0, 0.0}};
        double root_real = roots[root_index][0];
        double root_imag = roots[root_index][1];

        for (int power = 0; power <= degree; ++power) {
            double coefficient_real = coefficients[power].real;
            double coefficient_imag = coefficients[power].imag;

            next[power + 1].real += coefficient_real;
            next[power + 1].imag += coefficient_imag;

            next[power].real += -root_real * coefficient_real + root_imag * coefficient_imag;
            next[power].imag += -root_real * coefficient_imag - root_imag * coefficient_real;
        }

        degree += 1;
        for (int power = 0; power <= degree; ++power) {
            coefficients[power] = next[power];
        }
    }
}

static void overlay_append_complex_magnitude(
    char *output,
    size_t capacity,
    size_t *used,
    struct overlay_complex value
) {
    bool has_real = !overlay_near_zero(value.real);
    bool has_imag = !overlay_near_zero(value.imag);
    char number[64];

    if (has_real && has_imag) {
        overlay_format_number(value.real, number, sizeof(number));
        overlay_append(output, capacity, used, "(%s", number);

        double imag_magnitude = fabs(value.imag);
        overlay_append(output, capacity, used, value.imag < 0.0 ? "-" : "+");
        if (!overlay_near_one(imag_magnitude)) {
            overlay_format_number(imag_magnitude, number, sizeof(number));
            overlay_append(output, capacity, used, "%s", number);
        }
        overlay_append(output, capacity, used, "i)");
        return;
    }

    if (has_imag) {
        double imag_magnitude = fabs(value.imag);
        if (!overlay_near_one(imag_magnitude)) {
            overlay_format_number(imag_magnitude, number, sizeof(number));
            overlay_append(output, capacity, used, "%s", number);
        }
        overlay_append(output, capacity, used, "i");
        return;
    }

    overlay_format_number(fabs(value.real), number, sizeof(number));
    overlay_append(output, capacity, used, "%s", number);
}

static void overlay_format_polynomial(
    const struct overlay_complex coefficients[MAX_FACTORS + 1],
    int degree,
    char *output,
    size_t capacity
) {
    size_t used = 0u;
    output[0] = '\0';
    bool first_term = true;

    for (int power = degree; power >= 0; --power) {
        struct overlay_complex coefficient = coefficients[power];
        coefficient.real = round(coefficient.real);
        coefficient.imag = round(coefficient.imag);
        if (overlay_near_zero(coefficient.real) && overlay_near_zero(coefficient.imag)) {
            continue;
        }

        if (overlay_near_zero(coefficient.imag)) {
            bool negative = coefficient.real < 0.0;
            double magnitude = fabs(coefficient.real);

            if (first_term) {
                if (negative) overlay_append(output, capacity, &used, "-");
            } else {
                overlay_append(output, capacity, &used, negative ? " - " : " + ");
            }

            if (power == 0 || !overlay_near_one(magnitude)) {
                char number[64];
                overlay_format_number(magnitude, number, sizeof(number));
                overlay_append(output, capacity, &used, "%s", number);
            }
        } else {
            bool negative = coefficient.real < -1.0e-6 ||
                (overlay_near_zero(coefficient.real) && coefficient.imag < 0.0);
            struct overlay_complex magnitude = coefficient;
            if (negative) {
                magnitude.real = -magnitude.real;
                magnitude.imag = -magnitude.imag;
            }

            if (first_term) {
                if (negative) overlay_append(output, capacity, &used, "-");
            } else {
                overlay_append(output, capacity, &used, negative ? " - " : " + ");
            }
            overlay_append_complex_magnitude(output, capacity, &used, magnitude);
        }

        if (power > 0) {
            overlay_append(output, capacity, &used, "z");
            if (power > 1) {
                // '^' is an internal layout marker. It is never drawn; the
                // following digits are rasterized smaller and raised.
                overlay_append(output, capacity, &used, "^%d", power);
            }
        }

        first_term = false;
    }

    if (first_term) {
        overlay_append(output, capacity, &used, "0");
    }
}

static void overlay_append_factor(
    char *output,
    size_t capacity,
    size_t *used,
    const float root[2]
) {
    double real = round((double)root[0]);
    double imag = round((double)root[1]);
    char number[64];

    overlay_append(output, capacity, used, "(z");

    if (!overlay_near_zero(real)) {
        overlay_format_number(fabs(real), number, sizeof(number));
        overlay_append(output, capacity, used, real > 0.0 ? "-%s" : "+%s", number);
    }

    if (!overlay_near_zero(imag)) {
        double magnitude = fabs(imag);
        overlay_append(output, capacity, used, imag > 0.0 ? "-" : "+");
        if (!overlay_near_one(magnitude)) {
            overlay_format_number(magnitude, number, sizeof(number));
            overlay_append(output, capacity, used, "%s", number);
        }
        overlay_append(output, capacity, used, "i");
    }

    overlay_append(output, capacity, used, ")");
}

static void polynomial_overlay_format_function(const struct engine *engine, char *output, size_t capacity) {
    struct overlay_complex numerator_coefficients[MAX_FACTORS + 1];
    char numerator[2048];

    overlay_expand_roots(engine->zeros, engine->zero_count, numerator_coefficients);
    overlay_format_polynomial(numerator_coefficients, engine->zero_count, numerator, sizeof(numerator));

    if (engine->pole_count == 0) {
        snprintf(output, capacity, "%s", numerator);
        return;
    }

    size_t used = 0u;
    output[0] = '\0';
    overlay_append(output, capacity, &used, "(%s)", numerator);
    for (int index = 0; index < engine->pole_count; ++index) {
        overlay_append(output, capacity, &used, " ÷");
        overlay_append_factor(output, capacity, &used, engine->poles[index]);
    }
}

static int overlay_max_digit_run(const char *text) {
    int maximum = 0;
    int current = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor >= '0' && *cursor <= '9') {
            current += 1;
            if (current > maximum) maximum = current;
        } else {
            current = 0;
        }
    }
    return maximum;
}

static int overlay_decode_glyph(const char *text, uint32_t *glyph) {
    const unsigned char first = (unsigned char)text[0];
    const unsigned char second = (unsigned char)text[1];
    if (first == 0xc3u && second == 0xb7u) {
        *glyph = 0x00f7u;
        return 2;
    }

    *glyph = (uint32_t)first;
    return 1;
}

static uint8_t overlay_glyph_row(uint32_t character, int row) {
    static const uint8_t digits[10][7] = {
        {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e},
        {0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f},
        {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
        {0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e},
        {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02},
        {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e},
        {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e},
        {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e},
        {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}
    };

    if (character >= '0' && character <= '9') {
        return digits[character - '0'][row];
    }

    switch (character) {
        case 'a': { static const uint8_t glyph[7] = {0x00,0x00,0x0e,0x01,0x0f,0x11,0x0f}; return glyph[row]; }
        case 'c': { static const uint8_t glyph[7] = {0x00,0x00,0x0e,0x10,0x10,0x11,0x0e}; return glyph[row]; }
        case 'd': { static const uint8_t glyph[7] = {0x01,0x01,0x0f,0x11,0x11,0x11,0x0f}; return glyph[row]; }
        case 'e': { static const uint8_t glyph[7] = {0x00,0x00,0x0e,0x11,0x1f,0x10,0x0f}; return glyph[row]; }
        case 'f': { static const uint8_t glyph[7] = {0x06,0x08,0x08,0x1e,0x08,0x08,0x08}; return glyph[row]; }
        case 'g': { static const uint8_t glyph[7] = {0x00,0x00,0x0f,0x11,0x0f,0x01,0x0e}; return glyph[row]; }
        case 'h': { static const uint8_t glyph[7] = {0x10,0x10,0x1e,0x11,0x11,0x11,0x11}; return glyph[row]; }
        case 'i': { static const uint8_t glyph[7] = {0x04,0x00,0x0c,0x04,0x04,0x04,0x0e}; return glyph[row]; }
        case 'l': { static const uint8_t glyph[7] = {0x0c,0x04,0x04,0x04,0x04,0x04,0x0e}; return glyph[row]; }
        case 'n': { static const uint8_t glyph[7] = {0x00,0x00,0x1e,0x11,0x11,0x11,0x11}; return glyph[row]; }
        case 'o': { static const uint8_t glyph[7] = {0x00,0x00,0x0e,0x11,0x11,0x11,0x0e}; return glyph[row]; }
        case 'p': { static const uint8_t glyph[7] = {0x00,0x00,0x1e,0x11,0x1e,0x10,0x10}; return glyph[row]; }
        case 'r': { static const uint8_t glyph[7] = {0x00,0x00,0x16,0x19,0x10,0x10,0x10}; return glyph[row]; }
        case 's': { static const uint8_t glyph[7] = {0x00,0x00,0x0f,0x10,0x0e,0x01,0x1e}; return glyph[row]; }
        case 't': { static const uint8_t glyph[7] = {0x08,0x08,0x1e,0x08,0x08,0x09,0x06}; return glyph[row]; }
        case 'u': { static const uint8_t glyph[7] = {0x00,0x00,0x11,0x11,0x11,0x13,0x0d}; return glyph[row]; }
        case 'v': { static const uint8_t glyph[7] = {0x00,0x00,0x11,0x11,0x11,0x0a,0x04}; return glyph[row]; }
        case 'w': { static const uint8_t glyph[7] = {0x00,0x00,0x11,0x11,0x15,0x15,0x0a}; return glyph[row]; }
        case 'x': { static const uint8_t glyph[7] = {0x00,0x00,0x11,0x0a,0x04,0x0a,0x11}; return glyph[row]; }
        case 'y': { static const uint8_t glyph[7] = {0x00,0x00,0x11,0x11,0x0f,0x01,0x0e}; return glyph[row]; }
        case 'z': { static const uint8_t glyph[7] = {0x00,0x00,0x1f,0x02,0x04,0x08,0x1f}; return glyph[row]; }
        case '+': { static const uint8_t glyph[7] = {0x00,0x04,0x04,0x1f,0x04,0x04,0x00}; return glyph[row]; }
        case '-': { static const uint8_t glyph[7] = {0x00,0x00,0x00,0x1f,0x00,0x00,0x00}; return glyph[row]; }
        case '=': { static const uint8_t glyph[7] = {0x00,0x00,0x1f,0x00,0x1f,0x00,0x00}; return glyph[row]; }
        case '(': { static const uint8_t glyph[7] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; return glyph[row]; }
        case ')': { static const uint8_t glyph[7] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; return glyph[row]; }
        case '.': { static const uint8_t glyph[7] = {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c}; return glyph[row]; }
        case ':': { static const uint8_t glyph[7] = {0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00}; return glyph[row]; }
        case '^': { static const uint8_t glyph[7] = {0x04,0x0a,0x11,0x00,0x00,0x00,0x00}; return glyph[row]; }
        case '/': { static const uint8_t glyph[7] = {0x01,0x02,0x04,0x04,0x08,0x10,0x00}; return glyph[row]; }
        case 0x00f7u: { static const uint8_t glyph[7] = {0x04,0x00,0x00,0x1f,0x00,0x00,0x04}; return glyph[row]; }
        default: return 0x00;
    }
}

static void overlay_set_pixel(
    uint8_t *pixels,
    int width,
    int height,
    int x,
    int y,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha
) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    size_t offset = ((size_t)y * (size_t)width + (size_t)x) * 4u;
    pixels[offset + 0u] = red;
    pixels[offset + 1u] = green;
    pixels[offset + 2u] = blue;
    pixels[offset + 3u] = alpha;
}

static void overlay_draw_glyph(
    uint8_t *pixels,
    int width,
    int height,
    int x,
    int y,
    int scale,
    uint32_t character
) {
    for (int row = 0; row < 7; ++row) {
        uint8_t bits = overlay_glyph_row(character, row);
        for (int column = 0; column < 5; ++column) {
            if ((bits & (1u << (4 - column))) == 0u) {
                continue;
            }
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    overlay_set_pixel(
                        pixels,
                        width,
                        height,
                        x + column * scale + dx,
                        y + row * scale + dy,
                        248,
                        244,
                        232,
                        255
                    );
                }
            }
        }
    }
}

static int overlay_measure_wrapped_lines(const char *text, int max_columns) {
    int lines = 1;
    int column = 0;
    const char *cursor = text;

    while (*cursor != '\0') {
        while (*cursor == ' ') cursor += 1;
        if (*cursor == '\0') break;

        int word_bytes = 0;
        int word_columns = 0;
        while (cursor[word_bytes] != '\0' && cursor[word_bytes] != ' ') {
            uint32_t glyph = 0u;
            word_bytes += overlay_decode_glyph(cursor + word_bytes, &glyph);
            word_columns += 1;
        }

        if (column == 0) {
            column = word_columns;
        } else if (column + 1 + word_columns <= max_columns) {
            column += 1 + word_columns;
        } else {
            lines += 1;
            column = word_columns;
        }
        cursor += word_bytes;
    }

    return lines;
}

static void overlay_draw_wrapped_text(
    uint8_t *pixels,
    int width,
    int height,
    const char *text,
    int scale,
    int padding,
    int max_columns
) {
    int character_advance = 6 * scale;
    int line_advance = 9 * scale;
    int x = padding;
    int y = padding;
    int column = 0;
    const char *cursor = text;

    while (*cursor != '\0') {
        while (*cursor == ' ') cursor += 1;
        if (*cursor == '\0') break;

        int word_bytes = 0;
        int word_columns = 0;
        while (cursor[word_bytes] != '\0' && cursor[word_bytes] != ' ') {
            uint32_t glyph = 0u;
            word_bytes += overlay_decode_glyph(cursor + word_bytes, &glyph);
            word_columns += 1;
        }

        if (column != 0 && column + 1 + word_columns <= max_columns) {
            x += character_advance;
            column += 1;
        } else if (column != 0) {
            x = padding;
            y += line_advance;
            column = 0;
        }

        int index = 0;
        while (index < word_bytes) {
            if (cursor[index] == '^') {
                int superscript_scale = scale > 1 ? scale - 1 : 1;
                int superscript_y = y - 2 * scale;
                column += 1;
                index += 1;
                while (
                    index < word_bytes &&
                    cursor[index] >= '0' &&
                    cursor[index] <= '9'
                ) {
                    overlay_draw_glyph(
                        pixels,
                        width,
                        height,
                        x,
                        superscript_y,
                        superscript_scale,
                        (uint32_t)(unsigned char)cursor[index]
                    );
                    x += 6 * superscript_scale;
                    column += 1;
                    index += 1;
                }
                continue;
            }

            uint32_t glyph = 0u;
            int glyph_bytes = overlay_decode_glyph(cursor + index, &glyph);
            overlay_draw_glyph(pixels, width, height, x, y, scale, glyph);
            x += character_advance;
            column += 1;
            index += glyph_bytes;
        }

        cursor += word_bytes;
    }
}

static void overlay_configure_texture(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static int overlay_density(const struct engine *engine) {
    int density = AConfiguration_getDensity(engine->app->config);
    if (density < 72 || density > 1000) density = 160;
    return density;
}

static float clear_button_margin(const struct engine *engine) {
    return (float)((56 * overlay_density(engine) + 159) / 160);
}

static void clear_button_origin(const struct engine *engine, float *x, float *y) {
    float margin = clear_button_margin(engine);
    *x = (float)(engine->width - engine->clear_button_width) - margin;
    *y = (float)(engine->height - engine->clear_button_height) - margin;
}

static void view_button_origin(const struct engine *engine, float *x, float *y) {
    float margin = clear_button_margin(engine);
    *x = 0.5f * (float)(engine->width - engine->view_button_width);
    *y = (float)(engine->height - engine->view_button_height) - margin;
}

static bool polynomial_overlay_initialize(struct engine *engine) {
    if (engine->overlay_program != 0) {
        return true;
    }
    if (engine->overlay_unavailable) {
        return false;
    }

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, POLYNOMIAL_OVERLAY_FRAGMENT_SHADER);
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        engine->overlay_unavailable = true;
        return false;
    }

    engine->overlay_program = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (engine->overlay_program == 0) {
        engine->overlay_unavailable = true;
        return false;
    }

    engine->overlay_resolution_location = glGetUniformLocation(engine->overlay_program, "u_resolution");
    engine->overlay_origin_location = glGetUniformLocation(engine->overlay_program, "u_overlay_origin");
    engine->overlay_size_location = glGetUniformLocation(engine->overlay_program, "u_overlay_size");
    engine->overlay_sampler_location = glGetUniformLocation(engine->overlay_program, "u_overlay");

    glGenTextures(1, &engine->overlay_texture);
    overlay_configure_texture(engine->overlay_texture);
    glGenTextures(1, &engine->clear_button_texture);
    overlay_configure_texture(engine->clear_button_texture);
    glGenTextures(1, &engine->view_button_texture);
    overlay_configure_texture(engine->view_button_texture);
    return true;
}

static bool overlay_rebuild_button_texture(
    struct engine *engine,
    GLuint texture,
    const char *label,
    int *output_width,
    int *output_height
) {
    int density = overlay_density(engine);

    int scale = (density + 40) / 80;
    if (scale < 2) scale = 2;
    if (scale > 6) scale = 6;

    int label_characters = 0;
    while (label[label_characters] != '\0') label_characters += 1;
    int label_width = (6 * label_characters - 1) * scale;
    int minimum_height = (48 * density + 159) / 160;
    int horizontal_padding = (16 * density + 159) / 160;
    int width = label_width + 2 * horizontal_padding;
    int height = minimum_height;
    if (height < 11 * scale) height = 11 * scale;

    size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *pixels = malloc(pixel_count * 4u);
    if (pixels == NULL) {
        return false;
    }

    for (size_t index = 0; index < pixel_count; ++index) {
        pixels[index * 4u + 0u] = 18;
        pixels[index * 4u + 1u] = 18;
        pixels[index * 4u + 2u] = 18;
        pixels[index * 4u + 3u] = 230;
    }

    int border = scale > 2 ? 2 : 1;
    for (int x = 0; x < width; ++x) {
        for (int thickness = 0; thickness < border; ++thickness) {
            overlay_set_pixel(pixels, width, height, x, thickness, 238, 235, 224, 255);
            overlay_set_pixel(pixels, width, height, x, height - 1 - thickness, 238, 235, 224, 255);
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int thickness = 0; thickness < border; ++thickness) {
            overlay_set_pixel(pixels, width, height, thickness, y, 238, 235, 224, 255);
            overlay_set_pixel(pixels, width, height, width - 1 - thickness, y, 238, 235, 224, 255);
        }
    }

    int x = (width - label_width) / 2;
    int y = (height - 7 * scale) / 2;
    for (int index = 0; label[index] != '\0'; ++index) {
        overlay_draw_glyph(
            pixels,
            width,
            height,
            x,
            y,
            scale,
            (uint32_t)(unsigned char)label[index]
        );
        x += 6 * scale;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(pixels);

    *output_width = width;
    *output_height = height;
    return true;
}

static bool clear_button_rebuild_texture(struct engine *engine) {
    return overlay_rebuild_button_texture(
        engine,
        engine->clear_button_texture,
        "clear",
        &engine->clear_button_width,
        &engine->clear_button_height
    );
}

static bool view_button_rebuild_texture(struct engine *engine) {
    const char *label = engine->view_kind == VIEW_CONTINUATION
        ? "edit O / X"
        : "convergence discs";
    bool rebuilt = overlay_rebuild_button_texture(
        engine,
        engine->view_button_texture,
        label,
        &engine->view_button_width,
        &engine->view_button_height
    );
    if (rebuilt) {
        float left = 0.0f;
        float top = 0.0f;
        view_button_origin(engine, &left, &top);
        LOGI(
            "view control center: %d %d",
            (int)(left + 0.5f * (float)engine->view_button_width),
            (int)(top + 0.5f * (float)engine->view_button_height)
        );
        engine->view_button_dirty = false;
    }
    return rebuilt;
}

static bool polynomial_overlay_rebuild_texture(struct engine *engine) {
    if (engine->width <= 64 || engine->height <= 64) {
        return false;
    }

    char text[4096];
    polynomial_overlay_format_function(engine, text, sizeof(text));

    int width = engine->width - 32;
    if (width > 1280) width = 1280;
    int scale = width >= 900 ? 3 : 2;
    int max_number_digits = overlay_max_digit_run(text);
    if (max_number_digits >= 4 && scale > 2) scale = 2;
    if (max_number_digits >= 7) scale = 1;
    int padding = 4 * scale;
    int character_advance = 6 * scale;
    int line_advance = 9 * scale;
    int max_columns = (width - 2 * padding) / character_advance;
    if (max_columns < 12) max_columns = 12;

    int lines = overlay_measure_wrapped_lines(text, max_columns);
    int height = 2 * padding + 7 * scale + (lines - 1) * line_advance;
    if (height > engine->height - 32) {
        height = engine->height - 32;
    }

    size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *pixels = malloc(pixel_count * 4u);
    if (pixels == NULL) {
        return false;
    }

    for (size_t index = 0; index < pixel_count; ++index) {
        pixels[index * 4u + 0u] = 18;
        pixels[index * 4u + 1u] = 18;
        pixels[index * 4u + 2u] = 18;
        pixels[index * 4u + 3u] = 214;
    }

    for (int x = 0; x < width; ++x) {
        for (int thickness = 0; thickness < 2; ++thickness) {
            overlay_set_pixel(pixels, width, height, x, thickness, 238, 235, 224, 240);
            overlay_set_pixel(pixels, width, height, x, height - 1 - thickness, 238, 235, 224, 240);
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int thickness = 0; thickness < 2; ++thickness) {
            overlay_set_pixel(pixels, width, height, thickness, y, 238, 235, 224, 240);
            overlay_set_pixel(pixels, width, height, width - 1 - thickness, y, 238, 235, 224, 240);
        }
    }

    overlay_draw_wrapped_text(pixels, width, height, text, scale, padding, max_columns);

    glBindTexture(GL_TEXTURE_2D, engine->overlay_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(pixels);

    engine->overlay_width = width;
    engine->overlay_height = height;
    engine->overlay_dirty = false;
    return true;
}

static void overlay_draw_texture(
    struct engine *engine,
    GLuint texture,
    int width,
    int height,
    float x,
    float y
) {
    glUniform2f(engine->overlay_origin_location, x, y);
    glUniform2f(engine->overlay_size_location, (float)width, (float)height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(engine->overlay_sampler_location, 0);
    glBindVertexArray(engine->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void polynomial_overlay_draw(struct engine *engine) {
    if (!polynomial_overlay_initialize(engine)) {
        return;
    }
    if (
        engine->view_kind == VIEW_WHOLE_PORTRAIT &&
        engine->overlay_dirty &&
        !polynomial_overlay_rebuild_texture(engine)
    ) {
        return;
    }
    if (
        (engine->clear_button_width <= 0 || engine->clear_button_height <= 0) &&
        !clear_button_rebuild_texture(engine)
    ) {
        return;
    }
    if (
        (engine->view_button_width <= 0 || engine->view_button_height <= 0 ||
         engine->view_button_dirty) &&
        !view_button_rebuild_texture(engine)
    ) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(engine->overlay_program);
    glUniform2f(engine->overlay_resolution_location, (float)engine->width, (float)engine->height);

    float clear_button_x = 0.0f;
    float clear_button_y = 0.0f;
    clear_button_origin(engine, &clear_button_x, &clear_button_y);
    float view_button_x = 0.0f;
    float view_button_y = 0.0f;
    view_button_origin(engine, &view_button_x, &view_button_y);

    if (engine->view_kind == VIEW_WHOLE_PORTRAIT) {
        overlay_draw_texture(
            engine,
            engine->overlay_texture,
            engine->overlay_width,
            engine->overlay_height,
            16.0f,
            16.0f
        );
    }
    overlay_draw_texture(
        engine,
        engine->clear_button_texture,
        engine->clear_button_width,
        engine->clear_button_height,
        clear_button_x,
        clear_button_y
    );
    overlay_draw_texture(
        engine,
        engine->view_button_texture,
        engine->view_button_width,
        engine->view_button_height,
        view_button_x,
        view_button_y
    );

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

static void polynomial_overlay_destroy(struct engine *engine) {
    if (engine->overlay_texture != 0) {
        glDeleteTextures(1, &engine->overlay_texture);
        engine->overlay_texture = 0;
    }
    if (engine->clear_button_texture != 0) {
        glDeleteTextures(1, &engine->clear_button_texture);
        engine->clear_button_texture = 0;
    }
    if (engine->view_button_texture != 0) {
        glDeleteTextures(1, &engine->view_button_texture);
        engine->view_button_texture = 0;
    }
    if (engine->overlay_program != 0) {
        glDeleteProgram(engine->overlay_program);
        engine->overlay_program = 0;
    }
    engine->overlay_width = 0;
    engine->overlay_height = 0;
    engine->clear_button_width = 0;
    engine->clear_button_height = 0;
    engine->view_button_width = 0;
    engine->view_button_height = 0;
    engine->overlay_unavailable = false;
    engine->overlay_dirty = true;
    engine->view_button_dirty = true;
}

static bool polynomial_overlay_contains(const struct engine *engine, float x, float y) {
    return engine->view_kind == VIEW_WHOLE_PORTRAIT &&
        engine->overlay_width > 0 && engine->overlay_height > 0 &&
        x >= 16.0f && x < 16.0f + (float)engine->overlay_width &&
        y >= 16.0f && y < 16.0f + (float)engine->overlay_height;
}

static bool clear_button_contains(const struct engine *engine, float x, float y) {
    float left = 0.0f;
    float top = 0.0f;
    clear_button_origin(engine, &left, &top);
    return engine->clear_button_width > 0 && engine->clear_button_height > 0 &&
        x >= left && x < left + (float)engine->clear_button_width &&
        y >= top && y < top + (float)engine->clear_button_height;
}

static bool view_button_contains(const struct engine *engine, float x, float y) {
    float left = 0.0f;
    float top = 0.0f;
    view_button_origin(engine, &left, &top);
    return engine->view_button_width > 0 && engine->view_button_height > 0 &&
        x >= left && x < left + (float)engine->view_button_width &&
        y >= top && y < top + (float)engine->view_button_height;
}

#endif
