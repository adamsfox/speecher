Before-and-after captures of the real Qt `TranscriberPopup`, using the same input text and simulated microphone levels. Before source is `b67b7fc`; after source is `780a9a3`, matching the accepted Linux test popup at `575671e`. The after captures show the latest connected outline with rounded cutouts and shimmering sign-in renewal. Both use Qt 6.10.2, the Breeze widget style, `QT_QPA_PLATFORM=offscreen`, `QT_QPA_PLATFORMTHEME=kde`, and an isolated `XDG_CONFIG_HOME` containing BreezeLight or BreezeDark colours.

The comparison sheets arrange the original PNGs without resizing them. The blue-grey backdrop is added to make the transparent popup visible; it is not an app colour or a desktop screenshot. Animation frames are illustrative, not pixel-identical timing comparisons.

Coverage: listening with no preview, short and long live previews, transcribing, refining before and after words arrive, delivery receipt, sign-in renewal, short and long errors, permission errors, and both update banners. All captures were visually inspected. The Dictation page continues to use the waveform's standalone capsule.

The additional `before-large-font-*` / `after-large-font-*` pairs use a 48-point application font on Breeze Dark. They verify that the shared capsule preserves the baseline's readable status and receipt at large font sizes. The final popup positioning test also checks that a long error remains inside the screen with the normal bottom clearance.

The preview text sits above the compact waveform or processing label. The lower contour hugs the painted content with rounded transitions. These images retain the accepted Linux spacing; the later request for more vertical padding applies to the native macOS and Windows PR.
