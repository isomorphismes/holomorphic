module Example.HolomorphicLive

-- Maintained mathematical/rendering source for the generated Android fragment.
-- Compiled by the registered Idris2 -> GLSL ES backend. The old GLSL template
-- remains an independently named legacy baseline, not a generated fallback.
-- See docs/holomorphic-mathematical-contract.md and issue #55.

import Shader.Source
import Shader.PhasePortrait

%default total

complex_multiply : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> ShaderFloatTuple 2
complex_multiply left right =
  vec2 (x left * x right - y left * y right)
       (x left * y right + y left * x right)

||| The live five-term entire polynomial in the same fixed z/3 coordinate as
||| the 2026-10-10 Android renderer. Coefficient count and order are structural.
||| Horner evaluation changes association, not the mathematical polynomial.
holomorphic_q5 : ShaderFloatTuple 2 -> SArray 5 (ShaderFloatTuple 2) -> ShaderFloatTuple 2
holomorphic_q5 point coefficients =
  let u = scale_components (1.0 / 3.0) point
      c1 = array_at coefficients 0.0
      c2 = array_at coefficients 1.0
      c3 = array_at coefficients 2.0
      c4 = array_at coefficients 3.0
      c5 = array_at coefficients 4.0
   in complex_multiply u
        (add_components c1
          (complex_multiply u
            (add_components c2
              (complex_multiply u
                (add_components c3
                  (complex_multiply u
                    (add_components c4 (complex_multiply u c5))))))))

||| Input is an algebraically reduced divisor: (world x, world y, signed
||| integer multiplicity). The host groups exact coordinates including all 24
||| wandering poles, removes order-zero groups, and rejects capacity overflow.
||| Neither camera state nor a numerical epsilon defines factor equality.
|||
||| The third result is the signed order at the sample: positive means a zero,
||| negative means a pole, zero means a regular/removable point. Safe operands
||| are formed BEFORE atan/log, even on backends that eagerly lower branches.
factor_measure_stable : ShaderFloatTuple 2 -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
factor_measure_stable point factor =
  let delta : ShaderFloatTuple 2
      delta = subtract_components point (vec2 (x factor) (y factor))
      largest : Double
      largest = maxF (absF (x delta)) (absF (y delta))
      regular : Bool
      regular = largest > 0.0
      safe_largest : Double
      safe_largest = if regular then largest else 1.0
      direction : ShaderFloatTuple 2
      direction = if regular then vec2 (x delta / safe_largest)
                                         (y delta / safe_largest)
                             else vec2 1.0 0.0
      phase = atan2F (y direction) (x direction)
      log_radius = logF safe_largest + 0.5 * logF (component_product_sum direction direction)
      order = z factor
      singular_order = if regular then 0.0 else order
   in vec3 (order * phase) (order * log_radius) singular_order

covering
divisor_measure_from : ShaderFloatTuple 2 -> Double -> SArray 96 (ShaderFloatTuple 3) ->
                       Double -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
divisor_measure_from point count factors index accumulated =
  if index < minF count 96.0
     then
       let factor = array_at factors index
           next = add_components accumulated (factor_measure_stable point factor)
        in divisor_measure_from point count factors (index + 1.0) next
     else accumulated

covering
public export
holomorphic_divisor_measure : ShaderFloatTuple 2 -> Double -> SArray 96 (ShaderFloatTuple 3) -> ShaderFloatTuple 3
holomorphic_divisor_measure point count factors =
  divisor_measure_from point count factors 0.0 (vec3 0.0 0.0 0.0)

||| Numerical output, before the Wegert colour mapping. The final component
||| identifies a usable produced sample; zero rejects nonfinite/erased output.
||| At an actual zero/pole the first two components are finite placeholders;
||| the signed order MUST be checked before treating them as a regular field.
finite_marker : Double -> Double
finite_marker value =
  if absF value <= 3.4028234663852886e38 then 1.0 else 0.0

||| This is a rejection check, not an error bound or permission to widen a
||| tolerance. In particular, independent many-factor comparison remains
||| necessary even when a nonzero contribution was not erased completely.
addition_marker : Double -> Double -> Double -> Double
addition_marker entire divisor result =
  if absF divisor > 0.0
     then if result == entire then 0.0 else 1.0
     else 1.0

numerical_measure : ShaderFloatTuple 3 -> ShaderFloatTuple 2 -> ShaderFloatTuple 4
numerical_measure divisor q =
  let phase = x divisor + y q
      log_modulus = y divisor + x q
      usable = finite_marker phase * finite_marker log_modulus *
               finite_marker (x q) * finite_marker (y q) *
               addition_marker (y q) (x divisor) phase *
               addition_marker (x q) (y divisor) log_modulus
   in vec4 phase log_modulus (z divisor) usable

covering
public export
holomorphic_measure : ShaderFloatTuple 2 -> Double -> SArray 96 (ShaderFloatTuple 3) ->
                      SArray 5 (ShaderFloatTuple 2) -> ShaderFloatTuple 4
