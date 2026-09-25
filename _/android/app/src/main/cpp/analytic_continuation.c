#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LOG_TAG "AnalyticContinuation"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define MAX_FACTORS 32
#define LASSO_COEFFICIENT_COUNT 5
#define LASSO_SAMPLE_COUNT 120

static const float TAU = 6.28318530717958647692f;
static const float FACTOR_PREIMAGE_LIMIT = 0.94f;
static const float LASSO_DERIVATIVE_BUDGET = 0.88f;

static const char *VERTEX_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec2 a_position;\n"
    "out vec2 v_ndc;\n"
    "void main() {\n"
    "    v_ndc = a_position;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

enum placement_kind {
    PLACEMENT_ZERO = 0,
    PLACEMENT_INFINITY = 1
};

enum factor_kind {
    FACTOR_NONE = -1,
    FACTOR_ZERO = 0,
    FACTOR_INFINITY = 1
};

struct engine {
    struct android_app *app;

    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;

    GLuint program;
    GLuint vao;
    GLuint vbo;
    GLint resolution_location;
    GLint zero_count_location;
    GLint pole_count_location;
    GLint zero_preimages_location;
    GLint pole_preimages_location;
    GLint zero_positions_location;
    GLint pole_positions_location;
    GLint lasso_coefficients_location;
    GLint phase_location;
    GLint paused_location;
    GLint zoom_location;
    GLint placement_kind_location;

    float zero_positions[MAX_FACTORS][2];
    float zero_preimages[MAX_FACTORS][2];
    int zero_count;
    float pole_positions[MAX_FACTORS][2];
    float pole_preimages[MAX_FACTORS][2];
    int pole_count;
    enum placement_kind placement_kind;

    float lasso_coefficients[LASSO_COEFFICIENT_COUNT][2];
    float lasso_start_coefficients[LASSO_COEFFICIENT_COUNT][2];
    float lasso_parameter[2];
    bool lasso_candidate;
    bool dragging_lasso;

    float phase;
    float phase_velocity;
    uint32_t random_state;
    double last_animation_time;
    bool paused;

    float zoom;
    float pinch_start_distance;
    float pinch_start_zoom;
    bool pinching;
    bool suppress_tap;

    enum factor_kind candidate_kind;
    int candidate_index;
    bool dragging_factor;
    bool moved;
    float down_x;
    float down_y;

    bool dirty;
    bool logged_first_frame;
};

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + 1.0e-9 * (double)now.tv_nsec;
}

static float random_signed(struct engine *engine) {
    uint32_t value = engine->random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    engine->random_state = value;
    return 2.0f * ((float)(value & 0x00ffffffu) / 16777215.0f) - 1.0f;
}

static void complex_multiply(
    float left_x,
    float left_y,
    float right_x,
    float right_y,
    float output[2]
) {
    output[0] = left_x * right_x - left_y * right_y;
    output[1] = left_x * right_y + left_y * right_x;
}

static bool complex_divide(
    float numerator_x,
    float numerator_y,
    float denominator_x,
    float denominator_y,
    float output[2]
) {
    float denominator = denominator_x * denominator_x + denominator_y * denominator_y;
    if (denominator < 1.0e-10f) {
        return false;
    }
    output[0] = (numerator_x * denominator_x + numerator_y * denominator_y) / denominator;
    output[1] = (numerator_y * denominator_x - numerator_x * denominator_y) / denominator;
    return true;
}

static void lasso_map_with_coefficients(
    const float coefficients[LASSO_COEFFICIENT_COUNT][2],
    float w_x,
    float w_y,
    float amount,
    float output[2],
    float derivative[2]
) {
    output[0] = w_x;
    output[1] = w_y;
    derivative[0] = 1.0f;
    derivative[1] = 0.0f;

    float power[2] = {w_x, w_y};
    for (int index = 0; index < LASSO_COEFFICIENT_COUNT; ++index) {
        int degree = index + 2;
        float next_power[2];
        complex_multiply(power[0], power[1], w_x, w_y, next_power);

        float mapped_term[2];
        complex_multiply(
            coefficients[index][0], coefficients[index][1],
            next_power[0], next_power[1], mapped_term
        );
        output[0] += amount * mapped_term[0];
        output[1] += amount * mapped_term[1];

        float derivative_term[2];
        complex_multiply(
            coefficients[index][0], coefficients[index][1],
            power[0], power[1], derivative_term
        );
        derivative[0] += amount * (float)degree * derivative_term[0];
        derivative[1] += amount * (float)degree * derivative_term[1];

        power[0] = next_power[0];
        power[1] = next_power[1];
    }
}

