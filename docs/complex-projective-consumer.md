# Complex/projective arithmetic consumer boundary

`analytic-continuation` is a live consumer of the shared complex arithmetic semantics, not an owner of the language-level `Complex`, C^n, or CP^n types.

The live application keeps its intended whole-plane model:

```text
f(z) = R(z) H(z)
H(z) = exp(q(z))
```

`R` carries the explicit finite zero/pole divisor. `q` is a polynomial in the normalized coordinate `u = z / 6`, hence is entire in `z`. Therefore `exp(q)` is entire and nonzero and does not add finite zeros or poles.

The native walk already evolves the complex coefficients of `q` directly. Its `direction_at` routine uses polynomial complex multiplication; magnitude is used outside that polynomial evaluation for direction budgeting/scoring, not to mutate `q` into a non-holomorphic function.

The fragment renderer independently evaluates the same polynomial `q`, then adds `Re(q)` to log modulus and `Im(q)` to phase. This is exactly the observational identity for multiplying by `exp(q)`; it is not an insertion of `log`, conjugation, or magnitude into the holomorphic factor.

## Shared acceptance relation

The unit-test workflow checks out the current canonical complex/projective corpus from `isomorphisms/Idric`. The application fixture consumes the shared:

- `f(z)=R(z)*exp(q(z))` field model;
- explicit zero and pole used by the cross-backend reference scene.

The application uses its own live `q` coefficients because its random walk has a coefficient-budget/normalization policy in `u=z/6`. That policy belongs to the application and should not be moved into the compiler or x86 backend merely to force identical pixels.

The application test verifies that its fixture stays within the live coefficient budget, that sampled `exp(q)` values remain finite and nonzero, and that the source segment which evaluates `q` contains no phase/log/magnitude/conjugation/derivative observation.

## Projective relation

The ordinary live viewport currently consumes finite complex `z` values. It does not need to store the CP^1 point at infinity as interaction state. The shared CP^1 semantics are nevertheless the right mathematical home for the extended complex plane and may be used by a future meromorphic API or serialization boundary.

This job does not add an artificial projective runtime object to the Android renderer simply to advertise CP^1 integration. The consumer receipt records the CP^1 runtime stage as `SKIP` until an actual application operation requires it.

## Repository separation

No lasso, overlapping-disc, continuation-path, Riemann-surface, or lacunary machinery is reintroduced here. Those experiments remain outside this random whole-plane holomorphic explorer.
