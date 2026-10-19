#include <android/asset_manager.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "holomorphic_walk.h"
#include "holomorphic_divisor.h"
#include "holomorphic_shader_binding.h"
#include "runtime_source_binding.h"

#define LOG_TAG "AnalyticContinuation"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define MAX_FACTORS 32

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
    GLint remote_pole_positions_location;
    GLint holomorphic_coefficients_location;
    GLint divisor_count_location;
    GLint divisor_factors_location;
    GLint remote_pole_time_location;
    GLint numeric_output_location;
    GLint numeric_point_location;
    GLint zoom_location;
    GLint placement_kind_location;

    float zero_positions[MAX_FACTORS][2];
    int zero_count;
    float pole_positions[MAX_FACTORS][2];
    float remote_pole_positions[HOLOMORPHIC_REMOTE_POLE_COUNT][2];
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
    bool numeric_probe_attempted;
    uint64_t timing_frames;
    double timing_submission_cpu_ms;
    double timing_presentation_cpu_ms;
    double timing_window_start;
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
    engine->numeric_probe_attempted = false;
    engine->timing_frames = 0;
    engine->timing_submission_cpu_ms = 0.0;
    engine->timing_presentation_cpu_ms = 0.0;
    engine->timing_window_start = 0.0;
}

static char *load_asset_text(AAssetManager *manager, const char *name, size_t *byte_length) {
    AAsset *asset = AAssetManager_open(manager, name, AASSET_MODE_BUFFER);
    if (asset == NULL) {
        LOGE("could not open asset %s", name);
        return NULL;
    }

    off64_t length = AAsset_getLength64(asset);
    if (length <= 0 || (uint64_t)length >= (uint64_t)INT_MAX) {
        AAsset_close(asset);
        LOGE("invalid text asset length for %s", name);
        return NULL;
    }
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
    if (holo_source_contains_nul(text, (size_t)length)) {
        LOGE("embedded NUL in text asset %s", name);
        free(text);
        return NULL;
    }
    if (byte_length != NULL) *byte_length = (size_t)length;
    return text;
}

