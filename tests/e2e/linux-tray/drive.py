#!/usr/bin/env python3
"""D-Bus driver for the Linux tray icon E2E flow.

Reads the StatusNotifierItem the daemon registered with the rig's watcher,
checks its tooltip, icon and menu, starts and stops a dictation through the
menu over com.canonical.dbusmenu, and quits the daemon through the menu.

Evidence: every assertion prints an `assert name=... ok` line and the run ends
with E2E-RESULT: PASS|FAIL. Exits non-zero on the first failed assertion.
"""

import os
import re
import subprocess
import sys
import time

from gi.repository import Gio, GLib

OUT = os.environ["E2E_FLOW_DIR"]
APP = os.environ["E2E_APP"]
APP_ENV = dict(os.environ, APPIMAGE_EXTRACT_AND_RUN="1")
BUS = Gio.bus_get_sync(Gio.BusType.SESSION, None)

SNI_INTERFACE = "org.kde.StatusNotifierItem"
MENU_INTERFACE = "com.canonical.dbusmenu"


def log(message: str) -> None:
    print(message, flush=True)


def fail(message: str) -> None:
    log(f"assert name={message!r} ok=FALSE")
    tail = os.path.join(OUT, "app-stdio.log")
    if os.path.isfile(tail):
        with open(tail) as handle:
            log("app-stdio tail: " + " | ".join(handle.readlines()[-15:]))
    log("E2E-RESULT: FAIL")
    sys.exit(1)


def ok(message: str) -> None:
    log(f"assert name={message!r} ok=true")


def call(service, path, interface, method, params=None):
    return BUS.call_sync(service, path, interface, method, params, None,
                         Gio.DBusCallFlags.NONE, 5000, None)


def sni_property(service, path, name):
    result = call(service, path, "org.freedesktop.DBus.Properties", "Get",
                  GLib.Variant("(ss)", (SNI_INTERFACE, name)))
    return result.unpack()[0]


def tooltip_strings(service, path) -> list[str]:
    tooltip = sni_property(service, path, "ToolTip")
    return [part for part in tooltip if isinstance(part, str) and part]


def wait_for_item(timeout: float = 30):
    events = os.path.join(OUT, "sni-events.log")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.isfile(events):
            for line in open(events):
                if line.startswith("registered "):
                    _, service, path = line.split()
                    return service, path
        time.sleep(0.2)
    fail(f"no StatusNotifierItem was registered within {timeout}s")


def item_unregistered(timeout: float = 15) -> bool:
    events = os.path.join(OUT, "sni-events.log")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if any(line.startswith("unregistered ") for line in open(events)):
            return True
        time.sleep(0.2)
    return False


def menu_items(service, menu_path) -> dict[str, int]:
    call(service, menu_path, MENU_INTERFACE, "AboutToShow",
         GLib.Variant("(i)", (0,)))
    result = call(service, menu_path, MENU_INTERFACE, "GetLayout",
                  GLib.Variant("(iias)", (0, -1, [])))
    _revision, layout = result.unpack()
    items: dict[str, int] = {}

    def walk(node):
        node_id, props, children = node
        label = props.get("label", "")
        if label:
            items[label] = node_id
        for child in children:
            walk(child)

    walk(layout)
    return items


def menu_click(service, menu_path, item_id: int) -> None:
    call(service, menu_path, MENU_INTERFACE, "Event",
         GLib.Variant("(isvu)", (item_id, "clicked", GLib.Variant("s", ""), 0)))


def wait_tooltip(service, path, wanted: str, timeout: float = 20) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if wanted in tooltip_strings(service, path):
            return True
        time.sleep(0.3)
    return False


def app_status() -> str:
    result = subprocess.run([APP, "status"], env=APP_ENV,
                            capture_output=True, text=True, timeout=30)
    return result.stdout.strip()


def wait_status(wanted: str, timeout: float = 20) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if app_status() == wanted:
            return True
        time.sleep(0.5)
    return False


def main() -> None:
    service, path = wait_for_item()
    ok("the daemon registers a StatusNotifierItem on startup")
    log(f"item service={service} path={path}")

    icon_name = sni_property(service, path, "IconName")
    log(f"IconName={icon_name!r} ToolTip={tooltip_strings(service, path)!r}")
    if icon_name not in ("io.github.firemonster612.speecher",
                        "audio-input-microphone"):
        fail(f"unexpected tray icon name {icon_name!r}")
    ok("the tray icon uses the app icon (or its themed fallback)")
    if "Speecher" not in tooltip_strings(service, path):
        fail("the tray tooltip does not say Speecher")
    ok("the tray tooltip says Speecher while idle")

    menu_path = sni_property(service, path, "Menu")
    items = menu_items(service, menu_path)
    log(f"menu items={items}")
    for label in ("Start Dictation", "Settings...", "Quit"):
        if label not in items:
            fail(f"menu is missing {label!r}")
    ok("the menu offers Start Dictation, Settings... and Quit")

    menu_click(service, menu_path, items["Start Dictation"])
    if not wait_tooltip(service, path, "Speecher is listening"):
        fail(f"tooltip never showed listening; status={app_status()!r}")
    ok("Start Dictation from the tray menu starts listening")
    if not wait_status("listening"):
        fail(f"daemon state is {app_status()!r}, not listening")
    ok("the daemon reports the listening state over IPC")
    items = menu_items(service, menu_path)
    if "Stop Dictation" not in items:
        fail(f"menu did not flip to Stop Dictation while listening: {items}")
    ok("the menu flips to Stop Dictation while listening")

    menu_click(service, menu_path, items["Stop Dictation"])
    if not wait_tooltip(service, path, "Speecher"):
        fail(f"tooltip never returned to idle; status={app_status()!r}")
    if not wait_status("idle"):
        fail(f"daemon state is {app_status()!r}, not idle after the stop")
    ok("Stop Dictation from the tray menu returns the daemon to idle")
    items = menu_items(service, menu_path)
    if "Start Dictation" not in items:
        fail(f"menu did not flip back to Start Dictation: {items}")
    ok("the menu flips back to Start Dictation when idle")

    pid_match = re.match(r"org\.kde\.StatusNotifierItem-(\d+)-", service)
    pid = int(pid_match.group(1)) if pid_match else None
    menu_click(service, menu_path, items["Quit"])
    if not item_unregistered():
        fail("the StatusNotifierItem is still on the bus after Quit")
    ok("Quit removes the tray item from the bus")
    if pid is not None:
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline and os.path.exists(f"/proc/{pid}"):
            time.sleep(0.3)
        if os.path.exists(f"/proc/{pid}"):
            fail(f"daemon pid {pid} is still alive after Quit")
        ok("Quit ends the daemon process")

    log("E2E-RESULT: PASS")


if __name__ == "__main__":
    main()
