import CoreGraphics
import Foundation

// Posts keyboard events from outside Speecher's process — exactly the input
// the single-key binder's self-PID filter must let through (it drops only
// events Speecher itself posted). setup_run.sh seeds kTCCServicePostEvent for
// this binary's path so the posts are permitted.
//
//   key-post right-option down|up|tap
//   key-post control-option-d

let rightOption: CGKeyCode = 61 // kVK_RightOption
let keyD: CGKeyCode = 2 // kVK_ANSI_D
// IOLLEvent.h's NX_DEVICERALTKEYMASK: the device-dependent flag bit that
// tells right Option from left, which the binder keys on.
let rightOptionDeviceBit: UInt64 = 0x40
// A modifier release carries only the non-coalesced marker.
let neutralFlags = CGEventFlags(rawValue: 0x100)

func usage() -> Never {
    FileHandle.standardError.write(Data(
        "usage: key-post right-option down|up|tap | control-option-d\n".utf8))
    exit(64)
}

func post(_ event: CGEvent?) {
    guard let event else {
        FileHandle.standardError.write(Data("could not create the event\n".utf8))
        exit(2)
    }
    event.post(tap: .cghidEventTap)
}

// Bare modifiers reach NSEvent monitors as flagsChanged; the key code rides on
// the event and the press edge is the flag bit, which is exactly what the
// binder and the recorders read.
func rightOptionEvent(down: Bool) -> CGEvent? {
    let event = CGEvent(keyboardEventSource: nil, virtualKey: rightOption, keyDown: down)
    event?.type = .flagsChanged
    event?.flags = down
        ? CGEventFlags(rawValue: CGEventFlags.maskAlternate.rawValue | rightOptionDeviceBit)
        : neutralFlags
    return event
}

func chordEvent(down: Bool) -> CGEvent? {
    let event = CGEvent(keyboardEventSource: nil, virtualKey: keyD, keyDown: down)
    event?.flags = [.maskControl, .maskAlternate]
    return event
}

// AppKit tracks modifier state from the event stream; ending a synthetic chord
// with a neutral flagsChanged keeps it from thinking ⌃⌥ stuck down.
func neutralModifiersEvent() -> CGEvent? {
    let event = CGEvent(keyboardEventSource: nil, virtualKey: 59, keyDown: false)
    event?.type = .flagsChanged
    event?.flags = neutralFlags
    return event
}

let arguments = CommandLine.arguments
switch arguments.count > 1 ? arguments[1] : "" {
case "right-option":
    switch arguments.count > 2 ? arguments[2] : "" {
    case "down":
        post(rightOptionEvent(down: true))
    case "up":
        post(rightOptionEvent(down: false))
    case "tap":
        post(rightOptionEvent(down: true))
        usleep(60000)
        post(rightOptionEvent(down: false))
    default:
        usage()
    }
case "control-option-d":
    post(chordEvent(down: true))
    usleep(60000)
    post(chordEvent(down: false))
    post(neutralModifiersEvent())
default:
    usage()
}
