package org.isomorphisms.analyticcontinuation;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.VideoView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;

public final class MainActivity extends Activity {
    private VideoView videoView;
    private TextView statusView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(16), dp(12), dp(16), dp(16));
        page.setBackgroundColor(Color.rgb(15, 17, 22));

        TextView title = new TextView(this);
        title.setText("Analytic Continuation");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);
        title.setGravity(Gravity.CENTER_HORIZONTAL);
        page.addView(title, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        Button exploreButton = new Button(this);
        exploreButton.setText("Explore convergence discs live");
        exploreButton.setAllCaps(false);
        exploreButton.setOnClickListener(view -> startActivity(
            new Intent(this, ExplorerActivity.class)
        ));
        LinearLayout.LayoutParams exploreLayout = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        );
        exploreLayout.setMargins(0, dp(10), 0, dp(4));
        page.addView(exploreButton, exploreLayout);

        TextView exploreHint = new TextView(this);
        exploreHint.setText("Place zeros or ∞, then reveal overlapping Taylor discs.");
        exploreHint.setTextColor(Color.rgb(183, 189, 202));
        exploreHint.setTextSize(14);
        exploreHint.setGravity(Gravity.CENTER_HORIZONTAL);
        exploreHint.setPadding(0, 0, 0, dp(8));
        page.addView(exploreHint, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        statusView = new TextView(this);
        statusView.setTextColor(Color.rgb(210, 214, 223));
        statusView.setTextSize(15);
        statusView.setGravity(Gravity.CENTER_HORIZONTAL);
        statusView.setPadding(0, dp(6), 0, dp(8));
        page.addView(statusView, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        videoView = new VideoView(this);
        videoView.setBackgroundColor(Color.BLACK);
        videoView.setOnCompletionListener(player -> statusView.setText("Tap a movie to replay it."));
        videoView.setOnErrorListener((player, what, extra) -> {
            statusView.setText("This movie could not be played on this device.");
            return true;
        });
        page.addView(videoView, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            0,
            1f
        ));

        LinearLayout movieButtons = new LinearLayout(this);
        movieButtons.setOrientation(LinearLayout.HORIZONTAL);
        movieButtons.setGravity(Gravity.CENTER);

        HorizontalScrollView buttonScroller = new HorizontalScrollView(this);
        buttonScroller.setFillViewport(true);
        buttonScroller.addView(movieButtons, new HorizontalScrollView.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ));
        page.addView(buttonScroller, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        setContentView(page);
        addMovieButtons(movieButtons);
    }

    private void addMovieButtons(LinearLayout movieButtons) {
        try {
            String[] assetNames = getAssets().list("movies");
            if (assetNames == null) {
                assetNames = new String[0];
            }
            Arrays.sort(assetNames);

            int movieCount = 0;
            for (String assetName : assetNames) {
                if (!assetName.endsWith(".mp4")) {
                    continue;
                }
                movieCount += 1;
                Button button = new Button(this);
                button.setText(displayName(assetName));
                button.setAllCaps(false);
                button.setOnClickListener(view -> playMovie(assetName));
                movieButtons.addView(button);
            }

            if (movieCount == 0) {
                statusView.setText("Live explorer ready; this build contains no rendered movies.");
            } else {
                statusView.setText("Explore live or choose a guided movie.");
            }
        } catch (IOException exception) {
            statusView.setText("The packaged movie list could not be read.");
        }
    }

    private void playMovie(String assetName) {
        try {
            File cachedMovie = copyMovieToCache(assetName);
            statusView.setText(displayName(assetName));
            videoView.setVideoPath(cachedMovie.getAbsolutePath());
            videoView.setOnPreparedListener(player -> {
                player.setLooping(false);
                videoView.start();
            });
        } catch (IOException exception) {
            statusView.setText("The selected movie could not be opened.");
        }
    }

    private File copyMovieToCache(String assetName) throws IOException {
        File movieDirectory = new File(getCacheDir(), "movies");
        if (!movieDirectory.exists() && !movieDirectory.mkdirs()) {
            throw new IOException("Could not create movie cache");
        }

        File cachedMovie = new File(movieDirectory, assetName);
        try (
            InputStream input = getAssets().open("movies/" + assetName);
            FileOutputStream output = new FileOutputStream(cachedMovie, false)
        ) {
            byte[] buffer = new byte[16 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
            }
        }
        return cachedMovie;
    }

    private static String displayName(String assetName) {
        String withoutExtension = assetName.substring(0, assetName.length() - 4);
        String[] words = withoutExtension.replace('_', '-').split("-");
        StringBuilder display = new StringBuilder();
        for (String word : words) {
            if (word.isEmpty()) {
                continue;
            }
            if (display.length() > 0) {
                display.append(' ');
            }
            display.append(Character.toUpperCase(word.charAt(0)));
            display.append(word.substring(1));
        }
        return display.toString();
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
