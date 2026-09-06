#pragma once

#include "frontend/win/SettingsPage.h"

namespace speecher::win {

// The Shortcut pane: a recorder that captures the next chord, refuses chords
// without a modifier, shows the current binding as the system writes it, a
// reset to the default, and the binder's own error when it refuses a binding.
class ShortcutRecorder {
public:
    // Appends the pane's cards to an already-titled settings column.
    static void appendPane(const winrt::Microsoft::UI::Xaml::Controls::StackPanel &column,
                           PaneHost &host);

    // Starts or ends recording, suspending the hotkey registration while it
    // runs: the bound chord is consumed system-wide and would never reach the
    // recorder (it would start a dictation instead). Balanced against the
    // binder's suspension count, so every path that ends recording — chord,
    // Escape, cancel, pane switch, window close — must come through here.
    static void setRecording(PaneHost &host, bool recording);

    // The Qt key a Windows virtual key stands for on the active keyboard
    // layout, or 0 for modifiers and keys no layout can print. Shared with the
    // setup assistant's recorder so both accept the same keys.
    static int qtKeyForVirtualKey(int virtualKey);
    static Qt::KeyboardModifiers heldModifiers();
};

} // namespace speecher::win
