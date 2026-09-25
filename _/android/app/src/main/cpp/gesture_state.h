#ifndef WEGERT_GESTURE_STATE_H
#define WEGERT_GESTURE_STATE_H

#include <stdbool.h>

enum gesture_kind {
    GESTURE_NONE,
    GESTURE_SINGLE,
    GESTURE_PINCH,
    GESTURE_CLEAR_BUTTON,
    GESTURE_VIEW_BUTTON,
    GESTURE_BLOCKED
};

static bool gesture_is_ui_hold(enum gesture_kind gesture) {
    return gesture == GESTURE_CLEAR_BUTTON ||
        gesture == GESTURE_VIEW_BUTTON ||
        gesture == GESTURE_BLOCKED;
}

static bool gesture_pointer_down_starts_pinch(enum gesture_kind gesture, int pointer_count) {
    return gesture == GESTURE_SINGLE && pointer_count == 2;
}

static bool gesture_pointer_down_resets(enum gesture_kind gesture, int pointer_count) {
    return pointer_count >= 3 &&
        (gesture == GESTURE_SINGLE || gesture == GESTURE_PINCH);
}

static bool gesture_view_release_toggles(enum gesture_kind gesture, bool released_inside) {
    return gesture == GESTURE_VIEW_BUTTON && released_inside;
}

#endif