static void lasso_map(
    const struct engine *engine,
    float w_x,
    float w_y,
    float output[2]
) {
    float derivative[2];
    lasso_map_with_coefficients(
        engine->lasso_coefficients, w_x, w_y, 1.0f, output, derivative
    );
}

static bool inverse_lasso_with_coefficients(
    const float coefficients[LASSO_COEFFICIENT_COUNT][2],
    float z_x,
    float z_y,
    float output[2]
) {
    float w_x = z_x;
    float w_y = z_y;

    for (int stage = 1; stage <= 4; ++stage) {
        float amount = 0.25f * (float)stage;
        for (int iteration = 0; iteration < 4; ++iteration) {
            float mapped[2];
            float derivative[2];
            lasso_map_with_coefficients(
                coefficients, w_x, w_y, amount, mapped, derivative
            );

            float correction[2];
            if (!complex_divide(
                    mapped[0] - z_x, mapped[1] - z_y,
                    derivative[0], derivative[1], correction
                )) {
                return false;
            }

            float correction_length = hypotf(correction[0], correction[1]);
            if (correction_length > 0.55f) {
                float scale = 0.55f / correction_length;
                correction[0] *= scale;
                correction[1] *= scale;
            }
            w_x -= correction[0];
            w_y -= correction[1];
        }
    }

    float mapped[2];
    float derivative[2];
    lasso_map_with_coefficients(coefficients, w_x, w_y, 1.0f, mapped, derivative);
    float residual = hypotf(mapped[0] - z_x, mapped[1] - z_y);
    output[0] = w_x;
    output[1] = w_y;
    return residual < 2.0e-3f;
}

static bool inverse_lasso(
    const struct engine *engine,
    float z_x,
    float z_y,
    float output[2]
) {
    return inverse_lasso_with_coefficients(
        engine->lasso_coefficients, z_x, z_y, output
    );
}

static float lasso_derivative_budget(
    const float coefficients[LASSO_COEFFICIENT_COUNT][2]
) {
    float budget = 0.0f;
    for (int index = 0; index < LASSO_COEFFICIENT_COUNT; ++index) {
        budget += (float)(index + 2) * hypotf(
            coefficients[index][0], coefficients[index][1]
        );
    }
    return budget;
}

static bool compute_preimages(
    const float positions[MAX_FACTORS][2],
    int count,
    const float coefficients[LASSO_COEFFICIENT_COUNT][2],
    float output[MAX_FACTORS][2]
) {
    for (int index = 0; index < count; ++index) {
        float preimage[2];
        if (!inverse_lasso_with_coefficients(
                coefficients, positions[index][0], positions[index][1], preimage
            )) {
            return false;
        }
        if (hypotf(preimage[0], preimage[1]) >= FACTOR_PREIMAGE_LIMIT) {
            return false;
        }
        output[index][0] = preimage[0];
        output[index][1] = preimage[1];
    }
    return true;
}

static bool coefficients_are_valid(
    const struct engine *engine,
    const float coefficients[LASSO_COEFFICIENT_COUNT][2],
    float zero_preimages[MAX_FACTORS][2],
    float pole_preimages[MAX_FACTORS][2]
) {
    if (lasso_derivative_budget(coefficients) > LASSO_DERIVATIVE_BUDGET) {
        return false;
    }
    return compute_preimages(
        engine->zero_positions, engine->zero_count, coefficients, zero_preimages
    ) && compute_preimages(
        engine->pole_positions, engine->pole_count, coefficients, pole_preimages
    );
}

static void initialize_lasso(struct engine *engine) {
    memset(engine->lasso_coefficients, 0, sizeof(engine->lasso_coefficients));

    engine->zero_count = 1;
    engine->zero_positions[0][0] = 0.0f;
    engine->zero_positions[0][1] = 0.0f;
    engine->zero_preimages[0][0] = 0.0f;
    engine->zero_preimages[0][1] = 0.0f;
    engine->pole_count = 0;
    engine->placement_kind = PLACEMENT_ZERO;

    engine->phase = 0.0f;
    engine->phase_velocity = 0.48f;
    engine->random_state = 0xa341316cu;
    engine->last_animation_time = monotonic_seconds();
    engine->paused = false;

    engine->zoom = 1.0f;
    engine->pinch_start_distance = 0.0f;
    engine->pinch_start_zoom = 1.0f;
    engine->pinching = false;
    engine->suppress_tap = false;

    engine->candidate_kind = FACTOR_NONE;
    engine->candidate_index = -1;
    engine->dragging_factor = false;
    engine->lasso_candidate = false;
    engine->dragging_lasso = false;
    engine->moved = false;
    engine->dirty = true;
}