static GLuint compile_shader(GLenum type, const char *source, size_t byte_length, const char *receipt) {
    char submitted_sha256[65] = {0};
    if (type == GL_FRAGMENT_SHADER && HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        const struct holo_source_binding expected = {
            HOLOMORPHIC_FRAGMENT_LENGTH, HOLOMORPHIC_FRAGMENT_SHA256,
            HOLOMORPHIC_BACKEND_COMMIT, HOLOMORPHIC_APPLICATION_COMMIT
        };
        const char *failure = holo_verify_source_binding(
            source, byte_length, receipt, &expected, submitted_sha256
        );
        if (failure != NULL) {
            LOGE("holomorphic field shader provenance rejected: %s fallback=none", failure);
            return 0;
        }
    } else if (type == GL_FRAGMENT_SHADER && receipt != NULL) {
        LOGE("holomorphic field shader provenance rejected: generated-receipt-in-legacy-mode fallback=none");
        return 0;
    }
    if (byte_length == 0 || byte_length > INT_MAX) return 0;
    GLuint shader = glCreateShader(type);
    const GLint source_length = (GLint)byte_length;
    glShaderSource(shader, 1, &source, &source_length);
    if (type == GL_FRAGMENT_SHADER && HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        /* Read the selected object's source back from GL before compilation.
         * The receipt describes the actual submission and driver's retained
         * bytes, not merely an asset or a neighbouring provenance sidecar. */
        GLint stored_length = 0;
        glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &stored_length);
        char *stored = stored_length > 0 ? malloc((size_t)stored_length) : NULL;
        GLsizei retrieved_length = 0;
        if (stored != NULL) glGetShaderSource(shader, stored_length, &retrieved_length, stored);
        char driver_sha256[65] = {0};
        if (stored != NULL && retrieved_length >= 0)
            holo_source_sha256(stored, (size_t)retrieved_length, driver_sha256);
        free(stored);
        if ((size_t)retrieved_length != byte_length || strcmp(driver_sha256, submitted_sha256) != 0) {
            LOGE("holomorphic field shader provenance rejected: driver-source-mismatch fallback=none");
            glDeleteShader(shader);
            return 0;
        }
        LOGI(
            "holomorphic field shader bytes verified: mode=checked-idris compiler=idris2-glsles "
            "backend_commit=%s application_commit=%s fragment_sha256=%s bytes=%zu "
            "submission=glShaderSource fallback=none driver_readback_sha256=%s",
            HOLOMORPHIC_BACKEND_COMMIT, HOLOMORPHIC_APPLICATION_COMMIT,
            submitted_sha256, byte_length, driver_sha256
        );
    }
    double compile_start = monotonic_seconds();
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    LOGI("HOLO_SHADER_COMPILE stage=%s status=%s cpu_wall_ms=%.9g gpu_time=NOT_RUN",
        type == GL_FRAGMENT_SHADER ? "fragment" : "vertex",
        compiled == GL_TRUE ? "PASS" : "FAIL", (monotonic_seconds() - compile_start) * 1000.0);
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
    double link_start = monotonic_seconds();
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    LOGI("HOLO_SHADER_LINK status=%s cpu_wall_ms=%.9g gpu_time=NOT_RUN",
        linked == GL_TRUE ? "PASS" : "FAIL", (monotonic_seconds() - link_start) * 1000.0);
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

    GLint uniform_vectors = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &uniform_vectors);
    LOGI("holomorphic field shader capacity: fragment_uniform_vectors=%d required_generated=197", uniform_vectors);
    if (HOLOMORPHIC_REQUIRE_GENERATED_SHADER && uniform_vectors < 197) {
        LOGE("holomorphic field shader rejected: insufficient-fragment-uniforms fallback=none");
        return false;
    }
    size_t fragment_length = 0;
    char *fragment_source = load_asset_text(
        engine->app->activity->assetManager, "continuation.frag", &fragment_length
    );
    if (fragment_source == NULL) {
        return false;
    }

    AAsset *receipt_asset = AAssetManager_open(
        engine->app->activity->assetManager, "continuation.provenance", AASSET_MODE_BUFFER
    );
    char *fragment_receipt = NULL;
    if (receipt_asset != NULL) {
        AAsset_close(receipt_asset);
        fragment_receipt = load_asset_text(
            engine->app->activity->assetManager, "continuation.provenance", NULL
        );
        if (fragment_receipt == NULL) {
            free(fragment_source);
            return false;
        }
    }
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER, strlen(VERTEX_SHADER), NULL);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, fragment_length, fragment_receipt);
    free(fragment_source);
    free(fragment_receipt);
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
    engine->remote_pole_positions_location = glGetUniformLocation(engine->program, "u_remote_pole_positions[0]");
    engine->holomorphic_coefficients_location = glGetUniformLocation(
        engine->program, "u_holomorphic_coefficients[0]"
    );
    engine->divisor_count_location = glGetUniformLocation(engine->program, "u_divisor_count");
    engine->divisor_factors_location = glGetUniformLocation(engine->program, "u_divisor_factors[0]");
    engine->remote_pole_time_location = glGetUniformLocation(engine->program, "u_remote_pole_time");
    engine->numeric_output_location = glGetUniformLocation(engine->program, "u_numeric_output");
    engine->numeric_point_location = glGetUniformLocation(engine->program, "u_numeric_point");
    engine->zoom_location = glGetUniformLocation(engine->program, "u_zoom");
    engine->placement_kind_location = glGetUniformLocation(engine->program, "u_placement_kind");

    if (
        engine->resolution_location < 0 || engine->zero_count_location < 0 ||
        engine->pole_count_location < 0 || engine->zero_positions_location < 0 ||
        engine->pole_positions_location < 0 ||
        engine->holomorphic_coefficients_location < 0 ||
        (HOLOMORPHIC_REQUIRE_GENERATED_SHADER &&
            (engine->divisor_count_location < 0 || engine->divisor_factors_location < 0 ||
             engine->remote_pole_positions_location < 0 ||
             engine->numeric_output_location < 0 || engine->numeric_point_location < 0)) ||
        (!HOLOMORPHIC_REQUIRE_GENERATED_SHADER && engine->remote_pole_time_location < 0) ||
        engine->zoom_location < 0 || engine->placement_kind_location < 0
    ) {
        LOGE("holomorphic field shader uniforms unavailable");
        return false;
    }

    if (HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        LOGI("holomorphic field GLSL backend selected: compiler=idris2-glsles fallback=none fragment_sha256=%s",
             HOLOMORPHIC_FRAGMENT_SHA256);
    } else {
        LOGI("holomorphic field GLSL backend selected: legacy-handwritten");
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
        "holomorphic field renderer ready: GL_VERSION=%s GL_RENDERER=%s GL_VENDOR=%s",
        glGetString(GL_VERSION), glGetString(GL_RENDERER), glGetString(GL_VENDOR)
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

/* Use the same selected live program and divisor upload path. RGB screenshots
 * are never decoded as numbers. Unsupported float attachments remain NOT_RUN.
 * Independent AICI owns the binary64 expected values and tolerances. */
static void run_numeric_probes(struct engine *engine) {
    if (!HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        LOGI("HOLO_GPU_NUMERIC status=NOT_RUN reason=legacy-mode");
        return;
    }
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    if (extensions == NULL || strstr(extensions, "GL_EXT_color_buffer_float") == NULL) {
        LOGI("HOLO_GPU_NUMERIC status=NOT_RUN reason=unsupported-float-color-buffer");
        return;
    }
    GLint previous_framebuffer = 0, viewport[4] = {0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    GLuint framebuffer = 0, texture = 0, color_texture = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, NULL);
    glGenTextures(1, &color_texture);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGI("HOLO_GPU_NUMERIC status=NOT_RUN reason=incomplete-float-framebuffer");
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        glDeleteTextures(1, &color_texture);
        return;
    }
    static const float coefficients[5][2] = {
        {0.125f,-0.0625f}, {-0.03125f,0.015625f}, {0.0078125f,0.00390625f},
        {-0.001953125f,0.0009765625f}, {0.00048828125f,-0.000244140625f}
    };
    static const float points[4][2] = {
        {0.125f,0.1875f}, {0.25f,-0.375f}, {-0.75f,-0.5f}, {0.3125f,-0.40625f}
    };
    static const char *case_names[] = {
        "one_exact", "one_displaced", "24_exact", "24_displaced", "multiplicity",
        "moving0", "moving7_5", "moving7_5_zoom", "24_displaced_large", "24_displaced_small"
    };
    GLboolean previous_dither = glIsEnabled(GL_DITHER);
    glDisable(GL_DITHER);
    glViewport(0, 0, 1, 1);
    glUseProgram(engine->program);
    glBindVertexArray(engine->vao);
    glUniform1i(engine->numeric_output_location, 1);
    glUniform2f(engine->resolution_location, 1.0f, 1.0f);
    glUniform2fv(engine->holomorphic_coefficients_location, 5, &coefficients[0][0]);
    int produced_samples = 0;
    int produced_color_samples = 0;
    bool valid = true;
    for (int case_index = 0; case_index < 10 && valid; ++case_index) {
        float zeros[32][2] = {{0}}, poles[32][2] = {{0}};
        int zero_count = 1, pole_count = 1, remote_count = 0;
        double time = case_index == 6 || case_index == 7 ? 7.5 : 0.0;
        zeros[0][0] = poles[0][0] = 0.25f;
        zeros[0][1] = poles[0][1] = -0.375f;
        if (case_index == 1) {
            poles[0][0] += 0.0625f; poles[0][1] -= 0.03125f;
        } else if (case_index == 2 || case_index == 3 || case_index >= 8) {
            zero_count = pole_count = 24;
            float scale = case_index == 8 ? 0x1p60f : 0x1p-60f;
            for (int index = 0; index < 24; ++index) {
                zeros[index][0] = case_index >= 8 ? scale * (1.0f + index * 0.0625f) : -0.75f + index * 0.0625f;
                zeros[index][1] = case_index >= 8 ? scale * (-1.0f - index * 0.03125f) : -0.5f + index * 0.03125f;
                poles[index][0] = zeros[index][0] + (case_index == 2 ? 0.0f : (case_index >= 8 ? scale : 1.0f) * 0.015625f);
                poles[index][1] = zeros[index][1] - (case_index == 2 ? 0.0f : (case_index >= 8 ? scale : 1.0f) * 0.0078125f);
            }
        } else if (case_index == 4) {
            zero_count = 3;
            memcpy(zeros[1], zeros[0], sizeof(zeros[0]));
            memcpy(zeros[2], zeros[0], sizeof(zeros[0]));
        } else if (case_index == 5 || case_index == 6 || case_index == 7) {
            zeros[0][0] = -0.34375f; zeros[0][1] = 0.0f;
            poles[0][0] = 0.34375f; poles[0][1] = 0.0f;
            remote_count = 24;
        }
        float remote_positions[HOLOMORPHIC_REMOTE_POLE_COUNT][2] = {{0}};
        for (int index = 0; index < remote_count; ++index) {
            if (!holomorphic_remote_pole(index, time, remote_positions[index])) valid = false;
        }
        struct holomorphic_divisor divisor;
        if (!valid || !holomorphic_divisor_reduce_positions(
                &divisor, zero_count, zeros, pole_count, poles, remote_count, remote_positions)) {
            valid = false;
            break;
        }
        LOGI("HOLO_GPU_CASE case=%s coefficient_count=5 zero_count=%d pole_count=%d remote_count=%d factor_count=%d time=%.9g",
            case_names[case_index], zero_count, pole_count, remote_count, divisor.count, time);
        for (int index = 0; index < 5; ++index)
            LOGI("HOLO_GPU_COEFFICIENT case=%s index=%d real=%.9g imag=%.9g",
                case_names[case_index], index, coefficients[index][0], coefficients[index][1]);
        for (int index = 0; index < zero_count; ++index)
            LOGI("HOLO_GPU_INPUT case=%s kind=zero index=%d x=%.9g y=%.9g",
                case_names[case_index], index, zeros[index][0], zeros[index][1]);
        for (int index = 0; index < pole_count; ++index)
            LOGI("HOLO_GPU_INPUT case=%s kind=pole index=%d x=%.9g y=%.9g",
                case_names[case_index], index, poles[index][0], poles[index][1]);
        for (int index = 0; index < remote_count; ++index) {
            LOGI("HOLO_GPU_INPUT case=%s kind=remote index=%d x=%.9g y=%.9g",
                case_names[case_index], index, remote_positions[index][0], remote_positions[index][1]);
        }
        for (int index = 0; index < divisor.count; ++index)
            LOGI("HOLO_GPU_FACTOR case=%s index=%d x=%.9g y=%.9g order=%.9g",
                case_names[case_index], index, divisor.factors[index][0],
                divisor.factors[index][1], divisor.factors[index][2]);
        glUniform1i(engine->zero_count_location, zero_count);
        glUniform1i(engine->pole_count_location, pole_count);
        glUniform2fv(engine->zero_positions_location, 32, &zeros[0][0]);
        glUniform2fv(engine->pole_positions_location, 32, &poles[0][0]);
        glUniform2fv(engine->remote_pole_positions_location,
            HOLOMORPHIC_REMOTE_POLE_COUNT, &remote_positions[0][0]);
        glUniform1i(engine->divisor_count_location, divisor.count);
        glUniform3fv(engine->divisor_factors_location, divisor.count, &divisor.factors[0][0]);
        int sample_count = case_index >= 8 ? 1 : 4;
        for (int sample = 0; sample < sample_count && valid; ++sample) {
            float point_x = case_index >= 8 ? 0.0f : points[sample][0];
            float point_y = case_index >= 8 ? 0.0f : points[sample][1];
            float zoom = case_index == 7 ? 5.5f : sample == 0 ? 0.5f : sample == 1 ? 1.0f : 3.0f;
            glUniform2f(engine->numeric_point_location, point_x, point_y);
            glUniform1f(engine->zoom_location, zoom);
            const GLfloat missing_sample[4] = {NAN, NAN, NAN, 0.0f};
            glClearBufferfv(GL_COLOR, 0, missing_sample);
            double start = monotonic_seconds();
            glDrawArrays(GL_TRIANGLES, 0, 3);
            GLfloat result[4] = {NAN, NAN, NAN, 0.0f};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, result);
            GLenum error = glGetError();
            double readback_cpu_ms = (monotonic_seconds() - start) * 1000.0;
            valid = error == GL_NO_ERROR && isfinite(result[0]) && isfinite(result[1]) &&
                isfinite(result[2]) && result[3] == 1.0f;
            LOGI("HOLO_GPU_MEASURE case=%s sample=%d point_x=%.9g point_y=%.9g time=%.9g zoom=%.9g zero_count=%d pole_count=%d remote_count=%d phase=%.9g log_modulus=%.9g singular_order=%.9g produced=%.9g gl_error=%u readback_cpu_ms=%.9g",
                case_names[case_index], sample, point_x, point_y, time, zoom,
                zero_count, pole_count, remote_count, result[0], result[1], result[2], result[3],
                (unsigned int)error, readback_cpu_ms);
            if (valid) ++produced_samples;
            if (valid) {
                /* The second diagnostic mode is the canonical colour result
                 * before overlays in the exact same selected fragment. The
                 * colour bytes are read from an RGBA8 attachment, never a
                 * separate shader or a reconstruction from the raw numbers. */
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
                glUniform1i(engine->numeric_output_location, 2);
                const GLfloat empty_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                glClearBufferfv(GL_COLOR, 0, empty_color);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                GLubyte color[4] = {0, 0, 0, 0};
                glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
                GLenum color_error = glGetError();
                valid = color_error == GL_NO_ERROR && color[3] == 255;
                LOGI("HOLO_GPU_COLOR case=%s sample=%d red=%u green=%u blue=%u alpha=%u produced=%d gl_error=%u output_mode=2 framebuffer_format=RGBA8 conversion=glReadPixels",
                    case_names[case_index], sample, color[0], color[1], color[2], color[3],
                    valid ? 1 : 0, (unsigned int)color_error);
                if (valid) ++produced_color_samples;
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                glUniform1i(engine->numeric_output_location, 1);
            }
        }
    }
    glUniform1i(engine->numeric_output_location, 0);
    if (previous_dither) glEnable(GL_DITHER);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
    glDeleteTextures(1, &color_texture);
    LOGI("HOLO_GPU_NUMERIC status=%s samples=%d expected=34 reference_acceptance=NOT_RUN",
        valid && produced_samples == 34 ? "OUTPUT_READY" : "FAIL", produced_samples);
    LOGI("HOLO_GPU_COLOR_OUTPUT status=%s samples=%d expected=34 reference_acceptance=NOT_RUN",
        valid && produced_color_samples == 34 ? "OUTPUT_READY" : "FAIL", produced_color_samples);
}

