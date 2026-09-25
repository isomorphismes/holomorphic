#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "motion.h"
#include "scenario.h"
#include "scene.h"

static bool near(float left, float right, float tolerance) {
    return fabsf(left - right) <= tolerance;
}

static void test_default_scene(void) {
    struct scene scene;
    scene_initialize_default(&scene);
    assert(scene.zero_count == 1);
    assert(scene.pole_count == 1);
    assert(near(scene.zero_positions[0][0], -0.34f, 1.0e-6f));
    assert(near(scene.pole_positions[0][0], 0.34f, 1.0e-6f));
    assert(near(scene.zoom, 1.0f, 1.0e-6f));
}

static void test_scenario_parser(void) {
    const char *text =
        "name=video-two-exchanges\n"
        "presentation=clean\n"
        "marker_radius=5.0\n"
        "marker_stroke=1.8\n"
        "field=wandering_offscreen_poles\n"
        "field_speed=0.42\n"
        "field_budget=6.0\n"
        "motion=wander\n"
        "seed=7319\n"
        "wander_speed=0.08\n"
        "wander_bound=1.7\n"
        "zero=-0.8,0.2\n"
        "zero=0.0,-0.6\n"
        "zero=0.7,0.4\n"
        "pole=0.8,-0.2\n"
        "pole=0.1,0.6\n"
        "pole=-0.6,-0.3\n"
        "exchange=zero,0,pole,2,4.0,6.0,ccw\n"
        "exchange=zero,2,pole,0,14.0,6.0,cw\n";

    struct scenario scenario;
    char error[192];
    assert(scenario_parse_text(&scenario, text, error, sizeof(error)));
    assert(strcmp(scenario.name, "video-two-exchanges") == 0);
    assert(scenario.scene.zero_count == 3);
    assert(scenario.scene.pole_count == 3);
    assert(!scenario.presentation.show_controls);
    assert(!scenario.presentation.interaction_enabled);
    assert(near(scenario.presentation.marker_radius_px, 5.0f, 1.0e-6f));
    assert(near(scenario.presentation.marker_stroke_px, 1.8f, 1.0e-6f));
    assert(scenario.field_background == FIELD_BACKGROUND_WANDERING_OFFSCREEN_POLES);
    assert(near(scenario.field_speed, 0.42f, 1.0e-6f));
    assert(near(scenario.field_budget, 6.0f, 1.0e-6f));
    assert(scenario.motion.enabled);
    assert(scenario.motion.seed == 7319u);
    assert(scenario.motion.exchange_count == 2);
    assert(motion_program_validate(&scenario.motion, &scenario.scene));
}

static void test_half_circle_exchange(void) {
    struct scene scene;
    scene_initialize_default(&scene);
    scene_clear_factors(&scene);
    assert(scene_add_factor(&scene, SCENE_FACTOR_ZERO, -1.0f, 0.0f, NULL));
    assert(scene_add_factor(&scene, SCENE_FACTOR_POLE, 1.0f, 0.0f, NULL));

    struct motion_program motion;
    motion_program_initialize(&motion);
    assert(motion_program_add_exchange(
        &motion,
        (struct motion_factor_ref){SCENE_FACTOR_ZERO, 0},
        (struct motion_factor_ref){SCENE_FACTOR_POLE, 0},
        0.0f,
        1.0f,
        MOTION_TURN_COUNTERCLOCKWISE
    ));
    assert(motion_program_validate(&motion, &scene));

    for (int step = 0; step < 10; ++step) {
        assert(motion_program_advance(&motion, &scene, 0.05f));
    }
    assert(near(scene.zero_positions[0][0], 0.0f, 0.02f));
    assert(near(scene.zero_positions[0][1], -1.0f, 0.02f));
    assert(near(scene.pole_positions[0][0], 0.0f, 0.02f));
    assert(near(scene.pole_positions[0][1], 1.0f, 0.02f));

    for (int step = 0; step < 11; ++step) {
        motion_program_advance(&motion, &scene, 0.05f);
    }
    assert(motion.exchanges[0].completed);
    assert(motion_program_completed_exchange_count(&motion) == 1);
    assert(near(scene.zero_positions[0][0], 1.0f, 1.0e-6f));
    assert(near(scene.zero_positions[0][1], 0.0f, 1.0e-6f));
    assert(near(scene.pole_positions[0][0], -1.0f, 1.0e-6f));
    assert(near(scene.pole_positions[0][1], 0.0f, 1.0e-6f));
}

