# Native popup layout evidence

Windows PNGs were captured from the composited Windows 11 VM desktop using `panelEvidenceGrabsForDocumentation` in `tests/test_win_frontend.cpp`. Both versions received a quiet level of 0.02 followed by forty levels of 0.7. Preview text is identical in the before and after images.

- Before: production popup source from `b67b7fc`, rebuilt with the current capture driver and read-only geometry probes.
- After: `20033eb`, built with MSVC and Qt 6.8.3. WinUI draws the rounded borders using its theme acrylic brush. The VM renders a flat fallback fill, so these captures do not establish desktop translucency. The screenshot includes the desktop visible between and around the two capsules.
- The notice screenshot enables the existing What's New state in temporary test preferences. It includes the notice's own window above the waveform.

The after build passed all 21 CTest suites. On the interactive desktop, 21 native frontend tests passed, with one intentionally gated visual driver skipped. The banner-enabled run had the same result. Against the baseline, the fifteen-bar assertion and separate-preview geometry assertion failed as expected.

The captures cover speaking with and without preview, frozen bars at 40% opacity, transcribing, streamed refinement, the delivery receipt, an error with Dismiss, and the notice banner. Test preferences are temporary and separate from the installed application's settings.

The reviewed Windows checks also cover long previews containing narrow letters and emoji sequences. The complete ellipsis and retained suffix must fit the capsule, and the newest words must remain intact. Error capsules retain the available screen-width budget.

## macOS captures

The macOS PNGs are native panel captures from the existing macOS 26 panel-flow workflow. Baseline run [34398156576](https://github.com/firemonster612/speecher/actions/runs/34398156576) used `b67b7fc`; after run [34400730082](https://github.com/firemonster612/speecher/actions/runs/34400730082) used `d513ed2`, the final Swift source. Both workflows succeeded. The final panel-flow, banner-stack and Dictation-pane verdicts all passed.

`macos-comparison.png` pairs listening, speech preview, transcribing, refining, streamed refinement and receipt. Frames were selected by state from the same scripted flow; animation timing and receipt delivery details differ between runs. The original transparent PNGs are included. Comparison sheets composite transparency onto a blue-grey background for readability; that colour is not an app change.

Final macOS frames were inspected alongside the baseline. Windows comparison pairs cover the gauge replacement and the separate transcript capsule; the remaining Windows after frames document processing, errors, frozen waveform and notices.
