#include "frontend/qt/LinuxTrayIcon.h"

#include "app/ApplicationController.h"
#include "dictation/DictationSession.h"

#include <QAction>
#include <QIcon>
#include <QSystemTrayIcon>

namespace speecher {

namespace {

QIcon trayIcon(bool listening)
{
    if (listening) {
        return QIcon::fromTheme(QStringLiteral("media-record"));
    }
    // The app icon lands in hicolor once Speecher is installed; a themed
    // microphone stands in until then (the AppImage bundles breeze as the
    // fallback theme, so both names resolve there too).
    const QIcon icon = QIcon::fromTheme(QStringLiteral("io.github.firemonster612.speecher"));
    return icon.isNull() ? QIcon::fromTheme(QStringLiteral("audio-input-microphone")) : icon;
}

} // namespace

LinuxTrayIcon::LinuxTrayIcon(ApplicationController *controller, QObject *parent)
    : QObject(parent)
    , m_tray(new QSystemTrayIcon(this))
{
    m_toggleAction = m_menu.addAction(QStringLiteral("Start Dictation"),
                                      controller, &ApplicationController::toggle);
    m_toggleAction->setObjectName(QStringLiteral("trayToggleDictation"));
    QAction *settings = m_menu.addAction(QStringLiteral("Settings..."),
                                         controller, &ApplicationController::showSettingsWindow);
    settings->setObjectName(QStringLiteral("traySettings"));
    m_menu.addSeparator();
    QAction *quit = m_menu.addAction(QStringLiteral("Quit"),
                                     controller, &ApplicationController::quitApplication);
    quit->setObjectName(QStringLiteral("trayQuit"));
    m_tray->setContextMenu(&m_menu);

    connect(controller, &ApplicationController::stateChanged,
            this, &LinuxTrayIcon::applyState);
    // The popup's error banner dismisses itself after a few seconds; the tray
    // notification keeps the failure findable when nobody was watching.
    connect(controller->session(), &DictationSession::popupErrorRequested,
            this, [this](const QString &message) {
                m_tray->showMessage(QStringLiteral("Speecher"), message,
                                    QSystemTrayIcon::Critical);
            });
    connect(m_tray, &QSystemTrayIcon::activated,
            this, [controller](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    controller->showSettingsWindow();
                } else if (reason == QSystemTrayIcon::MiddleClick) {
                    controller->toggle();
                }
            });

    applyState(controller->stateName());
    m_tray->show();
}

void LinuxTrayIcon::applyState(const QString &stateName)
{
    const QString lowered = stateName.toLower();
    const bool listening = lowered == QStringLiteral("starting")
        || lowered == QStringLiteral("listening");
    m_toggleAction->setText(listening ? QStringLiteral("Stop Dictation")
                                      : QStringLiteral("Start Dictation"));
    m_tray->setToolTip(listening ? QStringLiteral("Speecher is listening")
                                 : QStringLiteral("Speecher"));
    m_tray->setIcon(trayIcon(listening));
}

} // namespace speecher
