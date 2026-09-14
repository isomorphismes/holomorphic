module HolomorphicField

import Shader.Source

%default total

-- This is the one maintained executable definition of the mathematical field
-- rendered by the Android application.  It returns (phase, log modulus, 0, 1).
-- Wegert coloring and interaction marks are deliberately outside this module.

complex_multiply : SVec 2 -> SVec 2 -> SVec 2
complex_multiply left right =
  vec2
    (x left * x right - y left * y right)
    (x left * y right + y left * x right)

holomorphic_q : SArray 5 (SVec 2) -> SVec 2 -> SVec 2
holomorphic_q coefficients point =
  let u = scale (1.0 / 3.0) point
      power_1 = u
      power_2 = complex_multiply power_1 u
      power_3 = complex_multiply power_2 u
      power_4 = complex_multiply power_3 u
      power_5 = complex_multiply power_4 u
      term_1 = complex_multiply (array_at coefficients 0.0) power_1
      term_2 = complex_multiply (array_at coefficients 1.0) power_2
      term_3 = complex_multiply (array_at coefficients 2.0) power_3
      term_4 = complex_multiply (array_at coefficients 3.0) power_4
      term_5 = complex_multiply (array_at coefficients 4.0) power_5
   in vadd term_1 (vadd term_2 (vadd term_3 (vadd term_4 term_5)))

factor_measure : SVec 2 -> SVec 2 -> SVec 2
factor_measure point factor =
  let delta = vsub point factor
      radius_squared = maxF (dot delta delta) 0.0000000000000001
      phase = atan2F (y delta) (x delta)
      log_modulus = 0.5 * logF radius_squared
   in vec2 phase log_modulus

active_factor_measure : SVec 2 -> Int -> SArray 32 (SVec 2) -> Double -> SVec 2
active_factor_measure point count factors index =
  if index < int_to_double count
     then factor_measure point (array_at factors index)
     else vec2 0.0 0.0

factor_sum_32 : SVec 2 -> Int -> SArray 32 (SVec 2) -> SVec 2
factor_sum_32 point count factors =
  let sum_0 = active_factor_measure point count factors 0.0
      sum_1 = vadd sum_0 (active_factor_measure point count factors 1.0)
      sum_2 = vadd sum_1 (active_factor_measure point count factors 2.0)
      sum_3 = vadd sum_2 (active_factor_measure point count factors 3.0)
      sum_4 = vadd sum_3 (active_factor_measure point count factors 4.0)
      sum_5 = vadd sum_4 (active_factor_measure point count factors 5.0)
      sum_6 = vadd sum_5 (active_factor_measure point count factors 6.0)
      sum_7 = vadd sum_6 (active_factor_measure point count factors 7.0)
      sum_8 = vadd sum_7 (active_factor_measure point count factors 8.0)
      sum_9 = vadd sum_8 (active_factor_measure point count factors 9.0)
      sum_10 = vadd sum_9 (active_factor_measure point count factors 10.0)
      sum_11 = vadd sum_10 (active_factor_measure point count factors 11.0)
      sum_12 = vadd sum_11 (active_factor_measure point count factors 12.0)
      sum_13 = vadd sum_12 (active_factor_measure point count factors 13.0)
      sum_14 = vadd sum_13 (active_factor_measure point count factors 14.0)
      sum_15 = vadd sum_14 (active_factor_measure point count factors 15.0)
      sum_16 = vadd sum_15 (active_factor_measure point count factors 16.0)
      sum_17 = vadd sum_16 (active_factor_measure point count factors 17.0)
      sum_18 = vadd sum_17 (active_factor_measure point count factors 18.0)
      sum_19 = vadd sum_18 (active_factor_measure point count factors 19.0)
      sum_20 = vadd sum_19 (active_factor_measure point count factors 20.0)
      sum_21 = vadd sum_20 (active_factor_measure point count factors 21.0)
      sum_22 = vadd sum_21 (active_factor_measure point count factors 22.0)
      sum_23 = vadd sum_22 (active_factor_measure point count factors 23.0)
      sum_24 = vadd sum_23 (active_factor_measure point count factors 24.0)
      sum_25 = vadd sum_24 (active_factor_measure point count factors 25.0)
      sum_26 = vadd sum_25 (active_factor_measure point count factors 26.0)
      sum_27 = vadd sum_26 (active_factor_measure point count factors 27.0)
      sum_28 = vadd sum_27 (active_factor_measure point count factors 28.0)
      sum_29 = vadd sum_28 (active_factor_measure point count factors 29.0)
      sum_30 = vadd sum_29 (active_factor_measure point count factors 30.0)
      sum_31 = vadd sum_30 (active_factor_measure point count factors 31.0)
   in sum_31

hash1 : Double -> Double
hash1 value =
  fractF (sinF (value * 127.1 + 31.7) * 43758.5453123)

remote_pole_direction : Double -> SVec 2
remote_pole_direction index =
  let angle = 6.2831853 * hash1 (index + 0.11)
   in vec2 (cosF angle) (sinF angle)

remote_pole_radius_scale : Double -> Double
remote_pole_radius_scale index =
  mixF 2.75 4.00 (hash1 (index + 1.73))

remote_pole_orbit_speed : Double -> Double
remote_pole_orbit_speed index =
  mixF 0.11 0.19 (hash1 (index + 4.37))

remote_pole_handedness : Double -> Double
remote_pole_handedness index =
  if hash1 (index + 7.91) < 0.5 then -1.0 else 1.0

