package org.isomorphisms.analyticcontinuation;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

/** Hosts the native EGL/OpenGL ES random holomorphic explorer. */
public final class ExplorerActivity extends NativeActivity {
    private static final String LOG_TAG = "AnalyticContinuation";

    private boolean cleanPresentation;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        cleanPresentation = scenarioRequestsCleanPresentation();
        super.onCreate(savedInstanceState);
        applyPresentationUi();
        if (cleanPresentation) {
            Log.i(LOG_TAG, "clean presentation system UI hidden");
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyPresentationUi();
        }
    }

    @Override
    public void onBackPressed() {
        Log.i(LOG_TAG, "explorer back requested");
        finish();
    }

    private boolean scenarioRequestsCleanPresentation() {
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(getAssets().open("scenario.conf")))) {
            String line;
            while ((line = reader.readLine()) != null) {
                int comment = line.indexOf('#');
                if (comment >= 0) {
                    line = line.substring(0, comment);
                }
                if (line.trim().equals("presentation=clean")) {
                    return true;
                }
            }
        } catch (IOException ignored) {
            // Native code owns scenario validation and reports scenario failures.
        }
        return false;
    }

    @SuppressWarnings("deprecation")
    private void applyPresentationUi() {
        if (!cleanPresentation) {
            return;
        }

        Window window = getWindow();
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(
                    WindowInsets.Type.statusBars() |
                    WindowInsets.Type.navigationBars()
                );
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                );
            }
            return;
        }

        window.getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
            View.SYSTEM_UI_FLAG_FULLSCREEN |
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }
}
