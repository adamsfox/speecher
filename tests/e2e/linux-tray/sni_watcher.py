#!/usr/bin/env python3
"""Minimal org.kde.StatusNotifierWatcher: just enough for Qt to publish a
QSystemTrayIcon as a StatusNotifierItem inside the headless session.

Appends events to the file given as argv[1] ("watcher-ready", then one
"registered <service> <path>" / "unregistered <service>" line per item) so the
driver can find the item without talking to this process.
"""

import sys

from gi.repository import Gio, GLib

events = open(sys.argv[1], "a", buffering=1)
items: list[str] = []

NODE = Gio.DBusNodeInfo.new_for_xml("""
<node>
  <interface name="org.kde.StatusNotifierWatcher">
    <method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
    <method name="RegisterStatusNotifierHost"><arg type="s" direction="in"/></method>
    <property name="RegisteredStatusNotifierItems" type="as" access="read"/>
    <property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
    <property name="ProtocolVersion" type="i" access="read"/>
    <signal name="StatusNotifierItemRegistered"><arg type="s"/></signal>
    <signal name="StatusNotifierItemUnregistered"><arg type="s"/></signal>
    <signal name="StatusNotifierHostRegistered"/>
  </interface>
</node>
""")


def emit(event: str) -> None:
    events.write(event + "\n")


def watch_owner(connection, service: str) -> None:
    def vanished(_connection, name: str) -> None:
        emit(f"unregistered {name}")

    Gio.bus_watch_name_on_connection(
        connection, service, Gio.BusNameWatcherFlags.NONE, None, vanished)


def handle_call(connection, sender, path, interface, method, params, invocation):
    if method == "RegisterStatusNotifierItem":
        (argument,) = params.unpack()
        # An object path means the sender owns the item; a service name means
        # the item sits at the conventional path on that service.
        if argument.startswith("/"):
            service, item_path = sender, argument
        else:
            service, item_path = argument, "/StatusNotifierItem"
        items.append(service + item_path)
        emit(f"registered {service} {item_path}")
        connection.emit_signal(
            None, path, interface, "StatusNotifierItemRegistered",
            GLib.Variant("(s)", (service + item_path,)))
        watch_owner(connection, service)
    invocation.return_value(None)


def handle_get(_connection, _sender, _path, _interface, prop):
    if prop == "RegisteredStatusNotifierItems":
        return GLib.Variant("as", items)
    if prop == "IsStatusNotifierHostRegistered":
        return GLib.Variant("b", True)
    if prop == "ProtocolVersion":
        return GLib.Variant("i", 0)
    return None


def on_bus_acquired(connection, _name):
    connection.register_object(
        "/StatusNotifierWatcher", NODE.interfaces[0], handle_call, handle_get, None)


def on_name_acquired(_connection, _name):
    emit("watcher-ready")


Gio.bus_own_name(
    Gio.BusType.SESSION, "org.kde.StatusNotifierWatcher",
    Gio.BusNameOwnerFlags.NONE, on_bus_acquired, on_name_acquired, None)
GLib.MainLoop().run()