static char *load_asset_text(AAssetManager *manager, const char *name) {
    AAsset *asset = AAssetManager_open(manager, name, AASSET_MODE_BUFFER);
    if (asset == NULL) {
        LOGE("could not open asset %s", name);
        return NULL;
    }

    off64_t length = AAsset_getLength64(asset);
    char *text = malloc((size_t)length + 1u);
    if (text == NULL) {
        AAsset_close(asset);
        return NULL;
    }

    off64_t offset = 0;
    while (offset < length) {
        int amount = AAsset_read(asset, text + offset, (size_t)(length - offset));
        if (amount <= 0) {
            free(text);
            AAsset_close(asset);
            LOGE("could not read asset %s", name);
            return NULL;
        }
        offset += amount;
    }
    text[length] = '\0';
    AAsset_close(asset);
    return text;
}

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    char *log = length > 0 ? malloc((size_t)length) : NULL;
    if (log != NULL) {
        glGetShaderInfoLog(shader, length, NULL, log);
        LOGE("shader compilation failed: %s", log);
        free(log);
    } else {
        LOGE("shader compilation failed");
    }
    glDeleteShader(shader);
    return 0;
}

static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    char *log = length > 0 ? malloc((size_t)length) : NULL;
    if (log != NULL) {
        glGetProgramInfoLog(program, length, NULL, log);
        LOGE("program link failed: %s", log);
        free(log);
    } else {
        LOGE("program link failed");
    }
    glDeleteProgram(program);
    return 0;
}

static bool create_renderer(struct engine *engine) {
    static const GLfloat fullscreen_triangle[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f
    };

    char *fragment_source = load_asset_text(
        engine->app->activity->assetManager, "continuation.frag"
    );
    if (fragment_source == NULL) {
        return false;
    }

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    free(fragment_source);
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        return false;
    }

    engine->program = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    if (engine->program == 0) {
        return false;
    }

    engine->resolution_location = glGetUniformLocation(engine->program, "u_resolution");
    engine->zero_count_location = glGetUniformLocation(engine->program, "u_zero_count");
    engine->pole_count_location = glGetUniformLocation(engine->program, "u_pole_count");
    engine->zero_preimages_location = glGetUniformLocation(engine->program, "u_zero_preimages[0]");
    engine->pole_preimages_location = glGetUniformLocation(engine->program, "u_pole_preimages[0]");
    engine->zero_positions_location = glGetUniformLocation(engine->program, "u_zero_positions[0]");
    engine->pole_positions_location = glGetUniformLocation(engine->program, "u_pole_positions[0]");
    engine->lasso_coefficients_location = glGetUniformLocation(engine->program, "u_lasso_coefficients[0]");
    engine->phase_location = glGetUniformLocation(engine->program, "u_phase");
    engine->paused_location = glGetUniformLocation(engine->program, "u_paused");
    engine->zoom_location = glGetUniformLocation(engine->program, "u_zoom");
    engine->placement_kind_location = glGetUniformLocation(engine->program, "u_placement_kind");

    if (
        engine->resolution_location < 0 || engine->zero_count_location < 0 ||
        engine->pole_count_location < 0 || engine->zero_preimages_location < 0 ||
        engine->pole_preimages_location < 0 || engine->zero_positions_location < 0 ||
        engine->pole_positions_location < 0 || engine->lasso_coefficients_location < 0 ||
        engine->phase_location < 0 || engine->paused_location < 0 ||
        engine->zoom_location < 0 || engine->placement_kind_location < 0
    ) {
        LOGE("lasso shader uniforms unavailable");
        return false;
    }

    glGenVertexArrays(1, &engine->vao);
    glBindVertexArray(engine->vao);
    glGenBuffers(1, &engine->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, engine->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_triangle), fullscreen_triangle, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    LOGI(
        "lasso renderer ready: GL_VERSION=%s GL_RENDERER=%s",
        glGetString(GL_VERSION), glGetString(GL_RENDERER)
    );
    return true;
}

