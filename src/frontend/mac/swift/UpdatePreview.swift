import AppKit
import SwiftUI

// Offscreen PNGs of the update UI states, for the PR that documents them. The
// runner has no display and no signed appcast, so each state is rendered from
// its real view with seeded values through ImageRenderer rather than driven
// through a live update. Test-only: nothing in the app calls this.

/// One captured state: the file it lands in and the view rendered into it.
@MainActor
@objc public final class SpeecherUpdatePreview: NSObject {
    /// Renders the five update states as PNGs into `directory`, returning the
    /// file names written. Empty on the first failure.
    @MainActor
    @objc public static func render(toDirectory directory: String) -> [String] {
        let base = URL(fileURLWithPath: directory, isDirectory: true)
        try? FileManager.default.createDirectory(at: base, withIntermediateDirectories: true)

        let available = AppModel.UpdateStatus(state: .updateAvailable, version: "0.2.0")
        let ready = AppModel.UpdateStatus(state: .readyToRestart)

        let updateChipState = DictationPanelState()
        updateChipState.status = "Listening"
        updateChipState.preview = "the quick brown fox"
        updateChipState.updateChip = "Speecher 0.2.0 available — install and restart"
        updateChipState.updateChipEnabled = true

        let whatsNewChipState = DictationPanelState()
        whatsNewChipState.status = "Listening"
        whatsNewChipState.preview = "the quick brown fox"
        whatsNewChipState.whatsNewChip = "Speecher 0.2.0 installed — see what's new"

        let jobs: [(String, CGSize, AnyView)] = [
            ("01-settings-banner-update-available.png", CGSize(width: 640, height: 90),
             AnyView(card(UpdateBannerContent(update: available).row))),
            ("02-settings-banner-ready-to-restart.png", CGSize(width: 640, height: 90),
             AnyView(card(UpdateBannerContent(update: ready).row))),
            ("03-settings-whats-new-strip.png", CGSize(width: 640, height: 90),
             AnyView(card(WhatsNewStrip(installedNumber: "0.2.0").row))),
            ("04-panel-update-chip.png", CGSize(width: 520, height: 140),
             AnyView(panel(updateChipState))),
            ("05-panel-whats-new-chip.png", CGSize(width: 520, height: 140),
             AnyView(panel(whatsNewChipState))),
        ]

        var written: [String] = []
        for (name, size, view) in jobs {
            guard writePNG(view, size: size, to: base.appendingPathComponent(name)) else {
                return []
            }
            written.append(name)
        }
        return written
    }

    /// A banner or strip row on a rounded card. The settings window draws these
    /// rows in a native GroupBox, which ImageRenderer leaves blank offscreen, so
    /// the capture wraps the same row in a material card it can rasterise.
    @MainActor
    private static func card(_ row: some View) -> some View {
        row
            .padding(.horizontal, 14)
            .padding(.vertical, 10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 8))
            .padding()
    }

    /// The dictation panel view with no-op actions, at rest on the live phase.
    @MainActor
    private static func panel(_ state: DictationPanelState) -> some View {
        DictationPanelView(state: state,
                           dismiss: {},
                           installUpdate: {},
                           openWhatsNew: {},
                           dismissWhatsNew: {})
            .padding()
    }

    @MainActor
    private static func writePNG(_ view: some View, size: CGSize, to url: URL) -> Bool {
        // A solid backing so a material that would be blank offscreen still
        // reads as a card rather than a transparent hole.
        let renderer = ImageRenderer(content: view
            .frame(width: size.width, height: size.height)
            .background(Color(nsColor: .windowBackgroundColor)))
        renderer.scale = 2
        guard let image = renderer.nsImage,
              let tiff = image.tiffRepresentation,
              let rep = NSBitmapImageRep(data: tiff),
              let png = rep.representation(using: .png, properties: [:]) else {
            return false
        }
        return (try? png.write(to: url)) != nil
    }
}
