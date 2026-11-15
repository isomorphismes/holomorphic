#ifndef HOLOMORPHIC_OVERLAY_QUALIFICATION_H
#define HOLOMORPHIC_OVERLAY_QUALIFICATION_H

/* Explicit qualification only. Both images come from the actual selected
 * generated program in this app, using the same frozen divisor and uniforms.
 * Include after draw_checked_scene; this does not implement another renderer. */
#define HOLO_OVERLAY_CASE_COUNT 12
#define HOLO_OVERLAY_TEST_WIDTH 192
#define HOLO_OVERLAY_TEST_HEIGHT 128

struct holo_overlay_difference {
    unsigned pixels;
    unsigned maximum;
};

static struct holo_overlay_difference holo_overlay_compare(
    const GLubyte *reference, const GLubyte *candidate, size_t pixels
) {
    struct holo_overlay_difference difference = {0, 0};
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        bool changed = false;
        for (size_t channel = 0; channel < 4; ++channel) {
            unsigned first = reference[4u * pixel + channel];
            unsigned second = candidate[4u * pixel + channel];
            unsigned error = first > second ? first - second : second - first;
            if (error != 0) changed = true;
            if (error > difference.maximum) difference.maximum = error;
        }
        if (changed) ++difference.pixels;
    }
    return difference;
}

static void holo_overlay_place(struct engine *engine, float position[2], float x, float y) {
    float shorter = engine->width < engine->height ? (float)engine->width : (float)engine->height;
    float radius = 0.42f * shorter * engine->zoom;
    position[0] = (x - 0.5f * (float)engine->width) ÷ radius;
    position[1] = (y - 0.5f * (float)engine->height) ÷ radius;
}

static bool holo_overlay_upload(struct engine *engine, struct holomorphic_divisor *divisor) {
    if (!holomorphic_divisor_reduce_positions(divisor,
            engine->zero_count, engine->zero_positions,
            engine->pole_count, engine->pole_positions,
            HOLOMORPHIC_REMOTE_POLE_COUNT, engine->remote_pole_positions)) return false;
    glUseProgram(engine->program);
    glUniform2f(engine->resolution_location, (float)engine->width, (float)engine->height);
    glUniform1i(engine->zero_count_location, engine->zero_count);
    glUniform1i(engine->pole_count_location, engine->pole_count);
    glUniform2fv(engine->zero_positions_location, MAX_FACTORS, &engine->zero_positions[0][0]);
    glUniform2fv(engine->pole_positions_location, MAX_FACTORS, &engine->pole_positions[0][0]);
    glUniform2fv(engine->remote_pole_positions_location,
        HOLOMORPHIC_REMOTE_POLE_COUNT, &engine->remote_pole_positions[0][0]);
    glUniform2fv(engine->holomorphic_coefficients_location,
        HOLOMORPHIC_WALK_COEFFICIENT_COUNT, &engine->holomorphic_coefficients[0][0]);
    glUniform1i(engine->divisor_count_location, divisor->count);
    glUniform3fv(engine->divisor_factors_location, divisor->count, &divisor->factors[0][0]);
    glUniform1f(engine->zoom_location, engine->zoom);
    glUniform1i(engine->placement_kind_location, (int)engine->placement_kind);
    glUniform1i(engine->numeric_output_location, 0);
    return glGetError() == GL_NO_ERROR;
}