static bool initialize_display(struct engine *engine) {
    if (engine->app->window == NULL) {
        return false;
    }

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }

    EGLConfig config = NULL;
    EGLint config_count = 0;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &config_count) || config_count != 1) {
        LOGE("could not choose GLES3 EGL config: 0x%x", eglGetError());
        eglTerminate(display);
        return false;
    }

    EGLint format = 0;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(engine->app->window, 0, 0, format);

    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) {
        LOGE("could not create EGL surface/context: 0x%x", eglGetError());
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        return false;
    }

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    eglQuerySurface(display, surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(display, surface, EGL_HEIGHT, &engine->height);

    if (!create_renderer(engine)) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        engine->display = EGL_NO_DISPLAY;
        engine->surface = EGL_NO_SURFACE;
        engine->context = EGL_NO_CONTEXT;
        return false;
    }

    glViewport(0, 0, engine->width, engine->height);
    engine->last_animation_time = monotonic_seconds();
    engine->dirty = true;
    LOGI(
        "lasso ready: surface=%dx%d zeros=%d poles=%d placement=zero outside=domain-coloring",
        engine->width, engine->height, engine->zero_count, engine->pole_count
    );
    return true;
}

static void terminate_display(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    if (engine->vbo != 0) {
        glDeleteBuffers(1, &engine->vbo);
        engine->vbo = 0;
    }
    if (engine->vao != 0) {
        glDeleteVertexArrays(1, &engine->vao);
        engine->vao = 0;
    }
    if (engine->program != 0) {
        glDeleteProgram(engine->program);
        engine->program = 0;
    }

    eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (engine->context != EGL_NO_CONTEXT) eglDestroyContext(engine->display, engine->context);
    if (engine->surface != EGL_NO_SURFACE) eglDestroySurface(engine->display, engine->surface);
    eglTerminate(engine->display);
    engine->display = EGL_NO_DISPLAY;
    engine->surface = EGL_NO_SURFACE;
    engine->context = EGL_NO_CONTEXT;
}

static void update_surface_size(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY || engine->surface == EGL_NO_SURFACE) {
        return;
    }
    eglQuerySurface(engine->display, engine->surface, EGL_WIDTH, &engine->width);
    eglQuerySurface(engine->display, engine->surface, EGL_HEIGHT, &engine->height);
    glViewport(0, 0, engine->width, engine->height);
    engine->dirty = true;
}

static float lasso_pixel_radius(const struct engine *engine) {
    return 0.42f * fminf((float)engine->width, (float)engine->height);
}

static float view_pixel_radius(const struct engine *engine) {
    return lasso_pixel_radius(engine) * engine->zoom;
}