remote_pole_position : Double -> Double -> Double -> SVec 2
remote_pole_position index view_outer_radius remote_pole_time =
  let initial_direction = remote_pole_direction index
      initial_angle = atan2F (y initial_direction) (x initial_direction)
      angle = initial_angle
            + remote_pole_handedness index
            * remote_pole_orbit_speed index
            * remote_pole_time
      bend_phase = 6.2831853 * hash1 (index + 11.23)
      radial_bend = 0.22 * sinF (2.0 * angle + bend_phase)
      radius = remote_pole_radius_scale index + radial_bend
      ellipticity = mixF (-0.08) 0.08 (hash1 (index + 14.67))
      orbit = vec2
        ((1.0 + ellipticity) * cosF angle)
        ((1.0 - ellipticity) * sinF angle)
   in scale (view_outer_radius * radius) orbit

remote_pole_measure : SVec 2 -> Double -> Double -> Double -> SVec 2
remote_pole_measure point view_outer_radius remote_pole_time index =
  factor_measure point (remote_pole_position index view_outer_radius remote_pole_time)

remote_pole_sum_24 : SVec 2 -> Double -> Double -> SVec 2
remote_pole_sum_24 point view_outer_radius remote_pole_time =
  let sum_0 = remote_pole_measure point view_outer_radius remote_pole_time 0.0
      sum_1 = vadd sum_0 (remote_pole_measure point view_outer_radius remote_pole_time 1.0)
      sum_2 = vadd sum_1 (remote_pole_measure point view_outer_radius remote_pole_time 2.0)
      sum_3 = vadd sum_2 (remote_pole_measure point view_outer_radius remote_pole_time 3.0)
      sum_4 = vadd sum_3 (remote_pole_measure point view_outer_radius remote_pole_time 4.0)
      sum_5 = vadd sum_4 (remote_pole_measure point view_outer_radius remote_pole_time 5.0)
      sum_6 = vadd sum_5 (remote_pole_measure point view_outer_radius remote_pole_time 6.0)
      sum_7 = vadd sum_6 (remote_pole_measure point view_outer_radius remote_pole_time 7.0)
      sum_8 = vadd sum_7 (remote_pole_measure point view_outer_radius remote_pole_time 8.0)
      sum_9 = vadd sum_8 (remote_pole_measure point view_outer_radius remote_pole_time 9.0)
      sum_10 = vadd sum_9 (remote_pole_measure point view_outer_radius remote_pole_time 10.0)
      sum_11 = vadd sum_10 (remote_pole_measure point view_outer_radius remote_pole_time 11.0)
      sum_12 = vadd sum_11 (remote_pole_measure point view_outer_radius remote_pole_time 12.0)
      sum_13 = vadd sum_12 (remote_pole_measure point view_outer_radius remote_pole_time 13.0)
      sum_14 = vadd sum_13 (remote_pole_measure point view_outer_radius remote_pole_time 14.0)
      sum_15 = vadd sum_14 (remote_pole_measure point view_outer_radius remote_pole_time 15.0)
      sum_16 = vadd sum_15 (remote_pole_measure point view_outer_radius remote_pole_time 16.0)
      sum_17 = vadd sum_16 (remote_pole_measure point view_outer_radius remote_pole_time 17.0)
      sum_18 = vadd sum_17 (remote_pole_measure point view_outer_radius remote_pole_time 18.0)
      sum_19 = vadd sum_18 (remote_pole_measure point view_outer_radius remote_pole_time 19.0)
      sum_20 = vadd sum_19 (remote_pole_measure point view_outer_radius remote_pole_time 20.0)
      sum_21 = vadd sum_20 (remote_pole_measure point view_outer_radius remote_pole_time 21.0)
      sum_22 = vadd sum_21 (remote_pole_measure point view_outer_radius remote_pole_time 22.0)
      sum_23 = vadd sum_22 (remote_pole_measure point view_outer_radius remote_pole_time 23.0)
   in sum_23

%export "glsles:fragment|v_ndc=in,u_resolution=uniform,u_zero_count=uniform,u_pole_count=uniform,u_zero_positions=uniform,u_pole_positions=uniform,u_holomorphic_coefficients=uniform,u_remote_pole_time=uniform,u_zoom=uniform"
holomorphic_field : SVec 2 -> SVec 2 -> Int -> Int ->
                    SArray 32 (SVec 2) -> SArray 32 (SVec 2) ->
                    SArray 5 (SVec 2) -> Double -> Double -> SVec 4
holomorphic_field ndc resolution zero_count pole_count
                  zero_positions pole_positions coefficients
                  remote_pole_time zoom =
  let pixel_radius = 0.42 * minF (x resolution) (y resolution) * zoom
      pixel = vec2
        (0.5 * x resolution * x ndc)
        (0.5 * y resolution * y ndc)
      point = scale (1.0 / pixel_radius) pixel
      view_outer_radius = length (scale 0.5 resolution) / pixel_radius
      zero_measure = factor_sum_32 point zero_count zero_positions
      pole_measure = factor_sum_32 point pole_count pole_positions
      remote_measure = remote_pole_sum_24 point view_outer_radius remote_pole_time
      divisor_measure = vsub (vsub zero_measure pole_measure) remote_measure
      q = holomorphic_q coefficients point
      q_measure = vec2 (y q) (x q)
      measure = vadd divisor_measure q_measure
   in vec4 (x measure) (y measure) 0.0 1.0

main : IO ()
main = pure ()
