# Deterministic meromorphic-divisor oracle

This directory is a host-side mathematical oracle for issue #54. It is not a
GPU renderer and it is not physical-device evidence.

The acceptance order is deliberate:

1. One stationary pole and one zero at exactly the same coordinate are reduced
   algebraically before evaluation. Their field is exactly the field with
   neither factor, including at the removable point.
2. A slightly displaced zero/pole pair is not reduced. Its phase and log
   modulus must agree with the direct complex ratio.
3. Twenty-four exact pairs reduce to the empty divisor, and twenty-four
   displaced pairs must still agree with the direct ratio product.
4. Only an evaluator satisfying those cases is worth comparing with moving
   wandering poles and then optimizing on a physical GPU.

The regression tests also retain two broken algorithms only as negative
controls:

- divisor contributions accumulated in fp32 before a large `q` can disappear
  when `q` is added last;
- a raw fp32 product of 24 pole factors can overflow before phase/log modulus
  are extracted.

Those implementations are not part of the oracle and are not architectural
constraints. Any later optimization has to agree with the reference
`evaluate_phase_log` behavior first.