static bool holo_overlay_pose(struct engine *engine, unsigned scenario) {
    memset(engine->zero_positions, 0, sizeof(engine->zero_positions));
    memset(engine->pole_positions, 0, sizeof(engine->pole_positions));
    memset(engine->holomorphic_coefficients, 0, sizeof(engine->holomorphic_coefficients));
    engine->zero_count = 1;
    engine->pole_count = 1;
    engine->zoom = scenario == 10 ? 0.12f : 1.0f;
    engine->placement_kind = scenario % 2u == 0u ? PLACEMENT_ZERO : PLACEMENT_POLE;
    engine->holomorphic_coefficients[0][0] = 0.03125f;
    engine->holomorphic_coefficients[1][1] = -0.015625f;
    engine->holomorphic_coefficients[4][0] = 0.000244140625f;
    for (int index = 0; index < HOLOMORPHIC_REMOTE_POLE_COUNT; ++index) {
        if (!holomorphic_remote_pole(index, 0.375, engine->remote_pole_positions[index])) return false;
    }
    holo_overlay_place(engine, engine->zero_positions[0], 52.25f, 80.75f);
    holo_overlay_place(engine, engine->pole_positions[0], 140.5f, 82.5f);
    if (scenario == 0) {
        holo_overlay_place(engine, engine->remote_pole_positions[0], 98.25f, 102.75f);
    } else if (scenario == 1) {
        holo_overlay_place(engine, engine->zero_positions[0], -5.25f, 70.5f);
    } else if (scenario == 2) {
        holo_overlay_place(engine, engine->pole_positions[0], 195.25f, 76.5f);
    } else if (scenario == 3) {
        holo_overlay_place(engine, engine->remote_pole_positions[0], 160.25f, -4.25f);
    } else if (scenario == 4) {
        holo_overlay_place(engine, engine->remote_pole_positions[0], 96.5f, 132.25f);
    } else if (scenario == 5) {
        holo_overlay_place(engine, engine->zero_positions[0], -4.25f, 132.25f);
    } else if (scenario == 6) {
        holo_overlay_place(engine, engine->zero_positions[0], -40.0f, 70.0f);
        holo_overlay_place(engine, engine->pole_positions[0], 240.0f, 70.0f);
    } else if (scenario == 7) {
        engine->zero_count = 2;
        holo_overlay_place(engine, engine->zero_positions[0], 95.25f, 82.5f);
        holo_overlay_place(engine, engine->zero_positions[1], 99.5f, 84.25f);
        holo_overlay_place(engine, engine->pole_positions[0], 98.25f, 82.5f);
        holo_overlay_place(engine, engine->remote_pole_positions[0], 96.25f, 86.5f);
    } else if (scenario == 8) {
        holo_overlay_place(engine, engine->zero_positions[0], 96.25f, 82.5f);
        memcpy(engine->pole_positions[0], engine->zero_positions[0], sizeof(engine->pole_positions[0]));
        memcpy(engine->remote_pole_positions[0], engine->zero_positions[0], sizeof(engine->remote_pole_positions[0]));
    } else if (scenario == 9) {
        holo_overlay_place(engine, engine->zero_positions[0], 42.0f, 42.0f);
        holo_overlay_place(engine, engine->pole_positions[0], 108.0f, 42.0f);
        holo_overlay_place(engine, engine->remote_pole_positions[0], 46.0f, 40.0f);
    } else if (scenario == 11) {
        engine->zero_count = MAX_FACTORS;
        engine->pole_count = MAX_FACTORS;
        for (int index = 0; index < MAX_FACTORS; ++index) {
            float x = -7.25f + (float)(index % 8) * 29.5f;
            float y = 10.25f + (float)(index ÷ 8) * 38.5f;
            holo_overlay_place(engine, engine->zero_positions[index], x, y);
            holo_overlay_place(engine, engine->pole_positions[index], x + 3.25f, y + 2.5f);
        }
    }
    return true;
}

