package org.isomorphisms.analyticcontinuation;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

/** Hosts the native EGL/OpenGL ES lasso explorer. */
public final class ExplorerActivity extends NativeActivity {
    private static final String LOG_TAG = "AnalyticContinuation";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (
            event.getActionMasked() == MotionEvent.ACTION_DOWN
                && closeControlContains(event.getX(), event.getY())
        ) {
            Log.i(LOG_TAG, "lasso close requested");
            finish();
            return true;
        }
        return super.dispatchTouchEvent(event);
    }

    private boolean closeControlContains(float x, float y) {
        View decor = getWindow().getDecorView();
        float width = decor.getWidth();
        float height = decor.getHeight();
        if (width <= 0.0f || height <= 0.0f) {
            return false;
        }

        float radius = 0.052f * Math.min(width, height);
        if (radius < 28.0f) radius = 28.0f;
        if (radius > 42.0f) radius = 42.0f;

        float centerX = width - 4.0f * radius - 16.0f;
        float centerY = radius + 16.0f;
        return Math.hypot(x - centerX, y - centerY) <= radius;
    }

    @Override
    public void onBackPressed() {
        Log.i(LOG_TAG, "lasso back requested");
        finish();
    }
}