holomorphic_measure point count factors coefficients =
  let divisor = holomorphic_divisor_measure point count factors
      q = holomorphic_q5 point coefficients
   in numerical_measure divisor q

||| Reduce the entire term before adding a small divisor contribution. Adding
||| a huge q first can erase a displaced pair in binary32. This preserves only
||| the canonical colour coordinates; holomorphic_measure retains the raw log.
colour_measure : ShaderFloatTuple 3 -> ShaderFloatTuple 2 -> ShaderFloatTuple 2
colour_measure divisor q =
  let phase_period = 6.28318530717958647692
      log_period = 2.30258509299404568402
      q_phase = y q - phase_period * floorF (y q / phase_period)
      q_log = x q - log_period * floorF (x q / log_period)
   in vec2 (q_phase + x divisor) (q_log + y divisor)

mix_rgb : ShaderFloatTuple 3 -> ShaderFloatTuple 3 -> Double -> ShaderFloatTuple 3
mix_rgb before after weight =
  add_components (scale_components (1.0 - weight) before) (scale_components weight after)

circle_mask : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double
circle_mask pixel center radius =
  1.0 - smoothstepF (radius - 1.25) (radius + 1.25)
                    (euclidean_component_norm (subtract_components pixel center))

ring_mask : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double -> Double
ring_mask pixel center outside inside =
  maxF 0.0 (circle_mask pixel center outside - circle_mask pixel center inside)

line_mask : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double
line_mask pixel start finish half_width =
  let segment = subtract_components finish start
      denominator = maxF (component_product_sum segment segment) 0.000001
      along = clampF (component_product_sum (subtract_components pixel start) segment / denominator) 0.0 1.0
      nearest = add_components start (scale_components along segment)
      distance = euclidean_component_norm (subtract_components pixel nearest)
   in 1.0 - smoothstepF (half_width - 1.0) (half_width + 1.0) distance

draw_zero : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
draw_zero pixel center radius color =
  let mark = ring_mask pixel center 9.0 5.0
      dark = mix_rgb color (vec3 0.04 0.04 0.04) mark
      highlight = ring_mask pixel center 6.8 5.5
   in mix_rgb dark (vec3 0.98 0.98 0.98) highlight

draw_pole : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
draw_pole pixel center color =
  let first = line_mask pixel
                (add_components center (vec2 (-7.0) (-7.0)))
                (add_components center (vec2 7.0 7.0)) 2.4
      second = line_mask pixel
                 (add_components center (vec2 (-7.0) 7.0))
                 (add_components center (vec2 7.0 (-7.0))) 2.4
   in mix_rgb color (vec3 0.04 0.04 0.04) (maxF first second)

covering
zero_overlays_from : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double ->
                     SArray 32 (ShaderFloatTuple 2) -> Double -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
zero_overlays_from pixel resolution pixel_radius count positions index color =
  if index < minF count 32.0
     then
       let center = add_components (scale_components 0.5 resolution)
                         (scale_components pixel_radius (array_at positions index))
           next = draw_zero pixel center pixel_radius color
        in zero_overlays_from pixel resolution pixel_radius count positions
                              (index + 1.0) next
     else color

covering
pole_overlays_from : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double ->
                     SArray 32 (ShaderFloatTuple 2) -> Double -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
pole_overlays_from pixel resolution pixel_radius count positions index color =
  if index < minF count 32.0
     then
       let center = add_components (scale_components 0.5 resolution)
                         (scale_components pixel_radius (array_at positions index))
           next = draw_pole pixel center color
        in pole_overlays_from pixel resolution pixel_radius count positions
                              (index + 1.0) next
     else color

||| Markers retain their pole identity even when an exact opposite factor
||| removes their contribution from the reduced mathematical divisor.
covering
remote_overlays_from : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Double -> Double ->
                       SArray 24 (ShaderFloatTuple 2) -> Double -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
remote_overlays_from pixel resolution pixel_radius count positions index color =
  if index < minF count 24.0
     then
       let center = add_components (scale_components 0.5 resolution)
                         (scale_components pixel_radius (array_at positions index))
           next = draw_pole pixel center color
        in remote_overlays_from pixel resolution pixel_radius count positions
                                (index + 1.0) next
     else color

