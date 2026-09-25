#define android_main analytic_continuation_legacy_main
#include "analytic_continuation.c"
#undef android_main

static double deformation_last_time = 0.0;

static void initialize_random_rational(struct engine *engine) {
    initialize_lasso(engine);

    engine->zero_count = 1;
    engine->zero_positions[0][0] = -0.34f;
    engine->zero_positions[0][1] = 0.0f;
    engine->zero_preimages[0][0] = -0.34f;
    engine->zero_preimages[0][1] = 0.0f;

    engine->pole_count = 1;
    engine->pole_positions[0][0] = 0.34f;
    engine->pole_positions[0][1] = 0.0f;
    engine->pole_preimages[0][0] = 0.34f;
    engine->pole_preimages[0][1] = 0.0f;

    engine->placement_kind = PLACEMENT_ZERO;
    engine->phase = 0.0f;
    engine->phase_velocity = 0.0f;
    engine->flow_time = 0.0f;
    deformation_last_time = monotonic_seconds();
}

static bool zero_control_contains(const struct engine *engine, float x, float y) {
    float radius = placement_radius(engine);
    float center_x = radius + 16.0f;
    float center_y = (float)engine->height - radius - 16.0f;
    return hypotf(x - center_x, y - center_y) <= radius;
}

static void toggle_holomorphic_pause(struct engine *engine) {
    engine->paused = !engine->paused;
    deformation_last_time = monotonic_seconds();
    engine->last_animation_time = deformation_last_time;
    engine->dirty = true;
    LOGI("holomorphic flow %s", engine->paused ? "paused" : "running");
}

static void advance_holomorphic_function(struct engine *engine) {
    double now = monotonic_seconds();
    float dt = (float)(now - deformation_last_time);
    deformation_last_time = now;
    if (dt <= 0.0f) {
        return;
    }
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    if (
        engine->paused || engine->dragging_lasso || engine->dragging_factor ||
        engine->pinching
    ) {
        return;
    }

    engine->flow_time += dt;
    engine->dirty = true;
}

static int32_t handle_zero_only_input(struct android_app *app, AInputEvent *event) {
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
            engine->placement_kind = PLACEMENT_ZERO;
            engine->suppress_tap = false;

            if (pause_control_contains(engine, x, y)) {
                toggle_holomorphic_pause(engine);
                clear_gesture(engine);
                engine->suppress_tap = true;
                return 1;
            }

            if (zero_control_contains(engine, x, y)) {
                clear_gesture(engine);
                engine->suppress_tap = true;
                LOGI("zero placement active");
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
                if (engine->lasso_candidate) {
                    begin_lasso_drag(engine);
                }
            }
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
                add_factor(engine, PLACEMENT_ZERO, x, y);
            } else if (engine->dragging_factor && engine->candidate_kind != FACTOR_NONE) {
                const char *name = engine->candidate_kind == FACTOR_ZERO ? "zero" : "infinity";
                float (*positions)[2] = engine->candidate_kind == FACTOR_ZERO
                    ? engine->zero_positions : engine->pole_positions;
                LOGI(
                    "%s moved: index=%d z=%.6g%+.6gi",
                    name,
                    engine->candidate_index,
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
    initialize_random_rational(&engine);

    LOGI("GPU-wide holomorphic flow enabled; CPU publishes only time");

    app->userData = &engine;
    app->onAppCmd = handle_command;
    app->onInputEvent = handle_zero_only_input;

    while (true) {
        int events = 0;
        struct android_poll_source *source = NULL;
        bool can_animate = engine.display != EGL_NO_DISPLAY && !engine.paused;
        int timeout = can_animate ? 16 : (engine.dirty ? 0 : -1);
        int ident = ALooper_pollOnce(timeout, NULL, &events, (void **)&source);

        if (ident >= 0 && source != NULL) {
            source->process(app, source);
        }
        if (app->destroyRequested != 0) {
            terminate_display(&engine);
            return;
        }
        if (engine.display != EGL_NO_DISPLAY) {
            advance_holomorphic_function(&engine);
        }
        if (engine.dirty) {
            draw_frame(&engine);
        }
    }
}
