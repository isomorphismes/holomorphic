package org.isomorphisms.analyticcontinuation;

import android.app.NativeActivity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;

/** Hosts the native EGL/OpenGL ES lasso explorer. */
public final class ExplorerActivity extends NativeActivity {
    private static final String LOG_TAG = "AnalyticContinuation";

    private PopupWindow formulaPopup;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        View decor = getWindow().getDecorView();
        decor.post(() -> showFormulaOverlay(decor));
    }

    private void showFormulaOverlay(View anchor) {
        FormulaOverlayView formula = new FormulaOverlayView(this);
        formulaPopup = new PopupWindow(
            formula,
            ViewGroup.LayoutParams.MATCH_PARENT,
            dp(112),
            false
        );
        formulaPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        formulaPopup.setTouchable(false);
        formulaPopup.setFocusable(false);
        formulaPopup.setOutsideTouchable(false);
        formulaPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        formulaPopup.showAtLocation(anchor, Gravity.TOP | Gravity.CENTER_HORIZONTAL, 0, 0);
        Log.i(LOG_TAG, "formula overlay shown");
    }

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            value,
            getResources().getDisplayMetrics()
        ));
    }

    @Override
    public void onBackPressed() {
        Log.i(LOG_TAG, "lasso back requested");
        finish();
    }

    @Override
    protected void onDestroy() {
        if (formulaPopup != null) {
            formulaPopup.dismiss();
            formulaPopup = null;
        }
        super.onDestroy();
    }

    /**
     * A compact, touch-transparent description of the function that the native
     * renderer is actually evaluating. It deliberately stays structural: the
     * random holomorphic walk continuously changes the lasso coefficients, so
     * pretending rounded decimal coefficients were exact would be misleading.
     */
    private static final class FormulaOverlayView extends View {
        private static final String[] LINES = {
            "w = φ⁻¹(z)",
            "φ(w) = w + c₂w² + c₃w³ + c₄w⁴ + c₅w⁵ + c₆w⁶",
            "f(z) = eⁱᵗ (∏ᵢ B(aᵢ,w)) ÷ (∏ⱼ B(pⱼ,w))",
            "B(a,w) = (w−a) ÷ (1−āw)"
        };

        private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint boxPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float density;

        FormulaOverlayView(Context context) {
            super(context);
            density = context.getResources().getDisplayMetrics().density;
            setClickable(false);
            setFocusable(false);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);

            textPaint.setColor(Color.rgb(246, 243, 232));
            textPaint.setTextSize(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_SP,
                14.0f,
                context.getResources().getDisplayMetrics()
            ));
            textPaint.setTypeface(Typeface.create("sans-serif", Typeface.NORMAL));

            boxPaint.setColor(Color.argb(188, 18, 18, 18));
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            return false;
        }

        @Override
        protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);

            float lineHeight = 20.0f * density;
            float padding = 8.0f * density;
            float left = Math.max(132.0f * density, 0.16f * getWidth());
            float maxTextWidth = 0.0f;
            for (String line : LINES) {
                maxTextWidth = Math.max(maxTextWidth, textPaint.measureText(line));
            }

            float right = Math.min(
                getWidth() - 14.0f * density,
                left + maxTextWidth + 2.0f * padding
            );
            float top = 8.0f * density;
            float bottom = top + LINES.length * lineHeight + 2.0f * padding;
            RectF box = new RectF(left, top, right, bottom);
            canvas.drawRoundRect(box, 7.0f * density, 7.0f * density, boxPaint);

            float x = left + padding;
            float baseline = top + padding - textPaint.ascent();
            for (String line : LINES) {
                canvas.drawText(line, x, baseline, textPaint);
                baseline += lineHeight;
            }
        }
    }
}
