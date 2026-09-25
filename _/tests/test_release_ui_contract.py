from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHADER = ROOT / "android" / "app" / "src" / "main" / "assets" / "continuation.frag.in"
RANDOM_NATIVE = ROOT / "android" / "app" / "src" / "main" / "cpp" / "analytic_continuation_random.c"
OVERLAY = ROOT / "android" / "app" / "src" / "main" / "cpp" / "polynomial_overlay.h"
ACTIVITY = ROOT / "android" / "app" / "src" / "main" / "java" / "org" / "isomorphisms" / "analyticcontinuation" / "ExplorerActivity.java"
GRADLE = ROOT / "android" / "app" / "build.gradle.kts"
FDROID = ROOT / "fdroid" / "org.isomorphisms.analyticcontinuation.yml.template"


class ReleaseUiContractTests(unittest.TestCase):
    def test_factor_markers_are_o_and_x(self) -> None:
        shader = SHADER.read_text()
        self.assertIn("Zeroes and poles deliberately read like tic-tac-toe: O and X.", shader)
        self.assertIn("float zero_mark = ring_mask(", shader)
        self.assertIn("vec2 pole_center =", shader)
        self.assertIn("float x_mark = max(", shader)

    def test_existing_factors_remain_draggable(self) -> None:
        native = RANDOM_NATIVE.read_text()
        self.assertIn("nearest_factor(", native)
        self.assertIn("factor drag selected: %s index=%d", native)
        self.assertIn("move_factor(", native)
        self.assertIn('engine->candidate_kind == FACTOR_ZERO ? "zero" : "pole"', native)
        self.assertIn('"%s moved: index=%d z=%.6g%+.6gi"', native)

    def test_exit_control_is_drawn_and_finishes_activity(self) -> None:
        shader = SHADER.read_text()
        native = RANDOM_NATIVE.read_text()
        self.assertIn("float exit_inset = 4.0 * pause_radius + 16.0;", shader)
        self.assertIn("float exit_mark = max(", shader)
        self.assertIn("float inset = 4.0f * radius + 16.0f;", native)
        self.assertIn("exit_control_contains", native)
        self.assertIn("ANativeActivity_finish(engine->app->activity);", native)

    def test_visible_formula_overlay_uses_division_and_superscripts(self) -> None:
        activity = ACTIVITY.read_text()
        self.assertIn("FormulaOverlayView formula = new FormulaOverlayView(this);", activity)
        formula_literals = (
            '"w = φ⁻¹(z)"',
            '"φ(w) = w + c₂w² + c₃w³ + c₄w⁴ + c₅w⁵ + c₆w⁶"',
            '"f(z) = eⁱᵗ (∏ᵢ B(aᵢ,w)) ÷ (∏ⱼ B(pⱼ,w))"',
            '"B(a,w) = (w−a) ÷ (1−āw)"',
        )
        for formula in formula_literals:
            self.assertIn(formula, activity)
            self.assertNotIn("^", formula)
            self.assertNotIn(" / ", formula)
        self.assertIn("public boolean onTouchEvent(MotionEvent event)", activity)
        self.assertIn("return false;", activity)

    def test_retained_bitmap_formatter_uses_division_glyph_not_fraction_slash(self) -> None:
        overlay = OVERLAY.read_text()
        self.assertIn('overlay_append(output, capacity, &used, " ÷");', overlay)
        self.assertIn("case 0x00f7u:", overlay)
        self.assertIn("overlay_decode_glyph", overlay)
        self.assertNotIn('snprintf(output, capacity, "(%s) / (%s)"', overlay)

    def test_retained_bitmap_formatter_draws_powers_as_superscripts(self) -> None:
        overlay = OVERLAY.read_text()
        self.assertIn('overlay_append(output, capacity, &used, "^%d", power);', overlay)
        self.assertIn("int superscript_scale = scale > 1 ? scale - 1 : 1;", overlay)
        self.assertIn("int superscript_y = y - 2 * scale;", overlay)
        self.assertIn("if (cursor[index] == '^')", overlay)
        superscript_branch = overlay.split("if (cursor[index] == '^')", 1)[1].split("continue;", 1)[0]
        marker_advance = superscript_branch.index("index += 1;")
        digit_draw = superscript_branch.index("overlay_draw_glyph")
        self.assertLess(marker_advance, digit_draw)

    def test_release_version_is_0_2_2_code_4_everywhere(self) -> None:
        gradle = GRADLE.read_text()
        fdroid = FDROID.read_text()
        self.assertIn("requestedVersionCode == null -> 4", gradle)
        self.assertIn('?: "0.2.2"', gradle)
        self.assertIn("versionName: 0.2.2", fdroid)
        self.assertIn("versionCode: 4", fdroid)
        self.assertIn("CurrentVersion: 0.2.2", fdroid)
        self.assertIn("CurrentVersionCode: 4", fdroid)


if __name__ == "__main__":
    unittest.main()
