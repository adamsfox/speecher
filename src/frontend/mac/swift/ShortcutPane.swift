import AppKit
import SwiftUI

// The desktop-wide dictation shortcut. The binder has always been able to
// rebind it; until now only the setup assistant asked.

/// Catches the next key press anywhere in the app, which is what recording a
/// shortcut is. A local event monitor rather than a first-responder view: the
/// combination being recorded is usually one AppKit would otherwise route to a
/// menu, and a monitor sees it before the menu does.
@MainActor
final class ShortcutRecorder: ObservableObject {
    enum Mode { case combination, singleKey }

    @Published private(set) var mode: Mode?
    var recording: Bool { mode != nil }
    private var monitor: Any?
    /// Restores the hotkey registration recording suspended. The bound
    /// combination is consumed system-wide while registered, so the monitor
    /// would never see it — pressing it would start dictation instead.
    private var restoreShortcut: (@MainActor @Sendable () -> Void)?
    /// Escape abandons the recording rather than becoming the shortcut.
    private let escapeKeyCode: UInt16 = 53

    func record(suspending model: AppModel,
                _ bind: @escaping (String, NSEvent.ModifierFlags) -> Void) {
        begin(.combination, suspending: model)
        monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
            guard let self else { return event }
            stop()
            if event.keyCode != escapeKeyCode {
                bind(event.charactersIgnoringModifiers ?? "", event.modifierFlags)
            }
            // Swallowed: the keys being recorded are the ones that would
            // otherwise do something.
            return nil
        }
    }

    /// Catches the next key of any kind — a bare modifier included, which
    /// keyDown never reports, so the mask adds flagsChanged. Escape still
    /// abandons. The callback says whether it took the key; one it does not
    /// know (a media key) leaves the recorder armed, as the Qt capture
    /// button does.
    func recordSingleKey(suspending model: AppModel,
                         _ bind: @escaping (UInt16) -> Bool) {
        begin(.singleKey, suspending: model)
        monitor = NSEvent.addLocalMonitorForEvents(matching: [.keyDown, .flagsChanged]) {
            [weak self] event in
            guard let self else { return event }
            if event.type == .flagsChanged {
                // Only a press records; the release of a modifier that was
                // already down when recording started passes by. Modifier
                // flag changes are not swallowed — hiding one from AppKit
                // would desync its idea of what is held.
                guard Self.modifierIsDown(event) else { return event }
                if bind(event.keyCode) { stop() }
                return event
            }
            if event.keyCode == escapeKeyCode {
                stop()
                return nil
            }
            if bind(event.keyCode) { stop() }
            return nil
        }
    }

    /// Whether this flagsChanged event is the press edge of the modifier its
    /// keyCode names, read from the flag rather than assumed from the edge.
    /// The side-specific NX_DEVICE* bits (IOLLEvent.h), as the binder uses:
    /// the family flag would read releasing Left Option while Right Option is
    /// held as a press of the released key.
    private static func modifierIsDown(_ event: NSEvent) -> Bool {
        let bit: UInt
        switch event.keyCode {
        case 54: bit = 0x0000_0010 // NX_DEVICERCMDKEYMASK
        case 55: bit = 0x0000_0008 // NX_DEVICELCMDKEYMASK
        case 56: bit = 0x0000_0002 // NX_DEVICELSHIFTKEYMASK
        case 60: bit = 0x0000_0004 // NX_DEVICERSHIFTKEYMASK
        case 58: bit = 0x0000_0020 // NX_DEVICELALTKEYMASK
        case 61: bit = 0x0000_0040 // NX_DEVICERALTKEYMASK
        case 59: bit = 0x0000_0001 // NX_DEVICELCTLKEYMASK
        case 62: bit = 0x0000_2000 // NX_DEVICERCTLKEYMASK
        case 57: bit = NSEvent.ModifierFlags.capsLock.rawValue
        case 63: bit = NSEvent.ModifierFlags.function.rawValue
        default: return false
        }
        return event.modifierFlags.rawValue & bit != 0
    }

    private func begin(_ newMode: Mode, suspending model: AppModel) {
        stop()
        model.beginShortcutRecording()
        restoreShortcut = { model.endShortcutRecording() }
        mode = newMode
    }

    func stop() {
        if let monitor {
            NSEvent.removeMonitor(monitor)
        }
        monitor = nil
        mode = nil
        restoreShortcut?()
        restoreShortcut = nil
    }

    deinit {
        if let monitor {
            NSEvent.removeMonitor(monitor)
        }
        // Deinitialization can run outside the main actor. Capture the cleanup
        // rather than the dying recorder, and restore on the actor it requires.
        if let restoreShortcut {
            Task { @MainActor in restoreShortcut() }
        }
    }
}

struct ShortcutPane: View {
    @ObservedObject var model: AppModel
    @StateObject private var recorder = ShortcutRecorder()

    var body: some View {
        Form {
            Section {
                LabeledContent {
                    Button(caption) {
                        recorder.record(suspending: model) { characters, flags in
                            model.bindShortcut(characters: characters, modifierFlags: flags)
                        }
                    }
                    .disabled(!model.shortcutSupported)
                } label: {
                    Text("Dictation shortcut")
                    Text("Hold it to dictate while it is down, or press and release to "
                         + "start and press again to stop.")
                }
                LabeledContent {
                    Button(singleKeyCaption) {
                        recorder.recordSingleKey(suspending: model) { keyCode in
                            model.bindSingleKey(macKeyCode: keyCode)
                        }
                    }
                    .disabled(!model.shortcutSupported)
                } label: {
                    Text("Single key")
                    Text("One key on its own, such as Right Option or F13.")
                }
                if model.shortcutNeedsAccessibility, !model.accessibilityEnabled {
                    Button("Grant Accessibility Access") { model.requestAccessibility() }
                }
            } header: {
                Text("Shortcut")
            } footer: {
                Text(footnote)
            }
        }
        .formStyle(.grouped)
        .onDisappear { recorder.stop() }
    }

    private var caption: String {
        if recorder.mode == .combination { return "Type a shortcut…" }
        return model.shortcut.isEmpty ? "Record Shortcut" : model.shortcut
    }

    private var singleKeyCaption: String {
        recorder.mode == .singleKey ? "Press a key…" : "Record a Single Key"
    }

    private var footnote: String {
        if recorder.mode == .combination {
            return "Press the keys you want, or Escape to keep the current one."
        }
        if recorder.mode == .singleKey {
            return "Press any single key — a bare modifier like Right Option works — "
                + "or Escape to keep the current one."
        }
        if !model.shortcutProblem.isEmpty { return model.shortcutProblem }
        if !model.shortcutWarning.isEmpty { return model.shortcutWarning }
        return "macOS keeps no desktop-wide shortcut registry, so this binding is Speecher's own."
    }
}
