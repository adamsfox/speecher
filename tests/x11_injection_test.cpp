// Proves the XInput2 single-key backend end to end: inject a right Alt press
// and release through XTEST and show that activated() then deactivated() fire.
// Runs under Xvfb in CI, never against a real session: XTEST here would type
// into whatever the user has focused. CMake only registers this test where
// Xvfb is present, and it is launched through xvfb-run on a private display.

#include "core/ShortcutBinding.h"
#include "platform/GlobalShortcutBinder.h"
#include "platform/XInput2ShortcutBinder.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTimer>

#include <cstdio>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

using namespace speecher;

namespace {

// Right Alt is X11 keycode 108 (evdev 100 + 8), which is exactly the key the
// binder computes from the "AltRight" vocabulary row.
constexpr int rightAltKeycode = 108;

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    XInput2ShortcutBinder binder;
    if (!binder.supported()) {
        std::fprintf(stderr, "XInput2 binder could not reach the X server\n");
        return 1;
    }

    QString error;
    if (!binder.setShortcut(ShortcutBinding::singleKey(QStringLiteral("AltRight")), &error)) {
        std::fprintf(stderr, "could not watch AltRight: %s\n", qPrintable(error));
        return 1;
    }

    QSignalSpy activated(&binder, &GlobalShortcutBinder::activated);
    QSignalSpy deactivated(&binder, &GlobalShortcutBinder::deactivated);

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        std::fprintf(stderr, "could not open a second connection for injection\n");
        return 1;
    }

    QTimer::singleShot(200, [display] {
        XTestFakeKeyEvent(display, rightAltKeycode, True, 0);
        XFlush(display);
    });
    QTimer::singleShot(400, [display] {
        XTestFakeKeyEvent(display, rightAltKeycode, False, 0);
        XFlush(display);
    });

    if (activated.count() == 0) {
        activated.wait(3000);
    }
    if (deactivated.count() == 0) {
        deactivated.wait(3000);
    }
    XCloseDisplay(display);

    if (activated.count() != 1 || deactivated.count() != 1) {
        std::fprintf(stderr, "expected one activated and one deactivated; got %lld and %lld\n",
                     static_cast<long long>(activated.count()),
                     static_cast<long long>(deactivated.count()));
        return 1;
    }
    std::puts("XInput2 backend: activated() then deactivated() fired on injected right Alt");
    return 0;
}
