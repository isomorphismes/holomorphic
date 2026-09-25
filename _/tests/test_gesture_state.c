#include <assert.h>
#include <stdio.h>

#include "../android/app/src/main/cpp/gesture_state.h"

int main(void) {
    enum gesture_kind ui_holds[] = {
        GESTURE_BLOCKED,
        GESTURE_CLEAR_BUTTON,
        GESTURE_VIEW_BUTTON
    };
    for (int index = 0; index < 3; ++index) {
        enum gesture_kind gesture = ui_holds[index];
        assert(gesture_is_ui_hold(gesture));
        assert(!gesture_pointer_down_starts_pinch(gesture, 2));
        assert(!gesture_pointer_down_resets(gesture, 3));
    }

    assert(gesture_pointer_down_starts_pinch(GESTURE_SINGLE, 2));
    assert(!gesture_pointer_down_starts_pinch(GESTURE_SINGLE, 3));
    assert(gesture_pointer_down_resets(GESTURE_SINGLE, 3));
    assert(gesture_pointer_down_resets(GESTURE_PINCH, 3));
    assert(!gesture_pointer_down_resets(GESTURE_NONE, 3));

    assert(!gesture_view_release_toggles(GESTURE_VIEW_BUTTON, false));
    assert(gesture_view_release_toggles(GESTURE_VIEW_BUTTON, true));
    assert(!gesture_view_release_toggles(GESTURE_BLOCKED, true));

    puts("gesture state tests passed");
    return 0;
}
