module Example.HolomorphicLive

-- Maintained mathematical/rendering source for the generated Android fragment.
-- Compiled by the registered Idris2 -> GLSL ES backend. The old GLSL template
-- remains an independently named legacy baseline, not a generated fallback.
-- See docs/holomorphic-mathematical-contract.md and issue #55.

import Shader.Source
import Shader.PhasePortrait

%default total

complex_multiply : SVec 2 -> SVec 2 -> SVec 2
complex_multiply left right =
  vec2 (x left * x right - y left * y right)
       (x left * y right + y left * x right)

||| The live five-term entire polynomial in the same fixed z/3 coordinate as
||| the 2026-10-10 Android renderer. Coefficient count and order are structural.
||| Horner evaluation changes association, not the mathematical polynomial.
holomorphic_q5 : SVec 2 -> SArray 5 (SVec 2) -> SVec 2
holomorphic_q5 point coefficients =
  let u = scale (1.0 / 3.0) point
      c1 = array_at coefficients 0.0
      c2 = array_at coefficients 1.0
      c3 = array_at coefficients 2.0
      c4 = array_at coefficients 3.0
      c5 = array_at coefficients 4.0
   in complex_multiply u
        (vadd c1
          (complex_multiply u
            (vadd c2
              (complex_multiply u
                (vadd c3
                  (complex_multiply u
                    (vadd c4 (complex_multiply u c5))))))))

hash1 : Double -> Double
hash1 value = fractF (sinF (value * 127.1 + 31.7) * 43758.5453123)

remote_pole_position : Double -> Double -> Double -> SVec 2
remote_pole_position index outside_radius time =
  let initial_angle = 6.2831853 * hash1 (index + 0.11)
      initial_direction = vec2 (cosF initial_angle) (sinF initial_angle)
      angle0 = atan2F (y initial_direction) (x initial_direction)
      handedness = if hash1 (index + 7.91) < 0.5 then -1.0 else 1.0
      speed = mixF 0.11 0.19 (hash1 (index + 4.37))
      angle = angle0 + handedness * speed * time
      bend_phase = 6.2831853 * hash1 (index + 11.23)
      radial_bend = 0.22 * sinF (2.0 * angle + bend_phase)
      radius = mixF 2.75 4.00 (hash1 (index + 1.73)) + radial_bend
      ellipticity = mixF (-0.08) 0.08 (hash1 (index + 14.67))
      ellipse = vec2 ((1.0 + ellipticity) * cosF angle)
                     ((1.0 - ellipticity) * sinF angle)
   in scale (outside_radius * radius) ellipse

pole_measure : SVec 2 -> SVec 2 -> SVec 2
pole_measure point pole =
  let delta = vsub point pole
      squared_radius = maxF (dot delta delta) 0.0000000000000001
   in vec2 (atan2F (y delta) (x delta))
           (0.5 * logF squared_radius)

||| Bounded, uniform loop: 24 real remote poles, no generated-screen heuristic.
covering
remote_measure_from : SVec 2 -> Double -> Double -> Double ->
                      Double -> SVec 2 -> SVec 2
remote_measure_from point outside_radius time count index accumulated =
  if index < minF count 24.0
     then
       let pole = remote_pole_position index outside_radius time
           next = vadd accumulated (pole_measure point pole)
        in remote_measure_from point outside_radius time count
                               (index + 1.0) next
     else accumulated

covering
remote_measure_24 : SVec 2 -> Double -> Double -> SVec 2
remote_measure_24 point outside_radius time =
  remote_measure_from point outside_radius time 24.0 0.0 (vec2 0.0 0.0)

mix_rgb : SVec 3 -> SVec 3 -> Double -> SVec 3
mix_rgb before after weight =
  vadd (scale (1.0 - weight) before) (scale weight after)

circle_mask : SVec 2 -> SVec 2 -> Double -> Double
circle_mask pixel center radius =
  1.0 - smoothstepF (radius - 1.25) (radius + 1.25)
                    (length (vsub pixel center))

ring_mask : SVec 2 -> SVec 2 -> Double -> Double -> Double
ring_mask pixel center outside inside =
  maxF 0.0 (circle_mask pixel center outside - circle_mask pixel center inside)

line_mask : SVec 2 -> SVec 2 -> SVec 2 -> Double -> Double
line_mask pixel start finish half_width =
  let segment = vsub finish start
      denominator = maxF (dot segment segment) 0.000001
      along = clampF (dot (vsub pixel start) segment / denominator) 0.0 1.0
      nearest = vadd start (scale along segment)
      distance = length (vsub pixel nearest)
   in 1.0 - smoothstepF (half_width - 1.0) (half_width + 1.0) distance