draw_placement_controls : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Int -> ShaderFloatTuple 3 -> ShaderFloatTuple 3
draw_placement_controls pixel resolution kind color =
  let radius = clampF (0.048 * minF (x resolution) (y resolution)) 26.0 38.0
      zero_center = vec2 (radius + 16.0) (radius + 16.0)
      pole_center = vec2 (x zero_center + 2.0 * radius + 14.0)
                         (y zero_center)
      zero_selected = int_to_double kind == 0.0
      pole_selected = int_to_double kind == 1.0
      zero_disk = circle_mask pixel zero_center radius
      zero_background = if zero_selected then vec3 0.94 0.94 0.94
                                         else vec3 0.04 0.04 0.04
      zero_mark_color = if zero_selected then vec3 0.05 0.05 0.05
                                         else vec3 0.96 0.96 0.96
      after_zero_disk = mix_rgb color zero_background (zero_disk * 0.86)
      zero_ring = ring_mask pixel zero_center (radius * 0.40) (radius * 0.25)
      after_zero_mark = mix_rgb after_zero_disk zero_mark_color zero_ring
      pole_disk = circle_mask pixel pole_center radius
      pole_background = if pole_selected then vec3 0.94 0.94 0.94
                                         else vec3 0.04 0.04 0.04
      pole_mark_color = if pole_selected then vec3 0.05 0.05 0.05
                                         else vec3 0.96 0.96 0.96
      after_pole_disk = mix_rgb after_zero_mark pole_background (pole_disk * 0.86)
      pole_a = line_mask pixel
                 (add_components pole_center (vec2 (-9.0) (-9.0)))
                 (add_components pole_center (vec2 9.0 9.0)) 2.2
      pole_b = line_mask pixel
                 (add_components pole_center (vec2 (-9.0) 9.0))
                 (add_components pole_center (vec2 9.0 (-9.0))) 2.2
   in mix_rgb after_pole_disk pole_mark_color (maxF pole_a pole_b)

||| The output of this ordinary checked Idris function is the real Android
||| fragment, not a typed copy sitting beside handwritten production GLSL.
%export "glsles:fragment|v_ndc=in,u_resolution=uniform,u_zero_count=uniform,u_pole_count=uniform,u_zero_positions=uniform,u_pole_positions=uniform,u_remote_pole_positions=uniform,u_holomorphic_coefficients=uniform,u_divisor_count=uniform,u_divisor_factors=uniform,u_zoom=uniform,u_placement_kind=uniform,u_numeric_output=uniform,u_numeric_point=uniform"
covering
holomorphic_live : ShaderFloatTuple 2 -> ShaderFloatTuple 2 -> Int -> Int ->
                   SArray 32 (ShaderFloatTuple 2) -> SArray 32 (ShaderFloatTuple 2) ->
                   SArray 24 (ShaderFloatTuple 2) -> SArray 5 (ShaderFloatTuple 2) ->
                   Int -> SArray 96 (ShaderFloatTuple 3) ->
                   Double -> Int -> Int -> ShaderFloatTuple 2 -> ShaderFloatTuple 4
holomorphic_live ndc resolution zero_count pole_count zeros poles remote_positions
                 coefficients divisor_count factors zoom placement_kind
                 numeric_output numeric_point =
  let pixel = scale_components 0.5 (add_components (vec2 1.0 1.0) ndc)
      fragment_pixel = vec2 (x pixel * x resolution) (y pixel * y resolution)
      pixel_radius = 0.42 * minF (x resolution) (y resolution) * zoom
      displayed_point = scale_components (1.0 / pixel_radius)
                              (subtract_components fragment_pixel (scale_components 0.5 resolution))
      numeric_mode = int_to_double numeric_output
      numerical = numeric_mode > 0.0
      raw_measure_output = numeric_mode == 1.0
      precolour_measure_output = numeric_mode == 3.0
      point = if numerical then numeric_point else displayed_point
      divisor = holomorphic_divisor_measure point (int_to_double divisor_count) factors
      q = holomorphic_q5 point coefficients
      measure = colour_measure divisor q
      regular_color = wegert_rgb_from_measure measure
      wegert_color = if z divisor > 0.0 then vec3 0.0 0.0 0.0
                     else if z divisor < 0.0 then vec3 1.0 1.0 1.0
                     else regular_color
      with_zero_marks = zero_overlays_from fragment_pixel resolution pixel_radius
                                           (int_to_double zero_count) zeros
                                           0.0 wegert_color
      with_pole_marks = pole_overlays_from fragment_pixel resolution pixel_radius
                                           (int_to_double pole_count) poles
                                           0.0 with_zero_marks
      with_remote_marks = remote_overlays_from fragment_pixel resolution pixel_radius
                                               24.0 remote_positions 0.0 with_pole_marks
      color = draw_placement_controls fragment_pixel resolution placement_kind
                                      with_remote_marks
      numerical_sample = numerical_measure divisor q
      -- These are the actual inputs to the shared Wegert colour function.
      -- Raw addition loss does not invalidate finite pre-colour coordinates.
      precolour_sample = vec4 (x measure) (y measure) (z divisor)
                              (finite_marker (x measure) * finite_marker (y measure) *
                               finite_marker (z divisor))
   in if raw_measure_output then numerical_sample
      else if precolour_measure_output then precolour_sample
      else if numerical then vec4 (x wegert_color) (y wegert_color) (z wegert_color) 1.0
      else vec4 (x color) (y color) (z color) 1.0

main : IO ()
main = pure ()
