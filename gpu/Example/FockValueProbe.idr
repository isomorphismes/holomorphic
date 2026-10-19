module Example.FockValueProbe

-- Whole-plane Bargmann--Fock candidate for Holomorphic's independently
-- specified minimum-norm perturbation. This is a generated shader *probe*,
-- not an alternative handwritten implementation of the live renderer.
-- See docs/holomorphic-mathematical-contract.md, sections 7-10.

import Shader.Source

%default total

complex_multiply : SVec 2 -> SVec 2 -> SVec 2
complex_multiply left right =
  vec2 (x left * x right - y left * y right)
       (x left * y right + y left * x right)

complex_exp : SVec 2 -> SVec 2
complex_exp value =
  let modulus = expF (x value)
   in vec2 (modulus * cosF (y value))
           (modulus * sinF (y value))

||| Unique minimum Fock-norm value direction on the closed subspace q(0)=0.
|||
||| phi_(a,0)(z) = (exp(z conj(a)/s^2)-1) / (exp(|a|^2/s^2)-1).
|||
||| The HOST must verify s > 0 and a != 0, and must restrict numerical
||| arguments sufficiently to avoid overflow and cancellation. These
||| requirements cannot be silently replaced with a GPU clamp or an
||| arbitrary 'safe' polynomial.
fock_gauged_value_direction : SVec 2 -> SVec 2 -> Double -> SVec 2
fock_gauged_value_direction point anchor scale_value =
  let scale_squared = scale_value * scale_value
      conjugate_anchor = vec2 (x anchor) (-(y anchor))
      exponent = scale (1.0 / scale_squared)
                       (complex_multiply point conjugate_anchor)
      numerator = vsub (complex_exp exponent) (vec2 1.0 0.0)
      denominator = expF (dot anchor anchor / scale_squared) - 1.0
   in scale (1.0 / denominator) numerator

||| The generated fragment exposes the actual complex q descriptor for
||| later framebuffer readback. RGB encoding is a diagnostic, not Wegert colour.
%export "glsles:fragment|v_ndc=in,u_anchor=uniform,u_scale=uniform,u_amplitude=uniform"
fock_value_probe : SVec 2 -> SVec 2 -> Double -> SVec 2 -> SVec 4
fock_value_probe point anchor scale_value amplitude =
  let direction = fock_gauged_value_direction point anchor scale_value
      q = complex_multiply amplitude direction
   in vec4 (0.5 + 0.25 * x q) (0.5 + 0.25 * y q) 0.25 1.0

main : IO ()
main = pure ()
