#include "output/win/WinPasteDelivery.h"

#include <windows.h>

namespace speecher {

bool WinPasteDelivery::paste(PasteMethod method, QString *error)
{
    const HWND foreground = GetForegroundWindow();
    // SendInput preserves the existing keyboard state. Held keys must not
    // turn paste into another shortcut, or be released by our synthetic ups.
    for (const int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, int('V')}) {
        if (GetAsyncKeyState(key) & 0x8000) {
            if (error) {
                *error = QStringLiteral("Release the held keys before pasting");
            }
            return false;
        }
    }

    INPUT input[6]{};
    int count = 0;
    const auto append = [&input, &count](WORD key, DWORD flags) {
        input[count].type = INPUT_KEYBOARD;
        input[count].ki.wVk = key;
        input[count].ki.dwFlags = flags;
        ++count;
    };
    append(VK_CONTROL, 0);
    if (method == PasteMethod::TerminalPaste) {
        append(VK_SHIFT, 0);
    }
    append('V', 0);
    append('V', KEYEVENTF_KEYUP);
    if (method == PasteMethod::TerminalPaste) {
        append(VK_SHIFT, KEYEVENTF_KEYUP);
    }
    append(VK_CONTROL, KEYEVENTF_KEYUP);

    if (!foreground || GetForegroundWindow() != foreground) {
        if (error) {
            *error = QStringLiteral("The focused window changed before paste");
        }
        return false;
    }
    const UINT sent = SendInput(count, input, sizeof(INPUT));
    if (sent == UINT(count)) {
        return true;
    }

    // A partial batch may contain a down without its matching up. Balance
    // only those keys, never keys that were held before this paste attempt.
    INPUT releases[3]{};
    UINT releaseCount = 0;
    for (UINT index = 0; index < sent; ++index) {
        if (input[index].ki.dwFlags & KEYEVENTF_KEYUP) {
            continue;
        }
        bool released = false;
        for (UINT later = index + 1; later < sent; ++later) {
            released |= input[later].ki.wVk == input[index].ki.wVk
                && (input[later].ki.dwFlags & KEYEVENTF_KEYUP);
        }
        if (!released) {
            releases[releaseCount] = input[index];
            releases[releaseCount++].ki.dwFlags = KEYEVENTF_KEYUP;
        }
    }
    if (releaseCount) {
        SendInput(releaseCount, releases, sizeof(INPUT));
    }
    if (error) {
        *error = QStringLiteral("Windows could not send the paste keystroke");
    }
    return false;
}

} // namespace speecher
