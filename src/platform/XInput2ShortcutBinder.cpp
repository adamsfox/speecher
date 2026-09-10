#include "platform/XInput2ShortcutBinder.h"

#include <QSocketNotifier>

// Xlib last: its macros (None, Bool, KeyPress) collide with Qt names.
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

namespace speecher {
namespace {

// X keycodes are one byte; the vocabulary's Fn key (evdev 464) lies past them.
constexpr int x11KeycodeOffset = 8;
constexpr int x11MaxKeycode = 255;

} // namespace

XInput2ShortcutBinder::XInput2ShortcutBinder(QObject *parent)
    : SingleKeyShortcutBinder(parent)
{
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        return;
    }
    int firstEvent = 0;
    int firstError = 0;
    int major = 2;
    int minor = 0;
    if (!XQueryExtension(m_display, "XInputExtension", &m_xiOpcode, &firstEvent, &firstError)
        || XIQueryVersion(m_display, &major, &minor) != Success) {
        XCloseDisplay(m_display);
        m_display = nullptr;
        return;
    }
    m_notifier = new QSocketNotifier(ConnectionNumber(m_display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] { readEvents(); });
}

XInput2ShortcutBinder::~XInput2ShortcutBinder()
{
    if (m_display) {
        XCloseDisplay(m_display);
    }
}

bool XInput2ShortcutBinder::supported() const
{
    return m_display != nullptr;
}

QString XInput2ShortcutBinder::unsupportedBindingReason(const ShortcutBinding &binding) const
{
    const QString reason = SingleKeyShortcutBinder::unsupportedBindingReason(binding);
    if (!reason.isEmpty()) {
        return reason;
    }
    if (!m_display) {
        return QStringLiteral("Speecher could not reach the X server to watch a key.");
    }
    if (physicalKey(binding.keyCode())->evdev + x11KeycodeOffset > x11MaxKeycode) {
        return QStringLiteral("X11 does not report the %1 key.").arg(binding.displayText());
    }
    return QString();
}

QString XInput2ShortcutBinder::watch(const PhysicalKey &key)
{
    m_keycode = key.evdev + x11KeycodeOffset;
    selectRawKeyEvents(true);
    return QString();
}

void XInput2ShortcutBinder::unwatch()
{
    if (m_keycode == 0) {
        return;
    }
    m_keycode = 0;
    selectRawKeyEvents(false);
}

void XInput2ShortcutBinder::selectRawKeyEvents(bool select)
{
    unsigned char bits[XIMaskLen(XI_LASTEVENT)] = {};
    if (select) {
        XISetMask(bits, XI_RawKeyPress);
        XISetMask(bits, XI_RawKeyRelease);
    }
    XIEventMask mask;
    mask.deviceid = XIAllMasterDevices;
    mask.mask_len = sizeof(bits);
    mask.mask = bits;
    XISelectEvents(m_display, DefaultRootWindow(m_display), &mask, 1);
    XFlush(m_display);
}

// Text delivery may have injected the watched key (ydotool's Ctrl+V reaches
// the server as real input); the round trip makes sure those events are here
// before they are dropped.
void XInput2ShortcutBinder::resuming()
{
    if (!m_display) {
        return;
    }
    XSync(m_display, False);
    while (XPending(m_display)) {
        XEvent event;
        XNextEvent(m_display, &event);
    }
}

void XInput2ShortcutBinder::readEvents()
{
    while (XPending(m_display)) {
        XEvent event;
        XNextEvent(m_display, &event);
        XGenericEventCookie *cookie = &event.xcookie;
        if (cookie->type != GenericEvent || cookie->extension != m_xiOpcode
            || !XGetEventData(m_display, cookie)) {
            continue;
        }
        const auto *raw = static_cast<const XIRawEvent *>(cookie->data);
        if (raw->detail == m_keycode && !(raw->flags & XIKeyRepeat)) {
            if (cookie->evtype == XI_RawKeyPress) {
                keyDown();
            } else if (cookie->evtype == XI_RawKeyRelease) {
                keyUp();
            }
        }
        XFreeEventData(m_display, cookie);
    }
}

} // namespace speecher
