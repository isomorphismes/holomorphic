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

#define LOG_TAG "AnalyticContinuation"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define MAX_FACTORS 64

#include "continuation_path.h"
#include "factor_snap.h"
#include "factor_state.h"
#include "gesture_state.h"
#include "perturbation_render.h"

static const char *PLACEMENT_CONTROL_FRAGMENT_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_ndc;\n"
    "uniform vec2 u_resolution;\n"
    "uniform int u_placement_kind;\n"
    "out vec4 out_color;\n"
    "float circle_mask(vec2 point, vec2 center, float radius) {\n"
    "    return 1.0 - smoothstep(radius - 1.5, radius + 1.5, length(point - center));\n"
    "}\n"
    "float line_mask(vec2 point, vec2 start, vec2 finish, float half_width) {\n"
    "    vec2 segment = finish - start;\n"
    "    float along = clamp(dot(point - start, segment) / dot(segment, segment), 0.0, 1.0);\n"
    "    float distance_to_line = length(point - (start + along * segment));\n"
    "    return 1.0 - smoothstep(half_width - 1.0, half_width + 1.0, distance_to_line);\n"
    "}\n"
    "vec4 button(vec2 point, vec2 center, bool selected, bool infinity_point) {\n"
    "    float radius = clamp(min(u_resolution.x, u_resolution.y) * 0.065, 36.0, 56.0);\n"
    "    float disk = circle_mask(point, center, radius);\n"
    "    float rim = circle_mask(point, center, radius) - circle_mask(point, center, radius - 3.0);\n"
    "    vec4 background = selected ? vec4(0.96, 0.96, 0.93, 0.94) : vec4(0.05, 0.05, 0.05, 0.72);\n"
    "    vec3 mark_color = selected ? vec3(0.05) : vec3(0.96);\n"
    "    float mark = 0.0;\n"
    "    if (infinity_point) {\n"
    "        float loop_radius = radius * 0.26;\n"
    "        float loop_offset = radius * 0.18;\n"
    "        float left_outer = circle_mask(point, center - vec2(loop_offset, 0.0), loop_radius);\n"
    "        float left_inner = circle_mask(point, center - vec2(loop_offset, 0.0), loop_radius * 0.58);\n"
    "        float right_outer = circle_mask(point, center + vec2(loop_offset, 0.0), loop_radius);\n"
    "        float right_inner = circle_mask(point, center + vec2(loop_offset, 0.0), loop_radius * 0.58);\n"
    "        mark = max(left_outer - left_inner, right_outer - right_inner);\n"
    "    } else {\n"
    "        float outer = circle_mask(point, center, radius * 0.40);\n"
    "        float inner = circle_mask(point, center, radius * 0.27);\n"
    "        mark = outer - inner;\n"
    "    }\n"
    "    vec4 color = background * disk;\n"
    "    color.rgb = mix(color.rgb, vec3(0.96), rim);\n"
    "    color.rgb = mix(color.rgb, mark_color, mark);\n"
    "    color.a = max(color.a, max(rim, mark));\n"
    "    return color;\n"
    "}\n"
    "void main() {\n"
    "    vec2 point = gl_FragCoord.xy;\n"
    "    float radius = clamp(min(u_resolution.x, u_resolution.y) * 0.065, 36.0, 56.0);\n"
    "    float margin = max(18.0, radius * 0.38);\n"
    "    vec2 zero_center = vec2(margin + radius, margin + radius);\n"
    "    vec2 pole_center = zero_center + vec2(2.0 * radius + margin * 0.55, 0.0);\n"
    "    vec4 zero_button = button(point, zero_center, u_placement_kind == 0, false);\n"
    "    vec4 pole_button = button(point, pole_center, u_placement_kind == 1, true);\n"
    "    out_color = zero_button.a >= pole_button.a ? zero_button : pole_button;\n"
    "}\n";

