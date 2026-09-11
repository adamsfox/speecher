#include "platform/win/WinSingleKeyShortcutBinder.h"

#include "platform/win/WinInjectedInput.h"

namespace speecher {
namespace {

constexpr auto messageWindowClass = L"SpeecherSingleKeyRawInput";

} // namespace

WinSingleKeyShortcutBinder::WinSingleKeyShortcutBinder(QObject *parent)
    : SingleKeyShortcutBinder(parent)
{
}

WinSingleKeyShortcutBinder::~WinSingleKeyShortcutBinder()
{
    if (m_messageWindow) {
        DestroyWindow(m_messageWindow);
    }
}

bool WinSingleKeyShortcutBinder::supported() const
{
    return true;
}

QString WinSingleKeyShortcutBinder::unsupportedBindingReason(const ShortcutBinding &binding) const
{
    const QString reason = SingleKeyShortcutBinder::unsupportedBindingReason(binding);
    if (!reason.isEmpty()) {
        return reason;
    }
    if (physicalKey(binding.keyCode())->win < 0) {
        return QStringLiteral("Windows does not report the %1 key.").arg(binding.displayText());
    }
    return QString();
}

QString WinSingleKeyShortcutBinder::watch(const PhysicalKey &key)
{
    QString error;
    if (!registerRawInput(&error)) {
        return error;
    }
    m_scancode = key.win;
    return QString();
}

void WinSingleKeyShortcutBinder::unwatch()
{
    // Raw input stays registered: removing the keyboard usage would also cut
    // off the combination binder's release detection (see the header).
    m_scancode = 0;
}

bool WinSingleKeyShortcutBinder::registerRawInput(QString *error)
{
    if (!m_messageWindow) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = messageWindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = messageWindowClass;
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            if (error) {
                *error = QStringLiteral("Windows could not create the key listener");
            }
            return false;
        }
        m_messageWindow = CreateWindowExW(0,
                                          messageWindowClass,
                                          L"",
                                          0,
                                          0,
                                          0,
                                          0,
                                          0,
                                          HWND_MESSAGE,
                                          nullptr,
                                          instance,
                                          this);
        if (!m_messageWindow) {
            if (error) {
                *error = QStringLiteral("Windows could not create the key listener");
            }
            return false;
        }
    }
    RAWINPUTDEVICE keyboard{0x01, 0x06, RIDEV_INPUTSINK, m_messageWindow};
    if (!RegisterRawInputDevices(&keyboard, 1, sizeof(keyboard))) {
        if (error) {
            *error = QStringLiteral("Windows could not watch the key");
        }
        return false;
    }
    return true;
}

LRESULT CALLBACK WinSingleKeyShortcutBinder::messageWindowProc(HWND window,
                                                                UINT message,
                                                                WPARAM wParam,
                                                                LPARAM lParam)
{
    auto *binder = reinterpret_cast<WinSingleKeyShortcutBinder *>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        binder = static_cast<WinSingleKeyShortcutBinder *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(binder));
    } else if (message == WM_INPUT && binder) {
        binder->handleRawInput(reinterpret_cast<HRAWINPUT>(lParam));
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void WinSingleKeyShortcutBinder::handleRawInput(HRAWINPUT handle)
{
    RAWINPUT input{};
    UINT size = sizeof(input);
    if (GetRawInputData(handle, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) == UINT(-1)) {
        return;
    }
    handleRawInput(input);
}

void WinSingleKeyShortcutBinder::handleRawInput(const RAWINPUT &input)
{
    if (input.header.dwType != RIM_TYPEKEYBOARD || m_scancode == 0) {
        return;
    }
    const RAWKEYBOARD &keyboard = input.data.keyboard;
    // The fake half of an escaped sequence (Pause's trailing 0x45) and
    // keyboard overrun both arrive as VKey 0xFF; neither is a key.
    if (keyboard.VKey == 0xFF) {
        return;
    }
    // Speecher's own paste injection: its WM_INPUT is queued and arrives only
    // after the delivery-scoped suspension has lifted, so the tag is what
    // keeps a binding on V or Ctrl from retriggering dictation on every paste.
    if (keyboard.ExtraInformation == injectedInputTag) {
        return;
    }
    int scancode = keyboard.MakeCode & 0x7F;
    if (keyboard.Flags & RI_KEY_E0) {
        scancode |= 0xE000;
    }
    // Raw input spells Pause as an E1-flagged 0x1D and NumLock as a bare
    // 0x45; the vocabulary holds the message-level spelling (0x45 / 0xE045).
    if (keyboard.Flags & RI_KEY_E1) {
        if (keyboard.VKey != VK_PAUSE) {
            return;
        }
        scancode = 0x45;
    } else if (scancode == 0x45) {
        scancode = 0xE045;
    }
    if (scancode != m_scancode) {
        return;
    }
    // Key repeat (repeated makes without a break) collapses in the base
    // class; on AltGr layouts the synthetic left-Ctrl make preceding right
    // Alt never reaches here for an AltRight binding, since 0x1D != 0xE038.
    if (keyboard.Flags & RI_KEY_BREAK) {
        keyUp();
    } else {
        keyDown();
    }
}

} // namespace speecher
