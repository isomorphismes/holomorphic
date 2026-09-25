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

#include "holomorphic_walk.h"

#define LOG_TAG "AnalyticContinuation"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define MAX_FACTORS 32
#define REMOTE_POLE_COUNT 24

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
    PLACEMENT_POLE = 1
};

enum factor_kind {
    FACTOR_NONE = -1,
    FACTOR_ZERO = 0,
    FACTOR_POLE = 1
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
    GLint zero_positions_location;
    GLint pole_positions_location;
    GLint holomorphic_coefficients_location;
    GLint remote_pole_positions_location;
    GLint zoom_location;
    GLint placement_kind_location;

    float zero_positions[MAX_FACTORS][2];
    int zero_count;
    float pole_positions[MAX_FACTORS][2];
    int pole_count;
    enum placement_kind placement_kind;

    float holomorphic_coefficients[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float deformation_velocity[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    float remote_pole_time;
    double deformation_last_time;
    double deformation_last_publish;
    double deformation_last_log;
    uint64_t deformation_accepted_steps;
    bool deformation_workers_started;
    bool deformation_direction_ready;
    bool focused;

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

static void initialize_state(struct engine *engine) {
    engine->zero_count = 1;
    engine->zero_positions[0][0] = -0.34f;
    engine->zero_positions[0][1] = 0.0f;

    engine->pole_count = 1;
    engine->pole_positions[0][0] = 0.34f;
    engine->pole_positions[0][1] = 0.0f;

    engine->placement_kind = PLACEMENT_ZERO;
    memset(engine->holomorphic_coefficients, 0, sizeof(engine->holomorphic_coefficients));
    memset(engine->deformation_velocity, 0, sizeof(engine->deformation_velocity));
    engine->remote_pole_time = 0.0f;
    engine->deformation_last_time = monotonic_seconds();
    engine->deformation_last_publish = 0.0;
    engine->deformation_last_log = 0.0;
    engine->deformation_accepted_steps = 0;
    engine->deformation_workers_started = false;
    engine->deformation_direction_ready = false;
    engine->focused = false;

    engine->zoom = 1.0f;
    engine->pinch_start_distance = 0.0f;
    engine->pinch_start_zoom = 1.0f;
    engine->pinching = false;
    engine->suppress_tap = false;

    engine->candidate_kind = FACTOR_NONE;
    engine->candidate_index = -1;
    engine->dragging_factor = false;
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
    engine->zero_positions_location = glGetUniformLocation(engine->program, "u_zero_positions[0]");
    engine->pole_positions_location = glGetUniformLocation(engine->program, "u_pole_positions[0]");
    engine->holomorphic_coefficients_location = glGetUniformLocation(
        engine->program, "u_holomorphic_coefficients[0]"
    );
    engine->remote_pole_positions_location = glGetUniformLocation(
        engine->program, "u_remote_pole_positions[0]"
    );
    engine->zoom_location = glGetUniformLocation(engine->program, "u_zoom");
    engine->placement_kind_location = glGetUniformLocation(engine->program, "u_placement_kind");

    if (
        engine->resolution_location < 0 || engine->zero_count_location < 0 ||
        engine->pole_count_location < 0 || engine->zero_positions_location < 0 ||
        engine->pole_positions_location < 0 ||
        engine->holomorphic_coefficients_location < 0 ||
        engine->remote_pole_positions_location < 0 ||
        engine->zoom_location < 0 || engine->placement_kind_location < 0
    ) {
        LOGE("holomorphic field shader uniforms unavailable");
        return false;
    }

    glGenVertexArrays(1, &engine->vao);
    glBindVertexArray(engine->vao);
    glGenBuffers(1, &engine->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, engine->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_triangle), fullscreen_triangle, GL_STATIC_DRAW);
    glVertexAttribPointer(
        0, 2, GL_FLOAT, GL_FALSE, 2 * (GLsizei)sizeof(GLfloat), (const void *)0
    );
    glEnableVertexAttribArray(0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    LOGI(
        "holomorphic field renderer ready: GL_VERSION=%s GL_RENDERER=%s",
        glGetString(GL_VERSION), glGetString(GL_RENDERER)
    );
    LOGI("remote pole path: CPU positions + collapsed fragment product");
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
    if (
        !eglChooseConfig(display, config_attributes, &config, 1, &config_count) ||
        config_count != 1
    ) {
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
    engine->dirty = true;
    LOGI(
        "holomorphic field ready: surface=%dx%d zeros=%d poles=%d",
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
    if (engine->context != EGL_NO_CONTEXT) {
        eglDestroyContext(engine->display, engine->context);
    }
    if (engine->surface != EGL_NO_SURFACE) {
        eglDestroySurface(engine->display, engine->surface);
    }
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

static float view_pixel_radius(const struct engine *engine) {
    return 0.42f * fminf((float)engine->width, (float)engine->height) * engine->zoom;
}

static float remote_pole_hash(float x) {
    float value = sinf(x * 127.1f + 31.7f) * 43758.5453123f;
    return value - floorf(value);
}

static void remote_pole_positions_for_frame(
    const struct engine *engine,
    float positions[REMOTE_POLE_COUNT][2]
) {
    const float tau = 6.2831853f;
    // Keep the wanderers in the mathematical plane. Zoom changes the view,
    // not their coordinates. At zoom 1 this matches the established orbit scale.
    float base_pixel_radius = 0.42f * fminf(
        (float)engine->width, (float)engine->height
    );
    float world_outer_radius = hypotf(
        0.5f * (float)engine->width,
        0.5f * (float)engine->height
    ) / base_pixel_radius;

    for (int index = 0; index < REMOTE_POLE_COUNT; ++index) {
        float k = (float)index;
        float initial_angle = tau * remote_pole_hash(k + 0.11f);
        float radius_scale = 2.75f
            + 1.25f * remote_pole_hash(k + 1.73f);
        float speed = 0.11f
            + 0.08f * remote_pole_hash(k + 4.37f);
        float handedness = remote_pole_hash(k + 7.91f) < 0.5f ? -1.0f : 1.0f;
        float angle = initial_angle
            + handedness * speed * engine->remote_pole_time;
        float bend_phase = tau * remote_pole_hash(k + 11.23f);
        float radius = radius_scale
            + 0.22f * sinf(2.0f * angle + bend_phase);
        float ellipticity = -0.08f
            + 0.16f * remote_pole_hash(k + 14.67f);

        positions[index][0] = world_outer_radius * radius
            * (1.0f + ellipticity) * cosf(angle);
        positions[index][1] = world_outer_radius * radius
            * (1.0f - ellipticity) * sinf(angle);
    }
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
    glUniform2fv(
        engine->zero_positions_location, MAX_FACTORS, &engine->zero_positions[0][0]
    );
    glUniform2fv(
        engine->pole_positions_location, MAX_FACTORS, &engine->pole_positions[0][0]
    );
    glUniform2fv(
        engine->holomorphic_coefficients_location,
        HOLOMORPHIC_WALK_COEFFICIENT_COUNT,
        &engine->holomorphic_coefficients[0][0]
    );
    float remote_pole_positions[REMOTE_POLE_COUNT][2];
    remote_pole_positions_for_frame(engine, remote_pole_positions);
    glUniform2fv(
        engine->remote_pole_positions_location,
        REMOTE_POLE_COUNT,
        &remote_pole_positions[0][0]
    );
    glUniform1f(engine->zoom_location, engine->zoom);
    glUniform1i(engine->placement_kind_location, (int)engine->placement_kind);

    glBindVertexArray(engine->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    if (!engine->logged_first_frame) {
        GLubyte center_pixel[4] = {0, 0, 0, 0};
        glReadPixels(
            engine->width / 2, engine->height / 2, 1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE, center_pixel
        );
        LOGI(
            "holomorphic field first frame: center rgba=%u,%u,%u,%u",
            center_pixel[0], center_pixel[1], center_pixel[2], center_pixel[3]
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

static float placement_radius(const struct engine *engine) {
    float radius = 0.048f * fminf((float)engine->width, (float)engine->height);
    if (radius < 26.0f) radius = 26.0f;
    if (radius > 38.0f) radius = 38.0f;
    return radius;
}

static bool placement_control_hit(
    const struct engine *engine,
    float x,
    float y,
    enum placement_kind *kind
) {
    float radius = placement_radius(engine);
    float zero_x = radius + 16.0f;
    float pole_x = zero_x + 2.0f * radius + 14.0f;
    float center_y = (float)engine->height - radius - 16.0f;

    if (hypotf(x - zero_x, y - center_y) <= radius) {
        *kind = PLACEMENT_ZERO;
        return true;
    }
    if (hypotf(x - pole_x, y - center_y) <= radius) {
        *kind = PLACEMENT_POLE;
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
            *kind = FACTOR_POLE;
            *factor_index = index;
        }
    }
}

static void move_factor(
    struct engine *engine,
    enum factor_kind kind,
    int index,
    float x,
    float y
) {
    float (*positions)[2] = kind == FACTOR_ZERO
        ? engine->zero_positions
        : engine->pole_positions;
    int count = kind == FACTOR_ZERO ? engine->zero_count : engine->pole_count;
    if (kind == FACTOR_NONE || index < 0 || index >= count) {
        return;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    positions[index][0] = point[0];
    positions[index][1] = point[1];
    engine->dirty = true;
}

static void add_factor(
    struct engine *engine,
    enum placement_kind placement,
    float x,
    float y
) {
    int *count = placement == PLACEMENT_ZERO
        ? &engine->zero_count
        : &engine->pole_count;
    float (*positions)[2] = placement == PLACEMENT_ZERO
        ? engine->zero_positions
        : engine->pole_positions;
    const char *name = placement == PLACEMENT_ZERO ? "zero" : "pole";

    if (*count >= MAX_FACTORS) {
        LOGI("%s ignored: limit=%d", name, MAX_FACTORS);
        return;
    }

    float point[2];
    screen_to_plane(engine, x, y, point);
    int index = *count;
    positions[index][0] = point[0];
    positions[index][1] = point[1];
    *count += 1;
    engine->dirty = true;

    LOGI("%s added: z=%.6g%+.6gi count=%d", name, point[0], point[1], *count);
}

static void publish_deformation_snapshot(struct engine *engine, double now) {
    if (!engine->deformation_workers_started) {
        return;
    }
    if (
        engine->deformation_last_publish == 0.0 ||
        now - engine->deformation_last_publish >= 0.200
    ) {
        holomorphic_walk_publish(engine->holomorphic_coefficients);
        engine->deformation_last_publish = now;
    }
}

static void log_holomorphic_state(struct engine *engine, double now, float score) {
    if (now - engine->deformation_last_log < 2.0) {
        return;
    }
    LOGI(
        "holomorphic field: workers=%d steps=%llu budget=%.4f score=%.6g zeros=%d poles=%d",
        HOLOMORPHIC_WALK_WORKER_COUNT,
        (unsigned long long)engine->deformation_accepted_steps,
        holomorphic_walk_coefficient_budget(engine->holomorphic_coefficients),
        score,
        engine->zero_count,
        engine->pole_count
    );
    engine->deformation_last_log = now;
}

static void advance_holomorphic_function(struct engine *engine) {
    double now = monotonic_seconds();
    float dt = (float)(now - engine->deformation_last_time);
    engine->deformation_last_time = now;
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    publish_deformation_snapshot(engine, now);
    if (!engine->focused || engine->dragging_factor || engine->pinching) {
        return;
    }

    engine->remote_pole_time += dt;
    engine->dirty = true;

    float score = 0.0f;
    float direction[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    if (
        engine->deformation_workers_started &&
        holomorphic_walk_best_direction(direction, &score)
    ) {
        float blend = 1.0f - expf(-4.0f * dt);
        const float speed = 0.30f;
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            engine->deformation_velocity[index][0] =
                (1.0f - blend) * engine->deformation_velocity[index][0] +
                blend * speed * direction[index][0];
            engine->deformation_velocity[index][1] =
                (1.0f - blend) * engine->deformation_velocity[index][1] +
                blend * speed * direction[index][1];
        }
        engine->deformation_direction_ready = true;
    }

    if (!engine->deformation_direction_ready) {
        log_holomorphic_state(engine, now, score);
        return;
    }

    float candidate[HOLOMORPHIC_WALK_COEFFICIENT_COUNT][2];
    for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
        candidate[index][0] =
            engine->holomorphic_coefficients[index][0] +
            dt * engine->deformation_velocity[index][0];
        candidate[index][1] =
            engine->holomorphic_coefficients[index][1] +
            dt * engine->deformation_velocity[index][1];
    }

    if (
        holomorphic_walk_coefficient_budget(candidate) <=
        HOLOMORPHIC_WALK_COEFFICIENT_BUDGET
    ) {
        memcpy(
            engine->holomorphic_coefficients,
            candidate,
            sizeof(engine->holomorphic_coefficients)
        );
        engine->deformation_accepted_steps += 1;
        engine->dirty = true;
    } else {
        for (int index = 0; index < HOLOMORPHIC_WALK_COEFFICIENT_COUNT; ++index) {
            engine->deformation_velocity[index][0] *= -0.30f;
            engine->deformation_velocity[index][1] *= -0.30f;
        }
        if (engine->deformation_workers_started) {
            holomorphic_walk_publish(engine->holomorphic_coefficients);
            engine->deformation_last_publish = now;
        }
    }

    log_holomorphic_state(engine, now, score);
}

static void clear_gesture(struct engine *engine) {
    engine->candidate_kind = FACTOR_NONE;
    engine->candidate_index = -1;
    engine->dragging_factor = false;
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
    if (isfinite(zoom) && zoom > 0.0f && zoom != engine->zoom) {
        engine->zoom = zoom;
        engine->dirty = true;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t masked_action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked_action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            engine->suppress_tap = false;

            enum placement_kind selected;
            if (placement_control_hit(engine, x, y, &selected)) {
                engine->placement_kind = selected;
                engine->dirty = true;
                clear_gesture(engine);
                engine->suppress_tap = true;
                LOGI(
                    "placement selected: %s",
                    selected == PLACEMENT_ZERO ? "zero" : "pole"
                );
                return 1;
            }

            engine->down_x = x;
            engine->down_y = y;
            nearest_factor(
                engine, x, y, &engine->candidate_kind, &engine->candidate_index
            );
            engine->dragging_factor = false;
            engine->moved = false;
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            if (pointer_count >= 2) {
                begin_pinch(engine, event);
            }
            return 1;

        case AMOTION_EVENT_ACTION_MOVE: {
            if (pointer_count >= 2) {
                update_pinch(engine, event);
                return 1;
            }
            if (pointer_count != 1) {
                return 1;
            }

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
            if (!engine->moved && !engine->dragging_factor) {
                add_factor(engine, engine->placement_kind, x, y);
            } else if (
                engine->dragging_factor && engine->candidate_kind != FACTOR_NONE
            ) {
                const char *name = engine->candidate_kind == FACTOR_ZERO
                    ? "zero"
                    : "pole";
                float (*positions)[2] = engine->candidate_kind == FACTOR_ZERO
                    ? engine->zero_positions
                    : engine->pole_positions;
                LOGI(
                    "%s moved: index=%d z=%.6g%+.6gi",
                    name,
                    engine->candidate_index,
                    positions[engine->candidate_index][0],
                    positions[engine->candidate_index][1]
                );
            }
            clear_gesture(engine);
            if (engine->deformation_workers_started) {
                holomorphic_walk_publish(engine->holomorphic_coefficients);
                engine->deformation_last_publish = monotonic_seconds();
            }
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_UP:
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
            engine->focused = true;
            engine->deformation_last_time = monotonic_seconds();
            engine->dirty = true;
            break;
        case APP_CMD_LOST_FOCUS:
            engine->focused = false;
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
    initialize_state(&engine);

    engine.deformation_workers_started = holomorphic_walk_start();
    if (engine.deformation_workers_started) {
        holomorphic_walk_publish(engine.holomorphic_coefficients);
        engine.deformation_last_publish = monotonic_seconds();
        LOGI(
            "holomorphic field started with %d workers",
            HOLOMORPHIC_WALK_WORKER_COUNT
        );
    } else {
        LOGE("holomorphic direction workers unavailable; rendering static meromorphic map");
    }

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        bool can_animate = engine.display != EGL_NO_DISPLAY && engine.focused;
        int timeout = can_animate ? 16 : (engine.dirty ? 0 : -1);
        int ident = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (ident >= 0 && source != NULL) {
            source->process(app, source);
        }
        if (app->destroyRequested != 0) {
            if (engine.deformation_workers_started) {
                holomorphic_walk_stop();
                engine.deformation_workers_started = false;
            }
            terminate_display(&engine);
            return;
        }
        if (engine.display != EGL_NO_DISPLAY && engine.focused) {
            advance_holomorphic_function(&engine);
        }
        if (engine.dirty) {
            draw_frame(&engine);
        }
    }
}