static const char *VERTEX_SHADER =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec2 a_position;\n"
    "out vec2 v_ndc;\n"
    "void main() {\n"
    "    v_ndc = a_position;\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

enum factor_kind {
    FACTOR_ZERO,
    FACTOR_POLE
};

enum view_kind {
    VIEW_WHOLE_PORTRAIT,
    VIEW_CONTINUATION
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
    GLint center_location;
    GLint half_height_location;
    GLint aspect_location;
    GLint resolution_location;
    GLint zero_count_location;
    GLint pole_count_location;
    GLint zeros_location;
    GLint poles_location;
    GLint view_kind_location;
    GLint continuation_count_location;
    GLint continuation_centers_location;
    GLint continuation_radii_location;

    GLuint placement_program;
    GLint placement_resolution_location;
    GLint placement_kind_location;

    GLuint overlay_program;
    GLuint overlay_texture;
    GLuint clear_button_texture;
    GLuint view_button_texture;
    GLint overlay_resolution_location;
    GLint overlay_origin_location;
    GLint overlay_size_location;
    GLint overlay_sampler_location;
    int overlay_width;
    int overlay_height;
    int clear_button_width;
    int clear_button_height;
    int view_button_width;
    int view_button_height;
    bool overlay_dirty;
    bool view_button_dirty;
    bool overlay_unavailable;

    float center[2];
    float half_height;
    float zeros[MAX_FACTORS][2];
    float poles[MAX_FACTORS][2];
    int zero_count;
    int pole_count;
    enum factor_kind placement_kind;
    enum view_kind view_kind;
    struct continuation_path continuation;

    enum gesture_kind gesture;
    bool moved;
    float down_x;
    float down_y;
    float last_x;
    float last_y;
    float pinch_last_distance;
    float pinch_last_mid_x;
    float pinch_last_mid_y;

    bool dirty;
    bool logged_first_frame;
};

static void placement_control_centers(
    const struct engine *engine,
    float *zero_x,
    float *pole_x,
    float *center_y
);

static void clear_continuation_path(struct engine *engine) {
    continuation_path_clear(&engine->continuation);
    engine->dirty = true;
}

static void set_whole_portrait_view(struct engine *engine) {
    if (engine->view_kind == VIEW_WHOLE_PORTRAIT) {
        return;
    }
    engine->view_kind = VIEW_WHOLE_PORTRAIT;
    engine->view_button_dirty = true;
    engine->dirty = true;
    LOGI("whole portrait view enabled");
}

static void set_continuation_view(struct engine *engine) {
    if (engine->view_kind == VIEW_CONTINUATION) {
        return;
    }

    if (engine->continuation.count == 0) {
        if (continuation_path_seed(
                &engine->continuation,
                engine->center[0],
                engine->center[1],
                engine->zeros,
                engine->zero_count,
                engine->poles,
                engine->pole_count
            )) {
            LOGI(
                "continuation seeded at %.6g%+.6gi radius=%.9g",
                engine->center[0],
                engine->center[1],
                engine->continuation.radii[0]
            );
        } else {
            LOGI("continuation seed rejected: camera center is an uncancelled pole");
        }
    }

    engine->view_kind = VIEW_CONTINUATION;
    engine->view_button_dirty = true;
    engine->dirty = true;
    LOGI("continuation view enabled");
}

static void toggle_view(struct engine *engine) {
    if (engine->view_kind == VIEW_CONTINUATION) {
        set_whole_portrait_view(engine);
    } else {
        set_continuation_view(engine);
    }
}

