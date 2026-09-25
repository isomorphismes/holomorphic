package org.isomorphisms.analyticcontinuation;

import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.UiAutomation;
import android.content.Intent;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public final class PinchZoomedDragTest {
    private static MotionEvent.PointerProperties pointer(int id) {
        MotionEvent.PointerProperties properties = new MotionEvent.PointerProperties();
        properties.id = id;
        properties.toolType = MotionEvent.TOOL_TYPE_FINGER;
        return properties;
    }

    private static MotionEvent.PointerCoords coordinates(float x, float y) {
        MotionEvent.PointerCoords coordinates = new MotionEvent.PointerCoords();
        coordinates.x = x;
        coordinates.y = y;
        coordinates.pressure = 1.0f;
        coordinates.size = 1.0f;
        return coordinates;
    }

    private static MotionEvent motion(
        long downTime,
        int action,
        MotionEvent.PointerProperties[] properties,
        MotionEvent.PointerCoords[] coordinates
    ) {
        return MotionEvent.obtain(
            downTime,
            SystemClock.uptimeMillis(),
            action,
            properties.length,
            properties,
            coordinates,
            0,
            0,
            1.0f,
            1.0f,
            0,
            0,
            InputDevice.SOURCE_TOUCHSCREEN,
            0
        );
    }

    private static void inject(UiAutomation automation, MotionEvent event) {
        try {
            assertTrue("input event injection", automation.injectInputEvent(event, true));
        } finally {
            event.recycle();
        }
    }

    private static void sleep(long millis) {
        SystemClock.sleep(millis);
    }

    @Test
    public void pinchThenDragInitialZeroAtZoomedCoordinate() {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        Intent intent = new Intent();
        intent.setClassName(
            instrumentation.getTargetContext().getPackageName(),
            "org.isomorphisms.analyticcontinuation.ExplorerActivity"
        );
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);

        Activity activity = instrumentation.startActivitySync(intent);
        instrumentation.waitForIdleSync();
        sleep(1400L);

        int width = activity.getWindow().getDecorView().getWidth();
        int height = activity.getWindow().getDecorView().getHeight();
        assertTrue("activity width", width > 0);
        assertTrue("activity height", height > 0);

        UiAutomation automation = instrumentation.getUiAutomation();
        float centerX = 0.5f * width;
        float centerY = 0.5f * height;
        float startHalfDistance = 80.0f;
        float finishHalfDistance = 180.0f;
        float expectedZoom = finishHalfDistance / startHalfDistance;

        MotionEvent.PointerProperties first = pointer(0);
        MotionEvent.PointerProperties second = pointer(1);
        long pinchDown = SystemClock.uptimeMillis();

        inject(
            automation,
            motion(
                pinchDown,
                MotionEvent.ACTION_DOWN,
                new MotionEvent.PointerProperties[] {first},
                new MotionEvent.PointerCoords[] {
                    coordinates(centerX - startHalfDistance, centerY)
                }
            )
        );
        sleep(50L);

        inject(
            automation,
            motion(
                pinchDown,
                MotionEvent.ACTION_POINTER_DOWN |
                    (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new MotionEvent.PointerProperties[] {first, second},
                new MotionEvent.PointerCoords[] {
                    coordinates(centerX - startHalfDistance, centerY),
                    coordinates(centerX + startHalfDistance, centerY)
                }
            )
        );
        sleep(50L);

        inject(
            automation,
            motion(
                pinchDown,
                MotionEvent.ACTION_MOVE,
                new MotionEvent.PointerProperties[] {first, second},
                new MotionEvent.PointerCoords[] {
                    coordinates(centerX - finishHalfDistance, centerY),
                    coordinates(centerX + finishHalfDistance, centerY)
                }
            )
        );
        sleep(80L);

        inject(
            automation,
            motion(
                pinchDown,
                MotionEvent.ACTION_POINTER_UP |
                    (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT),
                new MotionEvent.PointerProperties[] {first, second},
                new MotionEvent.PointerCoords[] {
                    coordinates(centerX - finishHalfDistance, centerY),
                    coordinates(centerX + finishHalfDistance, centerY)
                }
            )
        );
        sleep(50L);

        inject(
            automation,
            motion(
                pinchDown,
                MotionEvent.ACTION_UP,
                new MotionEvent.PointerProperties[] {first},
                new MotionEvent.PointerCoords[] {
                    coordinates(centerX - finishHalfDistance, centerY)
                }
            )
        );
        sleep(250L);

        // Random-Lasso starts with the first zero at z=-0.34. The hit point
        // below deliberately uses the zoomed view radius. If native input
        // conversion ignores zoom, the following down event misses the zero.
        float viewRadius = 0.42f * Math.min(width, height) * expectedZoom;
        float zeroX = centerX - 0.34f * viewRadius;
        float zeroY = centerY;
        long dragDown = SystemClock.uptimeMillis();

        inject(
            automation,
            motion(
                dragDown,
                MotionEvent.ACTION_DOWN,
                new MotionEvent.PointerProperties[] {first},
                new MotionEvent.PointerCoords[] {coordinates(zeroX, zeroY)}
            )
        );
        sleep(60L);
        inject(
            automation,
            motion(
                dragDown,
                MotionEvent.ACTION_MOVE,
                new MotionEvent.PointerProperties[] {first},
                new MotionEvent.PointerCoords[] {coordinates(zeroX + 54.0f, zeroY - 28.0f)}
            )
        );
        sleep(60L);
        inject(
            automation,
            motion(
                dragDown,
                MotionEvent.ACTION_UP,
                new MotionEvent.PointerProperties[] {first},
                new MotionEvent.PointerCoords[] {coordinates(zeroX + 54.0f, zeroY - 28.0f)}
            )
        );
        sleep(250L);

        activity.finish();
        instrumentation.waitForIdleSync();
    }
}