static bool holo_overlay_read(struct engine *engine, bool localized, GLubyte *pixels) {
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.2f, 0.3f, 0.4f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_checked_scene(engine, localized);
    glReadPixels(0, 0, engine->width, engine->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return glGetError() == GL_NO_ERROR;
}

static bool qualify_overlay_pixels(struct engine *engine) {
    static const char *names[HOLO_OVERLAY_CASE_COUNT] = {
        "visible", "clipped-left", "clipped-right", "clipped-bottom", "clipped-top",
        "clipped-corner", "offscreen", "overlapping", "coincident", "controls-overlap",
        "zoomed-out", "many-markers"
    };
    if (!HOLOMORPHIC_REQUIRE_GENERATED_SHADER) {
        LOGE("HOLO_OVERLAY_QUALIFICATION status=FAIL reason=checked-program-required");
        return false;
    }
    struct engine saved = *engine;
    GLint previous_draw, previous_read, previous_viewport[4], previous_scissor[4];
    GLint previous_program, previous_vao, previous_pack, previous_pack_buffer;
    GLint previous_active_texture, previous_texture;
    GLfloat previous_clear[4];
    GLboolean previous_mask[4];
    GLboolean previous_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean previous_blend = glIsEnabled(GL_BLEND);
    GLboolean previous_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previous_stencil = glIsEnabled(GL_STENCIL_TEST);
    GLboolean previous_cull = glIsEnabled(GL_CULL_FACE);
    GLboolean previous_dither = glIsEnabled(GL_DITHER);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, previous_scissor);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previous_pack);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previous_pack_buffer);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previous_clear);
    glGetBooleanv(GL_COLOR_WRITEMASK, previous_mask);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    GLuint framebuffer = 0, texture = 0;
    size_t pixel_count = (size_t)HOLO_OVERLAY_TEST_WIDTH * HOLO_OVERLAY_TEST_HEIGHT;
    GLubyte *reference = malloc(4u * pixel_count);
    GLubyte *candidate = malloc(4u * pixel_count);
    bool valid = reference != NULL && candidate != NULL;
    unsigned passed = 0, rejected = 0;
    engine->width = HOLO_OVERLAY_TEST_WIDTH;
    engine->height = HOLO_OVERLAY_TEST_HEIGHT;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, engine->width, engine->height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    valid = valid && framebuffer != 0 && texture != 0
        && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glViewport(0, 0, engine->width, engine->height);
    glBindVertexArray(engine->vao);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    for (unsigned scenario = 0; valid && scenario < HOLO_OVERLAY_CASE_COUNT; ++scenario) {
        struct holomorphic_divisor divisor;
        bool rendered = holo_overlay_pose(engine, scenario) && holo_overlay_upload(engine, &divisor)
            && holo_overlay_read(engine, false, reference) && holo_overlay_read(engine, true, candidate);
        struct holo_overlay_difference difference = {0, 0};
        if (rendered) difference = holo_overlay_compare(reference, candidate, pixel_count);
        bool equal = rendered && difference.pixels == 0;
        LOGI("HOLO_OVERLAY_CASE name=%s status=%s width=%d height=%d remote_count=24 "
             "reduced_factor_count=%d mismatch_pixels=%u max_channel_error=%u comparator=exact-rgba8",
            names[scenario], equal ? "PASS" : "FAIL", engine->width, engine->height,
            rendered ? divisor.count : -1, difference.pixels, difference.maximum);
        if (equal) ++passed;
        else valid = false;
        if (equal && (scenario == 1 || scenario == 6)) {
            /* Change the actual rendered candidate, keeping the reference
             * unchanged. Missing clipped zero keeps its mathematical factor;
             * missing offscreen pole keeps every other factor and marker. */
            bool mutation_admitted = true;
            if (scenario == 1) {
                engine->zero_count = 0;
                glUniform1i(engine->zero_count_location, 0);
            } else {
                float *remote = engine->remote_pole_positions[0];
                double radius = 0.42 * (double)engine->height * (double)engine->zoom;
                double screen_x = 0.5 * (double)engine->width + radius * (double)remote[0];
                double screen_y = 0.5 * (double)engine->height + radius * (double)remote[1];
                bool offscreen = screen_x < -12.0 || screen_y < -12.0
                    || screen_x > (double)engine->width + 12.0
                    || screen_y > (double)engine->height + 12.0;
                int removed = -1;
                for (int index = 0; index < divisor.count; ++index) {
                    if (divisor.factors[index][0] == remote[0]
                            && divisor.factors[index][1] == remote[1]
                            && divisor.factors[index][2] == -1.0f) removed = index;
                }
                mutation_admitted = offscreen && removed >= 0;
                if (mutation_admitted) {
                    for (int index = removed; index + 1 < divisor.count; ++index) {
                        for (int component = 0; component < 3; ++component)
                            divisor.factors[index][component] = divisor.factors[index + 1][component];
                    }
                    --divisor.count;
                    glUniform1i(engine->divisor_count_location, divisor.count);
                    glUniform3fv(engine->divisor_factors_location, divisor.count, &divisor.factors[0][0]);
                }
            }
            rendered = mutation_admitted && holo_overlay_read(engine, true, candidate);
            difference = rendered ? holo_overlay_compare(reference, candidate, pixel_count)
                : (struct holo_overlay_difference){0, 0};
            bool refused = rendered && difference.pixels > 0;
            LOGI("HOLO_OVERLAY_MUTANT name=%s status=%s mismatch_pixels=%u max_channel_error=%u",
                scenario == 1 ? "missing-clipped-zero" : "missing-offscreen-pole",
                refused ? "REJECTED" : "FAIL", difference.pixels, difference.maximum);
            if (refused) ++rejected;
            else valid = false;
        }
    }
    *engine = saved;
    struct holomorphic_divisor restored_divisor;
    bool restored = holo_overlay_upload(engine, &restored_divisor);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previous_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)previous_read);
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glScissor(previous_scissor[0], previous_scissor[1], previous_scissor[2], previous_scissor[3]);
    if (previous_scissor_enabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (previous_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (previous_depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (previous_stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (previous_cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (previous_dither) glEnable(GL_DITHER); else glDisable(GL_DITHER);
    glColorMask(previous_mask[0], previous_mask[1], previous_mask[2], previous_mask[3]);
    glClearColor(previous_clear[0], previous_clear[1], previous_clear[2], previous_clear[3]);
    glBindVertexArray((GLuint)previous_vao);
    glUseProgram((GLuint)previous_program);
    glPixelStorei(GL_PACK_ALIGNMENT, previous_pack);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)previous_pack_buffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_texture);
    glActiveTexture((GLenum)previous_active_texture);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &texture);
    free(reference);
    free(candidate);
    valid = valid && restored && glGetError() == GL_NO_ERROR
        && passed == HOLO_OVERLAY_CASE_COUNT && rejected == 2;
    LOGI("HOLO_OVERLAY_QUALIFICATION status=%s cases=%u mutants=%u comparator=exact-rgba8 "
         "selected_program=same remote_count=24 physical_C67=NOT_RUN",
        valid ? "PASS" : "FAIL", passed, rejected);
    return valid;
}

#endif
