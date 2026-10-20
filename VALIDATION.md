# Validation

The implementation was prepared through the GitHub connector because no shell runtime
was available in the authoring session. No local C++ build, Windows application run,
or real RTL-SDR/IPP hardware test is claimed.

Automated checks are defined in .github/workflows/ci.yml:
- C++ core regression tests on Windows and Linux, with assertions enabled in Release.
- Qt GUI object compilation with moc/uic and an isolated declaration-only IPP header.
- New regressions cover coherent-gain normalization, zero/nonfinite power,
  transient retention across GUI ticks, and peak-preserving pixel reduction.

Check the actual Actions run for results. Production Windows linkage, real IPP ABI,
USB cancellation and driver behavior, performance on the target PC, and calibrated
RF accuracy require the installed Windows dependencies and hardware.

Source baseline: b655ccb91fc57e75395fec702495819852549487
(Thomas-Wierzbicki/Intel_RTL_analyzer, feature/mouse-wheel-zoom).
