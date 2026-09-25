package org.isomorphisms.analyticcontinuation;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

/** Hosts the native EGL/OpenGL ES random holomorphic explorer. */
public final class ExplorerActivity extends NativeActivity {
    private static final String LOG_TAG = "AnalyticContinuation";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onBackPressed() {
        Log.i(LOG_TAG, "explorer back requested");
        finish();
    }
}
