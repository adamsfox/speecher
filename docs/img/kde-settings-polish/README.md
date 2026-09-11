# KDE settings captures

Before images use `b67b7fc`. After images use `70985d2` for the shared settings changes and `4e2fc06` for the final Accounts layout (captured pre-rebase as `95acb6e` and `3d188d4`; the rebase onto the merged popup work did not touch these files).

These are actual Qt Widgets renders under headless KWin with the KDE platform theme and Breeze style, in isolated configuration directories. Both Breeze light and dark are included for all seven pages and for CLI Proxy account mode. Default windows are 900×640; the Audio small-window pairs use 760×520.

The final source built successfully. The isolated rig passed 22 of 23 CTest suites; two updater assertions fail identically on the base. Linux, macOS and Windows GitHub checks pass for the final head. All captures were inspected.
