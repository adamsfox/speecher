#pragma once

#include "platform/SingleKeyShortcutBinder.h"

#include <windows.h>

class WinPlatformTests;

namespace speecher {

// Watches one physical key through raw input on a message-only window, which
// RegisterHotKey cannot do: a single key is not a hot key to Windows. Raw
// input reports the scancode identity the vocabulary's win column keys on
// (make code plus the E0 byte), arrives whatever window has focus
// (RIDEV_INPUTSINK), and involves no hook or grab, so the key keeps doing
// its normal job as well.
//
// Raw input registration is per process and usage: registering the keyboard
// usage here retargets the WM_INPUT stream the combination binder registered
// for its release detection, and vice versa. That is safe because there is
// one binding — whichever binder bound last is the one that needs the
// stream — and it is why unwatch() must never call RIDEV_REMOVE.
class WinSingleKeyShortcutBinder final : public SingleKeyShortcutBinder {
    Q_OBJECT

public:
    explicit WinSingleKeyShortcutBinder(QObject *parent = nullptr);
    ~WinSingleKeyShortcutBinder() override;

    bool supported() const override;
    QString unsupportedBindingReason(const ShortcutBinding &binding) const override;

protected:
    QString watch(const PhysicalKey &key) override;
    void unwatch() override;

private:
    friend class ::WinPlatformTests;
    static LRESULT CALLBACK messageWindowProc(HWND window,
                                               UINT message,
                                               WPARAM wParam,
                                               LPARAM lParam);
    bool registerRawInput(QString *error);
    void handleRawInput(HRAWINPUT handle);
    void handleRawInput(const RAWINPUT &input);

    HWND m_messageWindow = nullptr;
    // The vocabulary's win value while watching, 0 while not.
    int m_scancode = 0;
};

} // namespace speecher
