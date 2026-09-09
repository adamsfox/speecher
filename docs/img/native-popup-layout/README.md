# Native popup layout evidence

Windows PNGs were captured from the composited Windows 11 VM desktop using `panelEvidenceGrabsForDocumentation` in `tests/test_win_frontend.cpp`. Both versions received a quiet level of 0.02 followed by forty levels of 0.7. Preview text is identical in the before and after images.

- Before: production popup source from `b67b7fc`, rebuilt with the current capture driver and read-only geometry probes.
- After: `afcc7bd`, built with MSVC and Qt 6.8.3. WinUI draws the rounded acrylic borders. The screenshot includes the desktop visible between and around the two capsules.
- The notice screenshot enables the existing What's New state in temporary test preferences. It includes the notice's own window above the waveform.

The after build passed all 21 CTest suites. On the interactive desktop, 19 native frontend tests passed, with one intentionally gated visual driver skipped. The banner-enabled run had the same result. Against the baseline, the fifteen-bar assertion and separate-preview geometry assertion failed as expected.

The captures cover speaking with and without preview, transcribing, streamed refinement, the delivery receipt, an error with Dismiss, and the notice banner. Test preferences are temporary and separate from the installed application's settings.