static void test_exchange_timeline_uses_wall_clock(void) {
    struct scene scene;
    scene_initialize_default(&scene);
    scene_clear_factors(&scene);
    assert(scene_add_factor(&scene, SCENE_FACTOR_ZERO, -1.0f, 0.0f, NULL));
    assert(scene_add_factor(&scene, SCENE_FACTOR_POLE, 1.0f, 0.0f, NULL));

    struct motion_program motion;
    motion_program_initialize(&motion);
    assert(motion_program_add_exchange(
        &motion,
        (struct motion_factor_ref){SCENE_FACTOR_ZERO, 0},
        (struct motion_factor_ref){SCENE_FACTOR_POLE, 0},
        1.0f,
        2.0f,
        MOTION_TURN_CLOCKWISE
    ));

    /* Three deliberately slow rendered frames still advance a three-second event. */
    motion_program_advance(&motion, &scene, 1.0f);
    assert(!motion.exchanges[0].completed);
    motion_program_advance(&motion, &scene, 1.0f);
    assert(!motion.exchanges[0].completed);
    motion_program_advance(&motion, &scene, 1.0f);
    assert(motion.exchanges[0].completed);
    assert(near(motion.elapsed_seconds, 3.0f, 1.0e-6f));
    assert(near(scene.zero_positions[0][0], 1.0f, 1.0e-6f));
    assert(near(scene.pole_positions[0][0], -1.0f, 1.0e-6f));
}

static void test_wander_is_not_lockstep(void) {
    struct scene scene;
    scene_initialize_default(&scene);
    scene_clear_factors(&scene);
    assert(scene_add_factor(&scene, SCENE_FACTOR_ZERO, -0.2f, 0.1f, NULL));
    assert(scene_add_factor(&scene, SCENE_FACTOR_ZERO, -0.2f, 0.1f, NULL));
    assert(scene_add_factor(&scene, SCENE_FACTOR_POLE, 0.2f, -0.1f, NULL));

    struct motion_program motion;
    motion_program_initialize(&motion);
    motion.enabled = true;
    motion.seed = 991u;
    motion.wander_speed = 0.12f;
    motion.wander_bound = 2.0f;

    for (int step = 0; step < 120; ++step) {
        motion_program_advance(&motion, &scene, 0.05f);
    }

    float separation = hypotf(
        scene.zero_positions[0][0] - scene.zero_positions[1][0],
        scene.zero_positions[0][1] - scene.zero_positions[1][1]
    );
    assert(separation > 0.01f);
}

static void test_invalid_exchange_reference(void) {
    struct scene scene;
    scene_initialize_default(&scene);

    struct motion_program motion;
    motion_program_initialize(&motion);
    assert(motion_program_add_exchange(
        &motion,
        (struct motion_factor_ref){SCENE_FACTOR_ZERO, 0},
        (struct motion_factor_ref){SCENE_FACTOR_POLE, 7},
        1.0f,
        2.0f,
        MOTION_TURN_CLOCKWISE
    ));
    assert(!motion_program_validate(&motion, &scene));
}

int main(void) {
    test_default_scene();
    test_scenario_parser();
    test_half_circle_exchange();
    test_exchange_timeline_uses_wall_clock();
    test_wander_is_not_lockstep();
    test_invalid_exchange_reference();
    puts("scene/motion/scenario tests passed");
    return 0;
}