static void draw_frame(struct engine *engine) {
    if (
        engine->display == EGL_NO_DISPLAY || engine->program == 0 ||
        engine->width <= 0 || engine->height <= 0
    ) {
        return;
    }

    glUseProgram(engine->program);
    glUniform2f(engine->resolution_location, (float)engine->width, (float)engine->height);
    glUniform1i(engine->zero_count_location, engine->zero_count);
    glUniform1i(engine->pole_count_location, engine->pole_count);
    glUniform2fv(engine->zero_preimages_location, MAX_FACTORS, &engine->zero_preimages[0][0]);
    glUniform2fv(engine->pole_preimages_location, MAX_FACTORS, &engine->pole_preimages[0][0]);
    glUniform2fv(engine->zero_positions_location, MAX_FACTORS, &engine->zero_positions[0][0]);
    glUniform2fv(engine->pole_positions_location, MAX_FACTORS, &engine->pole_positions[0][0]);
    glUniform2fv(
        engine->lasso_coefficients_location,
        LASSO_COEFFICIENT_COUNT,
        &engine->lasso_coefficients[0][0]
    );
    glUniform1f(engine->phase_location, engine->phase);
    glUniform1i(engine->paused_location, engine->paused ? 1 : 0);
    glUniform1f(engine->zoom_location, engine->zoom);
    glUniform1i(engine->placement_kind_location, (int)engine->placement_kind);

    glBindVertexArray(engine->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (!engine->logged_first_frame) {
        GLubyte center_pixel[4] = {0, 0, 0, 0};
        GLubyte outside_pixel[4] = {0, 0, 0, 0};
        glReadPixels(
            engine->width / 2, engine->height / 2, 1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE, center_pixel
        );
        int outside_y = engine->height / 2 + (int)(1.25f * view_pixel_radius(engine));
        if (outside_y >= engine->height) outside_y = engine->height - 1;
        glReadPixels(
            engine->width / 2, outside_y, 1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE, outside_pixel
        );
        LOGI(
            "lasso first frame: center rgba=%u,%u,%u,%u outside rgba=%u,%u,%u,%u",
            center_pixel[0], center_pixel[1], center_pixel[2], center_pixel[3],
            outside_pixel[0], outside_pixel[1], outside_pixel[2], outside_pixel[3]
        );
        engine->logged_first_frame = true;
    }

    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    engine->dirty = false;
}

static void screen_to_plane(
    const struct engine *engine,
    float x,
    float y,
    float output[2]
) {
    float scale = view_pixel_radius(engine);
    output[0] = (x - 0.5f * (float)engine->width) / scale;
    output[1] = (0.5f * (float)engine->height - y) / scale;
}

static float control_radius(const struct engine *engine) {
    float radius = 0.052f * fminf((float)engine->width, (float)engine->height);
    if (radius < 28.0f) radius = 28.0f;
    if (radius > 42.0f) radius = 42.0f;
    return radius;
}

static float placement_radius(const struct engine *engine) {
    float radius = 0.048f * fminf((float)engine->width, (float)engine->height);
    if (radius < 26.0f) radius = 26.0f;
    if (radius > 38.0f) radius = 38.0f;
    return radius;
}

static bool pause_control_contains(const struct engine *engine, float x, float y) {
    float radius = control_radius(engine);
    return hypotf(x - radius - 16.0f, y - radius - 16.0f) <= radius;
}

static bool placement_control_hit(
    const struct engine *engine,
    float x,
    float y,
    enum placement_kind *kind
) {
    float radius = placement_radius(engine);
    float zero_x = radius + 16.0f;
    float infinity_x = zero_x + 2.0f * radius + 14.0f;
    float center_y = (float)engine->height - radius - 16.0f;

    if (hypotf(x - zero_x, y - center_y) <= radius) {
        *kind = PLACEMENT_ZERO;
        return true;
    }
    if (hypotf(x - infinity_x, y - center_y) <= radius) {
        *kind = PLACEMENT_INFINITY;
        return true;
    }
    return false;
}

static void nearest_factor(
    const struct engine *engine,
    float x,
    float y,
    enum factor_kind *kind,
    int *factor_index
) {
    *kind = FACTOR_NONE;
    *factor_index = -1;
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    float scale = view_pixel_radius(engine);
    float best_distance = 38.0f;

    for (int index = 0; index < engine->zero_count; ++index) {
        float distance = hypotf(
            (point[0] - engine->zero_positions[index][0]) * scale,
            (point[1] - engine->zero_positions[index][1]) * scale
        );
        if (distance < best_distance) {
            best_distance = distance;
            *kind = FACTOR_ZERO;
            *factor_index = index;
        }
    }
    for (int index = 0; index < engine->pole_count; ++index) {
        float distance = hypotf(
            (point[0] - engine->pole_positions[index][0]) * scale,
            (point[1] - engine->pole_positions[index][1]) * scale
        );
        if (distance < best_distance) {
            best_distance = distance;
            *kind = FACTOR_INFINITY;
            *factor_index = index;
        }
    }
}

static bool nearest_lasso_parameter(
    const struct engine *engine,
    float x,
    float y,
    float output[2]
) {
    if (engine->width <= 0 || engine->height <= 0) {
        return false;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    float scale = view_pixel_radius(engine);
    float best_distance = 44.0f;
    bool found = false;

    for (int sample = 0; sample < LASSO_SAMPLE_COUNT; ++sample) {
        float angle = TAU * (float)sample / (float)LASSO_SAMPLE_COUNT;
        float w_x = cosf(angle);
        float w_y = sinf(angle);
        float mapped[2];
        lasso_map(engine, w_x, w_y, mapped);
        float distance = hypotf(
            (point[0] - mapped[0]) * scale,
            (point[1] - mapped[1]) * scale
        );
        if (distance < best_distance) {
            best_distance = distance;
            output[0] = w_x;
            output[1] = w_y;
            found = true;
        }
    }
    return found;
}

static void move_factor(
    struct engine *engine,
    enum factor_kind kind,
    int index,
    float x,
    float y
) {
    float (*positions)[2] = kind == FACTOR_ZERO ? engine->zero_positions : engine->pole_positions;
    float (*preimages)[2] = kind == FACTOR_ZERO ? engine->zero_preimages : engine->pole_preimages;
    int count = kind == FACTOR_ZERO ? engine->zero_count : engine->pole_count;
    if (kind == FACTOR_NONE || index < 0 || index >= count) {
        return;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    float preimage[2];
    if (!inverse_lasso(engine, point[0], point[1], preimage)) {
        return;
    }

    float radius = hypotf(preimage[0], preimage[1]);
    if (radius >= FACTOR_PREIMAGE_LIMIT && radius > 0.0f) {
        float scale = FACTOR_PREIMAGE_LIMIT / radius;
        preimage[0] *= scale;
        preimage[1] *= scale;
        lasso_map(engine, preimage[0], preimage[1], point);
    }

    positions[index][0] = point[0];
    positions[index][1] = point[1];
    preimages[index][0] = preimage[0];
    preimages[index][1] = preimage[1];
    engine->dirty = true;
}

static void add_factor(
    struct engine *engine,
    enum placement_kind placement,
    float x,
    float y
) {
    int *count = placement == PLACEMENT_ZERO ? &engine->zero_count : &engine->pole_count;
    float (*positions)[2] = placement == PLACEMENT_ZERO ? engine->zero_positions : engine->pole_positions;
    float (*preimages)[2] = placement == PLACEMENT_ZERO ? engine->zero_preimages : engine->pole_preimages;
    const char *name = placement == PLACEMENT_ZERO ? "zero" : "infinity";

    if (*count >= MAX_FACTORS) {
        LOGI("%s ignored: limit=%d", name, MAX_FACTORS);
        return;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    float preimage[2];
    if (!inverse_lasso(engine, point[0], point[1], preimage)) {
        return;
    }
    if (hypotf(preimage[0], preimage[1]) >= FACTOR_PREIMAGE_LIMIT) {
        LOGI("%s ignored: tap outside lasso", name);
        return;
    }

    int index = *count;
    positions[index][0] = point[0];
    positions[index][1] = point[1];
    preimages[index][0] = preimage[0];
    preimages[index][1] = preimage[1];
    *count += 1;
    engine->dirty = true;

    LOGI(
        "%s added: z=%.6g%+.6gi preimage=%.6g%+.6gi count=%d",
        name, point[0], point[1], preimage[0], preimage[1], *count
    );
}

static void begin_lasso_drag(struct engine *engine) {
    memcpy(
        engine->lasso_start_coefficients,
        engine->lasso_coefficients,
        sizeof(engine->lasso_coefficients)
    );
}

static void deform_lasso(struct engine *engine, float x, float y) {
    float target[2];
    screen_to_plane(engine, x, y, target);

    float start_point[2];
    float derivative[2];
    lasso_map_with_coefficients(
        engine->lasso_start_coefficients,
        engine->lasso_parameter[0], engine->lasso_parameter[1],
        1.0f, start_point, derivative
    );

    float delta_x = target[0] - start_point[0];
    float delta_y = target[1] - start_point[1];
    float coefficient_delta[LASSO_COEFFICIENT_COUNT][2];
    float power[2] = {engine->lasso_parameter[0], engine->lasso_parameter[1]};

    for (int index = 0; index < LASSO_COEFFICIENT_COUNT; ++index) {
        float next_power[2];
        complex_multiply(
            power[0], power[1],
            engine->lasso_parameter[0], engine->lasso_parameter[1],
            next_power
        );
        coefficient_delta[index][0] =
            (delta_x * next_power[0] + delta_y * next_power[1]) /
            (float)LASSO_COEFFICIENT_COUNT;
        coefficient_delta[index][1] =
            (delta_y * next_power[0] - delta_x * next_power[1]) /
            (float)LASSO_COEFFICIENT_COUNT;
        power[0] = next_power[0];
        power[1] = next_power[1];
    }

    float best_coefficients[LASSO_COEFFICIENT_COUNT][2];
    float best_zero_preimages[MAX_FACTORS][2];
    float best_pole_preimages[MAX_FACTORS][2];
    memcpy(best_coefficients, engine->lasso_start_coefficients, sizeof(best_coefficients));
    memcpy(best_zero_preimages, engine->zero_preimages, sizeof(best_zero_preimages));
    memcpy(best_pole_preimages, engine->pole_preimages, sizeof(best_pole_preimages));

    float low = 0.0f;
    float high = 1.0f;
    for (int search = 0; search < 13; ++search) {
        float amount = 0.5f * (low + high);
        float candidate[LASSO_COEFFICIENT_COUNT][2];
        float candidate_zero_preimages[MAX_FACTORS][2];
        float candidate_pole_preimages[MAX_FACTORS][2];
        for (int index = 0; index < LASSO_COEFFICIENT_COUNT; ++index) {
            candidate[index][0] = engine->lasso_start_coefficients[index][0] + amount * coefficient_delta[index][0];
            candidate[index][1] = engine->lasso_start_coefficients[index][1] + amount * coefficient_delta[index][1];
        }

        if (coefficients_are_valid(
                engine, candidate, candidate_zero_preimages, candidate_pole_preimages
            )) {
            low = amount;
            memcpy(best_coefficients, candidate, sizeof(best_coefficients));
            memcpy(best_zero_preimages, candidate_zero_preimages, sizeof(best_zero_preimages));
            memcpy(best_pole_preimages, candidate_pole_preimages, sizeof(best_pole_preimages));
        } else {
            high = amount;
        }
    }

    memcpy(engine->lasso_coefficients, best_coefficients, sizeof(engine->lasso_coefficients));
    memcpy(engine->zero_preimages, best_zero_preimages, sizeof(engine->zero_preimages));
    memcpy(engine->pole_preimages, best_pole_preimages, sizeof(engine->pole_preimages));
    engine->dirty = true;

    LOGI(
        "lasso deformed: amount=%.4f budget=%.4f parameter=%.4g%+.4gi",
        low,
        lasso_derivative_budget(engine->lasso_coefficients),
        engine->lasso_parameter[0], engine->lasso_parameter[1]
    );
}

static void toggle_pause(struct engine *engine) {
    engine->paused = !engine->paused;
    engine->last_animation_time = monotonic_seconds();
    engine->dirty = true;
    LOGI("lasso phase walk %s", engine->paused ? "paused" : "running");
}

static void advance_phase(struct engine *engine) {
    double now = monotonic_seconds();
    float dt = (float)(now - engine->last_animation_time);
    engine->last_animation_time = now;
    if (dt <= 0.0f) return;
    if (dt > 0.05f) dt = 0.05f;

    float noise = random_signed(engine);
    engine->phase_velocity += 1.15f * sqrtf(dt) * noise;
    engine->phase_velocity *= expf(-0.85f * dt);
    if (engine->phase_velocity > 1.6f) engine->phase_velocity = 1.6f;
    if (engine->phase_velocity < -1.6f) engine->phase_velocity = -1.6f;

    engine->phase += engine->phase_velocity * dt;
    if (engine->phase >= TAU || engine->phase <= -TAU) {
        engine->phase = fmodf(engine->phase, TAU);
    }
    engine->dirty = true;
}

static void clear_gesture(struct engine *engine) {
    engine->candidate_kind = FACTOR_NONE;
    engine->candidate_index = -1;
    engine->dragging_factor = false;
    engine->lasso_candidate = false;
    engine->dragging_lasso = false;
    engine->moved = false;
}

static float pinch_distance(AInputEvent *event) {
    if (AMotionEvent_getPointerCount(event) < 2) return 0.0f;
    float dx = AMotionEvent_getX(event, 1) - AMotionEvent_getX(event, 0);
    float dy = AMotionEvent_getY(event, 1) - AMotionEvent_getY(event, 0);
    return hypotf(dx, dy);
}

static void begin_pinch(struct engine *engine, AInputEvent *event) {
    float distance = pinch_distance(event);
    if (distance < 8.0f) return;
    engine->pinching = true;
    engine->suppress_tap = true;
    engine->pinch_start_distance = distance;
    engine->pinch_start_zoom = engine->zoom;
    clear_gesture(engine);
}

static void update_pinch(struct engine *engine, AInputEvent *event) {
    if (!engine->pinching || engine->pinch_start_distance < 8.0f) return;
    float distance = pinch_distance(event);
    if (distance < 8.0f) return;

    float zoom = engine->pinch_start_zoom * distance / engine->pinch_start_distance;
    if (zoom < 0.5f) zoom = 0.5f;
    if (zoom > 4.0f) zoom = 4.0f;
    if (fabsf(zoom - engine->zoom) > 1.0e-4f) {
        engine->zoom = zoom;
        engine->dirty = true;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int32_t masked_action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked_action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            engine->suppress_tap = false;

            if (pause_control_contains(engine, x, y)) {
                toggle_pause(engine);
                clear_gesture(engine);
                engine->suppress_tap = true;
                return 1;
            }

            enum placement_kind selected;
            if (placement_control_hit(engine, x, y, &selected)) {
                engine->placement_kind = selected;
                engine->dirty = true;
                clear_gesture(engine);
                engine->suppress_tap = true;
                LOGI(
                    "placement selected: %s",
                    selected == PLACEMENT_ZERO ? "zero" : "infinity"
                );
                return 1;
            }

            engine->down_x = x;
            engine->down_y = y;
            nearest_factor(
                engine, x, y, &engine->candidate_kind, &engine->candidate_index
            );
            engine->dragging_factor = false;
            engine->lasso_candidate = false;
            engine->dragging_lasso = false;
            engine->moved = false;

            if (engine->candidate_kind == FACTOR_NONE) {
                engine->lasso_candidate = nearest_lasso_parameter(
                    engine, x, y, engine->lasso_parameter
                );
                if (engine->lasso_candidate) begin_lasso_drag(engine);
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (pointer_count >= 2) begin_pinch(engine, event);
            return 1;

        case AMOTION_EVENT_ACTION_MOVE: {
            if (pointer_count >= 2) {
                update_pinch(engine, event);
                return 1;
            }
            if (pointer_count != 1) return 1;

            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            if (hypotf(x - engine->down_x, y - engine->down_y) > 10.0f) {
                engine->moved = true;
            }

            if (engine->moved && engine->candidate_kind != FACTOR_NONE) {
                engine->dragging_factor = true;
                move_factor(
                    engine, engine->candidate_kind, engine->candidate_index, x, y
                );
            } else if (engine->moved && engine->lasso_candidate) {
                engine->dragging_lasso = true;
                deform_lasso(engine, x, y);
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_UP: {
            if (engine->suppress_tap || engine->pinching) {
                engine->pinching = false;
                engine->suppress_tap = false;
                clear_gesture(engine);
                return 1;
            }

            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            if (!engine->moved && !engine->dragging_factor && !engine->dragging_lasso) {
                add_factor(engine, engine->placement_kind, x, y);
            } else if (engine->dragging_factor && engine->candidate_kind != FACTOR_NONE) {
                const char *name = engine->candidate_kind == FACTOR_ZERO ? "zero" : "infinity";
                float (*positions)[2] = engine->candidate_kind == FACTOR_ZERO
                    ? engine->zero_positions : engine->pole_positions;
                LOGI(
                    "%s moved: index=%d z=%.6g%+.6gi",
                    name, engine->candidate_index,
                    positions[engine->candidate_index][0],
                    positions[engine->candidate_index][1]
                );
            } else if (engine->dragging_lasso) {
                LOGI(
                    "lasso drag finished: budget=%.4f",
                    lasso_derivative_budget(engine->lasso_coefficients)
                );
            }
            clear_gesture(engine);
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_UP:
            if (engine->pinching) LOGI("lasso zoom: %.3f", engine->zoom);
            engine->pinching = false;
            engine->suppress_tap = true;
            clear_gesture(engine);
            return 1;

        case AMOTION_EVENT_ACTION_CANCEL:
            engine->pinching = false;
            engine->suppress_tap = false;
            clear_gesture(engine);
            return 1;

        default:
            return 0;
    }
}

static void handle_command(struct android_app *app, int32_t command) {
    struct engine *engine = app->userData;
    switch (command) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL && engine->display == EGL_NO_DISPLAY) {
                initialize_display(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            terminate_display(engine);
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_CONFIG_CHANGED:
            update_surface_size(engine);
            break;
        case APP_CMD_GAINED_FOCUS:
            engine->last_animation_time = monotonic_seconds();
            engine->dirty = true;
            break;
        default:
            break;
    }
}

void android_main(struct android_app *app) {
    struct engine engine = {
        .app = app,
        .display = EGL_NO_DISPLAY,
        .surface = EGL_NO_SURFACE,
        .context = EGL_NO_CONTEXT,
        .candidate_kind = FACTOR_NONE,
        .candidate_index = -1,
        .dirty = true,
        .logged_first_frame = false
    };
    initialize_lasso(&engine);

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        bool can_animate = engine.display != EGL_NO_DISPLAY && !engine.paused;
        int timeout = can_animate ? 16 : (engine.dirty ? 0 : -1);
        int ident = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (ident >= 0 && source != NULL) source->process(app, source);
        if (app->destroyRequested != 0) {
            terminate_display(&engine);
            return;
        }
        if (engine.display != EGL_NO_DISPLAY && !engine.paused) advance_phase(&engine);
        if (engine.dirty) draw_frame(&engine);
    }
}
