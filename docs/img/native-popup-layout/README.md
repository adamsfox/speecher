# Native popup layout evidence

Both platforms use the new Linux layout from PR95: waveform or processing status on the left and latest preview text on the right, inside one capsule. Empty previews leave a compact waveform pill. Original native before images use `b67b7fc`.

## Windows

After PNGs were captured from the composited Windows 11 VM desktop using final Windows source `313b255`, MSVC and Qt 6.8.3. Before and after received the same quiet input followed by forty speech-level samples and identical preview text. Temporary test preferences keep captures separate from the installed app.

WinUI draws the rounded Border with its theme-aware in-app acrylic brush. The VM renders a flat fill, so these images establish layout and shape, not desktop translucency. The visible blue corners are desktop wallpaper.

The build and all 21 CTest suites passed. Native desktop tests passed 21 cases with one deliberately gated visual driver skipped, both with and without notices. The new shared-capsule containment check fails against the stacked layout and passes against the final source. Long previews with narrow letters and emoji retain their newest text and fit the available width.

Captures cover listening, preview, frozen bars, transcribing, streamed refinement, receipt, error with Dismiss and notices.

## macOS

The final Swift source is `1ea9262`. Native macOS 26 captures come from [workflow34461408821](https://github.com/firemonster612/speecher/actions/runs/34461408821), on commit `313b255`. The build, tests and capture workflow passed; panel-flow, banner-stack and Dictation-pane verdicts all passed. Baseline captures come from [workflow34398156576](https://github.com/firemonster612/speecher/actions/runs/34398156576) at `b67b7fc`.

The comparison pairs listening, speech preview, transcribing, refining, streamed refinement and receipt. Original transparent PNGs are included. Frames match states from the same scripted flow; animation timing and delivery details differ between runs. Comparison sheets preserve the source pixel scale and composite transparency onto a blue-grey background for readability. That background is not an app colour.

All final captures and both comparison sheets were inspected.