draw_zero : SVec 2 -> SVec 2 -> Double -> SVec 3 -> SVec 3
draw_zero pixel center radius color =
  let mark = ring_mask pixel center 9.0 5.0
      dark = mix_rgb color (vec3 0.04 0.04 0.04) mark
      highlight = ring_mask pixel center 6.8 5.5
   in mix_rgb dark (vec3 0.98 0.98 0.98) highlight

draw_pole : SVec 2 -> SVec 2 -> SVec 3 -> SVec 3
draw_pole pixel center color =
  let first = line_mask pixel
                (vadd center (vec2 (-7.0) (-7.0)))
                (vadd center (vec2 7.0 7.0)) 2.4
      second = line_mask pixel
                 (vadd center (vec2 (-7.0) 7.0))
                 (vadd center (vec2 7.0 (-7.0))) 2.4
   in mix_rgb color (vec3 0.04 0.04 0.04) (maxF first second)

covering
zero_overlays_from : SVec 2 -> SVec 2 -> Double -> Double ->
                     SArray 64 (SVec 2) -> Double -> SVec 3 -> SVec 3
zero_overlays_from pixel resolution pixel_radius count positions index color =
  if index < minF count 32.0
     then
       let center = vadd (scale 0.5 resolution)
                         (scale pixel_radius (array_at positions index))
           next = draw_zero pixel center pixel_radius color
        in zero_overlays_from pixel resolution pixel_radius count positions
                              (index + 1.0) next
     else color

covering
pole_overlays_from : SVec 2 -> SVec 2 -> Double -> Double ->
                     SArray 64 (SVec 2) -> Double -> SVec 3 -> SVec 3
pole_overlays_from pixel resolution pixel_radius count positions index color =
  if index < minF count 32.0
     then
       let center = vadd (scale 0.5 resolution)
                         (scale pixel_radius (array_at positions index))
           next = draw_pole pixel center color
        in pole_overlays_from pixel resolution pixel_radius count positions
                              (index + 1.0) next
     else color

draw_placement_controls : SVec 2 -> SVec 2 -> Int -> SVec 3 -> SVec 3
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
                 (vadd pole_center (vec2 (-9.0) (-9.0)))
                 (vadd pole_center (vec2 9.0 9.0)) 2.2
      pole_b = line_mask pixel
                 (vadd pole_center (vec2 (-9.0) 9.0))
                 (vadd pole_center (vec2 9.0 (-9.0))) 2.2
   in mix_rgb after_pole_disk pole_mark_color (maxF pole_a pole_b)

||| The output of this ordinary checked Idris function is the real Android
||| fragment, not a typed copy sitting beside handwritten production GLSL.
%export "glsles:fragment|v_ndc=in,u_resolution=uniform,u_zero_count=uniform,u_pole_count=uniform,u_zero_positions=uniform,u_pole_positions=uniform,u_holomorphic_coefficients=uniform,u_remote_pole_time=uniform,u_zoom=uniform,u_placement_kind=uniform"
covering
holomorphic_live : SVec 2 -> SVec 2 -> Int -> Int ->
                   SArray 64 (SVec 2) -> SArray 64 (SVec 2) ->
                   SArray 5 (SVec 2) -> Double -> Double -> Int -> SVec 4
holomorphic_live ndc resolution zero_count pole_count zeros poles
                 coefficients time zoom placement_kind =
  let pixel = scale 0.5 (vadd (vec2 1.0 1.0) ndc)
      fragment_pixel = vec2 (x pixel * x resolution) (y pixel * y resolution)
      pixel_radius = 0.42 * minF (x resolution) (y resolution) * zoom
      point = scale (1.0 / pixel_radius)
                    (vsub fragment_pixel (scale 0.5 resolution))
      outside_radius = length (scale 0.5 resolution) / pixel_radius
      base_measure = rational_measure point (int_to_double zero_count) zeros
                                      (int_to_double pole_count) poles
      remote = remote_measure_24 point outside_radius time
      q = holomorphic_q5 point coefficients
      measure = vadd (vsub base_measure remote) (vec2 (y q) (x q))
      wegert_color = wegert_rgb_from_measure measure
      with_zero_marks = zero_overlays_from fragment_pixel resolution pixel_radius
                                           (int_to_double zero_count) zeros
                                           0.0 wegert_color
      with_pole_marks = pole_overlays_from fragment_pixel resolution pixel_radius
                                           (int_to_double pole_count) poles
                                           0.0 with_zero_marks
      color = draw_placement_controls fragment_pixel resolution placement_kind
                                      with_pole_marks
   in vec4 (x color) (y color) (z color) 1.0

main : IO ()
main = pure ()