static void initialize_function(struct engine *engine) {
    engine->center[0] = 0.0f;
    engine->center[1] = 0.0f;
    engine->half_height = 3.5f;

    // Start with a genuinely non-polynomial rational map:
    //                  z + 1
    //         g(z) = ---------
    //                  z - 1
    // The finite pole at 1 gives the camera-center Taylor disc radius 1.
    engine->zero_count = 1;
    engine->zeros[0][0] = -1.0f;
    engine->zeros[0][1] = 0.0f;
    engine->pole_count = 1;
    engine->poles[0][0] = 1.0f;
    engine->poles[0][1] = 0.0f;
    engine->placement_kind = FACTOR_ZERO;
    engine->view_kind = VIEW_WHOLE_PORTRAIT;
    continuation_path_clear(&engine->continuation);
    engine->overlay_dirty = true;
    engine->view_button_dirty = true;
    engine->dirty = true;
    set_continuation_view(engine);
}

static void reset_all(struct engine *engine) {
    initialize_function(engine);
    LOGI("default function, camera, view, and continuation path reset");
}

static void clear_function(struct engine *engine) {
    engine->zero_count = 0;
    engine->pole_count = 0;
    continuation_path_clear(&engine->continuation);
    engine->overlay_dirty = true;
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

#include "polynomial_overlay.h"

static bool create_renderer(struct engine *engine) {
    static const GLfloat fullscreen_triangle[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f
    };

    char *fragment_source = load_asset_text(
        engine->app->activity->assetManager,
        "continuation.frag"
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

    engine->center_location = glGetUniformLocation(engine->program, "u_center");
    engine->half_height_location = glGetUniformLocation(engine->program, "u_half_height");
    engine->aspect_location = glGetUniformLocation(engine->program, "u_aspect");
    engine->resolution_location = glGetUniformLocation(engine->program, "u_resolution");
    engine->zero_count_location = glGetUniformLocation(engine->program, "u_zero_count");
    engine->pole_count_location = glGetUniformLocation(engine->program, "u_pole_count");
    engine->zeros_location = glGetUniformLocation(engine->program, "u_zeros[0]");
    engine->poles_location = glGetUniformLocation(engine->program, "u_poles[0]");
    engine->view_kind_location = glGetUniformLocation(engine->program, "u_view_kind");
    engine->continuation_count_location = glGetUniformLocation(
        engine->program,
        "u_continuation_count"
    );
    engine->continuation_centers_location = glGetUniformLocation(
        engine->program,
        "u_continuation_centers[0]"
    );
    engine->continuation_radii_location = glGetUniformLocation(
        engine->program,
        "u_continuation_radii[0]"
    );
    if (!perturbation_render_bind(engine->program)) {
        LOGE("perturbation workers unavailable");
    }

    GLuint placement_vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER);
    GLuint placement_fragment_shader = compile_shader(
        GL_FRAGMENT_SHADER,
        PLACEMENT_CONTROL_FRAGMENT_SHADER
    );
    if (placement_vertex_shader != 0 && placement_fragment_shader != 0) {
        engine->placement_program = link_program(
            placement_vertex_shader,
            placement_fragment_shader
        );
    }
    if (placement_vertex_shader != 0) glDeleteShader(placement_vertex_shader);
    if (placement_fragment_shader != 0) glDeleteShader(placement_fragment_shader);
    if (engine->placement_program == 0) {
        LOGE("placement controls unavailable");
    } else {
        engine->placement_resolution_location = glGetUniformLocation(
            engine->placement_program,
            "u_resolution"
        );
        engine->placement_kind_location = glGetUniformLocation(
            engine->placement_program,
            "u_placement_kind"
        );
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

    LOGI("renderer ready: GL_VERSION=%s GL_RENDERER=%s program=%u vao=%u vbo=%u uniforms=%d,%d,%d,%d,%d,%d,%d,%d",
         glGetString(GL_VERSION), glGetString(GL_RENDERER), engine->program, engine->vao, engine->vbo,
         engine->center_location, engine->half_height_location, engine->aspect_location,
         engine->resolution_location, engine->zero_count_location, engine->pole_count_location,
         engine->zeros_location, engine->poles_location);
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
    LOGI("EGL surface ready: %dx%d", engine->width, engine->height);

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
    engine->overlay_dirty = true;
    engine->dirty = true;
    return true;
}

static void terminate_display(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY) {
        return;
    }

    polynomial_overlay_destroy(engine);
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
    if (engine->placement_program != 0) {
        glDeleteProgram(engine->placement_program);
        engine->placement_program = 0;
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
    engine->overlay_dirty = true;
    engine->dirty = true;
}

static void draw_frame(struct engine *engine) {
    if (engine->display == EGL_NO_DISPLAY || engine->program == 0 || engine->width <= 0 || engine->height <= 0) {
        return;
    }

    float aspect = (float)engine->width / (float)engine->height;

    glUseProgram(engine->program);
    glUniform2f(engine->center_location, engine->center[0], engine->center[1]);
    glUniform1f(engine->half_height_location, engine->half_height);
    glUniform1f(engine->aspect_location, aspect);
    glUniform2f(engine->resolution_location, (float)engine->width, (float)engine->height);
    glUniform1i(engine->zero_count_location, engine->zero_count);
    glUniform1i(engine->pole_count_location, engine->pole_count);
    glUniform2fv(engine->zeros_location, MAX_FACTORS, &engine->zeros[0][0]);
    glUniform2fv(engine->poles_location, MAX_FACTORS, &engine->poles[0][0]);
    glUniform1i(engine->view_kind_location, (int)engine->view_kind);
    glUniform1i(engine->continuation_count_location, engine->continuation.count);
    glUniform2fv(
        engine->continuation_centers_location,
        MAX_CONTINUATION_STEPS,
        &engine->continuation.centers[0][0]
    );
    glUniform1fv(
        engine->continuation_radii_location,
        MAX_CONTINUATION_STEPS,
        engine->continuation.radii
    );
    perturbation_render_upload(
        engine->center[0],
        engine->center[1],
        engine->half_height,
        aspect
    );

    glBindVertexArray(engine->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    polynomial_overlay_draw(engine);

    if (engine->placement_program != 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(engine->placement_program);
        glUniform2f(
            engine->placement_resolution_location,
            (float)engine->width,
            (float)engine->height
        );
        glUniform1i(engine->placement_kind_location, (int)engine->placement_kind);
        glBindVertexArray(engine->vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_BLEND);
    }

    if (!engine->logged_first_frame) {
        float zero_control_x = 0.0f;
        float pole_control_x = 0.0f;
        float placement_control_y = 0.0f;
        placement_control_centers(
            engine,
            &zero_control_x,
            &pole_control_x,
            &placement_control_y
        );
        LOGI(
            "placement control centers: zero=%d,%d infinity=%d,%d",
            (int)zero_control_x,
            (int)placement_control_y,
            (int)pole_control_x,
            (int)placement_control_y
        );

        GLubyte pixel[4] = {0, 0, 0, 0};
        glReadPixels(engine->width / 2, engine->height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        GLenum error = glGetError();
        LOGI("first frame: center rgba=%u,%u,%u,%u glError=0x%x",
             pixel[0], pixel[1], pixel[2], pixel[3], error);
        engine->logged_first_frame = true;
    }

    if (!eglSwapBuffers(engine->display, engine->surface)) {
        LOGE("eglSwapBuffers failed: 0x%x", eglGetError());
    }
    engine->dirty = false;
}

static void screen_to_complex(const struct engine *engine, float x, float y, float output[2]) {
    float width = (float)engine->width;
    float height = (float)engine->height;
    float aspect = width / height;
    float ndc_x = 2.0f * x / width - 1.0f;
    float ndc_y = 1.0f - 2.0f * y / height;

    output[0] = engine->center[0] + ndc_x * engine->half_height * aspect;
    output[1] = engine->center[1] + ndc_y * engine->half_height;
}

static float factor_touch_radius_pixels(const struct engine *engine) {
    int density = AConfiguration_getDensity(engine->app->config);
    if (density < 72 || density > 1000) {
        density = 160;
    }

    float radius = 24.0f * (float)density / 160.0f;
    if (radius < 24.0f) radius = 24.0f;
    if (radius > 72.0f) radius = 72.0f;
    return radius;
}

static void snap_touch_to_factors(
    const struct engine *engine,
    float point[2],
    float factors[MAX_FACTORS][2],
    int factor_count
) {
    float world_per_pixel = 2.0f * engine->half_height / (float)engine->height;
    factor_snap_to_nearest(
        point,
        factors,
        factor_count,
        world_per_pixel,
        factor_touch_radius_pixels(engine)
    );
}

static void pan_by_pixels(struct engine *engine, float delta_x, float delta_y) {
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }
    float aspect = (float)engine->width / (float)engine->height;
    engine->center[0] -= 2.0f * delta_x * engine->half_height * aspect / (float)engine->width;
    engine->center[1] += 2.0f * delta_y * engine->half_height / (float)engine->height;
    engine->dirty = true;
}

static void add_zero(struct engine *engine, float x, float y) {
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    float factor[2];
    screen_to_complex(engine, x, y, factor);
    snap_touch_to_factors(engine, factor, engine->poles, engine->pole_count);
    enum factor_change change = factor_insert_reduced(
        engine->zeros,
        &engine->zero_count,
        engine->poles,
        &engine->pole_count,
        factor[0],
        factor[1]
    );
    if (change == FACTOR_UNCHANGED) {
        return;
    }

    continuation_path_clear(&engine->continuation);
    engine->overlay_dirty = true;
    engine->dirty = true;
}

static void add_pole(struct engine *engine, float x, float y) {
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    float factor[2];
    screen_to_complex(engine, x, y, factor);
    snap_touch_to_factors(engine, factor, engine->zeros, engine->zero_count);
    enum factor_change change = factor_insert_reduced(
        engine->poles,
        &engine->pole_count,
        engine->zeros,
        &engine->zero_count,
        factor[0],
        factor[1]
    );
    if (change == FACTOR_UNCHANGED) {
        return;
    }

    continuation_path_clear(&engine->continuation);
    engine->overlay_dirty = true;
    engine->dirty = true;
}

static void add_continuation_center(struct engine *engine, float x, float y) {
    if (engine->width <= 0 || engine->height <= 0) {
        return;
    }

    float center[2];
    screen_to_complex(engine, x, y, center);
    snap_touch_to_factors(engine, center, engine->poles, engine->pole_count);
    bool accepted = continuation_path_add_center(
        &engine->continuation,
        center[0],
        center[1],
        engine->zeros,
        engine->zero_count,
        engine->poles,
        engine->pole_count
    );
    if (accepted) {
        int step = engine->continuation.count - 1;
        if (engine->continuation.count == 1) {
            LOGI(
                "continuation seed added: center=%.6g%+.6gi radius=%.6g",
                center[0],
                center[1],
                engine->continuation.radii[step]
            );
        } else {
            LOGI(
                "continuation step added: center=%.6g%+.6gi radius=%.6g",
                center[0],
                center[1],
                engine->continuation.radii[step]
            );
        }
        engine->dirty = true;
    } else {
        if (engine->continuation.count == 0) {
            LOGI("continuation seed rejected: tap is an uncancelled pole");
        } else {
            LOGI("continuation step rejected: tap must be inside the preceding Taylor disc");
        }
    }
}

static float placement_control_radius(const struct engine *engine) {
    float radius = 0.065f * fminf((float)engine->width, (float)engine->height);
    if (radius < 36.0f) radius = 36.0f;
    if (radius > 56.0f) radius = 56.0f;
    return radius;
}

static void placement_control_centers(
    const struct engine *engine,
    float *zero_x,
    float *pole_x,
    float *center_y
) {
    float radius = placement_control_radius(engine);
    float margin = fmaxf(18.0f, radius * 0.38f);
    *zero_x = margin + radius;
    *pole_x = *zero_x + 2.0f * radius + margin * 0.55f;
    *center_y = (float)engine->height - margin - radius;
}

static bool placement_control_hit(
    const struct engine *engine,
    float x,
    float y,
    enum factor_kind *kind
) {
    if (engine->width <= 0 || engine->height <= 0) {
        return false;
    }

    float radius = placement_control_radius(engine);
    float zero_center_x = 0.0f;
    float pole_center_x = 0.0f;
    float center_y = 0.0f;
    placement_control_centers(
        engine,
        &zero_center_x,
        &pole_center_x,
        &center_y
    );

    if (hypotf(x - zero_center_x, y - center_y) <= radius) {
        *kind = FACTOR_ZERO;
        return true;
    }
    if (hypotf(x - pole_center_x, y - center_y) <= radius) {
        *kind = FACTOR_POLE;
        return true;
    }
    return false;
}

static float pointer_distance(const AInputEvent *event) {
    float dx = AMotionEvent_getX(event, 0) - AMotionEvent_getX(event, 1);
    float dy = AMotionEvent_getY(event, 0) - AMotionEvent_getY(event, 1);
    return hypotf(dx, dy);
}

static void pointer_midpoint(const AInputEvent *event, float *x, float *y) {
    *x = 0.5f * (AMotionEvent_getX(event, 0) + AMotionEvent_getX(event, 1));
    *y = 0.5f * (AMotionEvent_getY(event, 0) + AMotionEvent_getY(event, 1));
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    struct engine *engine = app->userData;
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return 0;
    }

    int32_t action = AMotionEvent_getAction(event);
    int32_t masked_action = action & AMOTION_EVENT_ACTION_MASK;
    size_t pointer_count = AMotionEvent_getPointerCount(event);

    switch (masked_action) {
        case AMOTION_EVENT_ACTION_DOWN: {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            enum factor_kind selected_kind = FACTOR_ZERO;
            if (placement_control_hit(engine, x, y, &selected_kind)) {
                engine->placement_kind = selected_kind;
                set_whole_portrait_view(engine);
                engine->gesture = GESTURE_BLOCKED;
                engine->moved = false;
                engine->dirty = true;
                return 1;
            }
            if (view_button_contains(engine, x, y)) {
                engine->gesture = GESTURE_VIEW_BUTTON;
                engine->moved = false;
                return 1;
            }
            if (clear_button_contains(engine, x, y)) {
                engine->gesture = GESTURE_CLEAR_BUTTON;
                engine->moved = false;
                return 1;
            }
            if (polynomial_overlay_contains(engine, x, y)) {
                engine->gesture = GESTURE_BLOCKED;
                engine->moved = false;
                return 1;
            }
            engine->gesture = GESTURE_SINGLE;
            engine->moved = false;
            engine->down_x = x;
            engine->down_y = y;
            engine->last_x = engine->down_x;
            engine->last_y = engine->down_y;
            return 1;
        }

        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            if (gesture_is_ui_hold(engine->gesture)) {
                engine->gesture = GESTURE_BLOCKED;
                engine->moved = false;
                return 1;
            }
            if (gesture_pointer_down_resets(engine->gesture, (int)pointer_count)) {
                reset_all(engine);
                engine->gesture = GESTURE_BLOCKED;
                return 1;
            }
            if (gesture_pointer_down_starts_pinch(engine->gesture, (int)pointer_count)) {
                engine->gesture = GESTURE_PINCH;
                engine->moved = false;
                engine->pinch_last_distance = pointer_distance(event);
                pointer_midpoint(event, &engine->pinch_last_mid_x, &engine->pinch_last_mid_y);
                return 1;
            }
            return 0;
        }

        case AMOTION_EVENT_ACTION_MOVE: {
            if (engine->gesture == GESTURE_SINGLE && pointer_count == 1) {
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                float from_down_x = x - engine->down_x;
                float from_down_y = y - engine->down_y;
                if (hypotf(from_down_x, from_down_y) > 12.0f) {
                    engine->moved = true;
                }
                if (engine->moved) {
                    pan_by_pixels(engine, x - engine->last_x, y - engine->last_y);
                }
                engine->last_x = x;
                engine->last_y = y;
                return 1;
            }

            if (engine->gesture == GESTURE_PINCH && pointer_count >= 2) {
                float midpoint_x = 0.0f;
                float midpoint_y = 0.0f;
                pointer_midpoint(event, &midpoint_x, &midpoint_y);
                float distance = pointer_distance(event);

                pan_by_pixels(
                    engine,
                    midpoint_x - engine->pinch_last_mid_x,
                    midpoint_y - engine->pinch_last_mid_y
                );

                if (distance > 1.0f && engine->pinch_last_distance > 1.0f) {
                    engine->half_height *= engine->pinch_last_distance / distance;
                    if (engine->half_height < 0.01f) engine->half_height = 0.01f;
                    if (engine->half_height > 100000.0f) engine->half_height = 100000.0f;
                    engine->dirty = true;
                }

                engine->pinch_last_distance = distance;
                engine->pinch_last_mid_x = midpoint_x;
                engine->pinch_last_mid_y = midpoint_y;
                return 1;
            }
            return gesture_is_ui_hold(engine->gesture) ? 1 : 0;
        }

        case AMOTION_EVENT_ACTION_POINTER_UP: {
            if (engine->gesture == GESTURE_PINCH && pointer_count == 2) {
                engine->gesture = GESTURE_BLOCKED;
                return 1;
            }
            return gesture_is_ui_hold(engine->gesture) ? 1 : 0;
        }

        case AMOTION_EVENT_ACTION_UP: {
            if (engine->gesture == GESTURE_SINGLE && !engine->moved) {
                float x = AMotionEvent_getX(event, 0);
                float y = AMotionEvent_getY(event, 0);
                if (engine->view_kind == VIEW_CONTINUATION) {
                    add_continuation_center(engine, x, y);
                } else if (engine->placement_kind == FACTOR_POLE) {
                    add_pole(engine, x, y);
                } else {
                    add_zero(engine, x, y);
                }
            } else if (
                engine->gesture == GESTURE_CLEAR_BUTTON &&
                clear_button_contains(
                    engine,
                    AMotionEvent_getX(event, 0),
                    AMotionEvent_getY(event, 0)
                )
            ) {
                if (engine->view_kind == VIEW_CONTINUATION) {
                    clear_continuation_path(engine);
                    LOGI("continuation path cleared");
                } else {
                    clear_function(engine);
                    LOGI("whole portrait factors cleared");
                }
            } else if (gesture_view_release_toggles(
                engine->gesture,
                view_button_contains(
                    engine,
                    AMotionEvent_getX(event, 0),
                    AMotionEvent_getY(event, 0)
                )
            )) {
                toggle_view(engine);
            }
            engine->gesture = GESTURE_NONE;
            engine->moved = false;
            return 1;
        }

        case AMOTION_EVENT_ACTION_CANCEL:
            engine->gesture = GESTURE_NONE;
            engine->moved = false;
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
        .gesture = GESTURE_NONE,
        .dirty = true,
        .logged_first_frame = false
    };
    initialize_function(&engine);

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        bool vibrating = engine.display != EGL_NO_DISPLAY
            && perturbation_render_is_running();
        int timeout = engine.dirty && engine.display != EGL_NO_DISPLAY
            ? 0
            : (vibrating ? 16 : -1);
        int ident = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (ident >= 0 && source != NULL) {
            source->process(app, source);
        }

        if (app->destroyRequested != 0) {
            perturbation_render_shutdown();
            terminate_display(&engine);
            return;
        }

        if (engine.dirty || (
                engine.display != EGL_NO_DISPLAY
                && perturbation_render_is_running()
            )) {
            draw_frame(&engine);
        }
    }
}