static void draw_frame(struct engine *engine) {
    if (
        engine->display == EGL_NO_DISPLAY || engine->program == 0 ||
        engine->width <= 0 || engine->height <= 0
    ) {
        return;
    }

    if (!engine->numeric_probe_attempted) {
        engine->numeric_probe_attempted = true;
        run_numeric_probes(engine);
    }

    struct holomorphic_divisor divisor;
    for (int index = 0; index < HOLOMORPHIC_REMOTE_POLE_COUNT; ++index) {
        if (!holomorphic_remote_pole(index, (double)engine->remote_pole_time,
                engine->remote_pole_positions[index])) {
            LOGE("holomorphic remote-pole geometry rejected: invalid input");
            return;
        }
    }
    if (!holomorphic_divisor_reduce_positions(
            &divisor, engine->zero_count, engine->zero_positions,
            engine->pole_count, engine->pole_positions,
            HOLOMORPHIC_REMOTE_POLE_COUNT, engine->remote_pole_positions)) {
        LOGE("holomorphic divisor reduction rejected: invalid input or capacity overflow");
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
    if (HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        glUniform2fv(engine->remote_pole_positions_location,
            HOLOMORPHIC_REMOTE_POLE_COUNT, &engine->remote_pole_positions[0][0]);
        glUniform1i(engine->divisor_count_location, divisor.count);
        glUniform3fv(engine->divisor_factors_location, divisor.count, &divisor.factors[0][0]);
        glUniform1i(engine->numeric_output_location, 0);
    } else {
        glUniform1f(engine->remote_pole_time_location, engine->remote_pole_time);
    }
    glUniform1f(engine->zoom_location, engine->zoom);
    glUniform1i(engine->placement_kind_location, (int)engine->placement_kind);

    glBindVertexArray(engine->vao);
    double submission_start = monotonic_seconds();
    if (engine->timing_window_start == 0.0) engine->timing_window_start = submission_start;
    glDrawArrays(GL_TRIANGLES, 0, 3);
    engine->timing_submission_cpu_ms += (monotonic_seconds() - submission_start) * 1000.0;

    if (!engine->logged_first_frame) {
        GLubyte center_pixel[4] = {0, 0, 0, 0};
        glReadPixels(
            engine->width ÷ 2, engine->height ÷ 2, 1, 1,
            GL_RGBA, GL_UNSIGNED_BYTE, center_pixel
        );
        LOGI(
            "holomorphic field first frame: center rgba=%u,%u,%u,%u",
            center_pixel[0], center_pixel[1], center_pixel[2], center_pixel[3]
        );
        engine->logged_first_frame = true;
    }

    double presentation_start = monotonic_seconds();
    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    double presentation_end = monotonic_seconds();
    engine->timing_presentation_cpu_ms += (presentation_end - presentation_start) * 1000.0;
    ++engine->timing_frames;
    if (engine->timing_frames == 60) {
        LOGI("HOLO_FRAME_TIMING frames=60 zero_count=%d pole_count=%d remote_count=24 reduced_factor_count=%d "
             "draw_submission_cpu_wall_ms=%.9g egl_presentation_cpu_wall_ms=%.9g frame_interval_wall_ms=%.9g "
             "gpu_execution_ms=NOT_RUN cpu_search_ms=NOT_MEASURED thermal=NOT_RUN",
            engine->zero_count, engine->pole_count, divisor.count,
            engine->timing_submission_cpu_ms ÷ 60.0,
            engine->timing_presentation_cpu_ms ÷ 60.0,
            (presentation_end - engine->timing_window_start) * 1000.0 ÷ 60.0);
        engine->timing_frames = 0;
        engine->timing_submission_cpu_ms = 0.0;
        engine->timing_presentation_cpu_ms = 0.0;
        engine->timing_window_start = 0.0;
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
    output[0] = (x - 0.5f * (float)engine->width) ÷ scale;
    output[1] = (0.5f * (float)engine->height - y) ÷ scale;
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

    float zoom = engine->pinch_start_zoom * distance ÷ engine->pinch_start_distance;
    if (zoom < 0.5f) zoom = 0.5f;
    if (zoom > 4.0f) zoom = 4.0f;
    if (fabsf(zoom - engine->zoom) > 1.0e-4f) {
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
