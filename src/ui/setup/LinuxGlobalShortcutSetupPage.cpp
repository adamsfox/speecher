#include "ui/setup/LinuxGlobalShortcutSetupPage.h"

#include "app/ApplicationController.h"
#include "core/AppSettings.h"
#include "core/SettingsStore.h"
#include "platform/KeywatchSetup.h"
#include "platform/LinuxDesktopIntegration.h"

#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace speecher {
namespace {

// X11 and xkb report a physical key as its evdev code plus eight.
constexpr int x11KeycodeOffset = 8;

QLabel *guidanceLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    return label;
}

QString shortcutSetStatus(const QString &display)
{
    return QStringLiteral("Shortcut set to %1. Try it now.").arg(display);
}

// A key that types text keeps typing after it is bound, which the user has to
// be told. Modifiers, Caps Lock and the F13-F24 block carry no text, so they
// bind without a caveat.
QString singleKeyTypingWarning(const ShortcutBinding &binding)
{
    static const QSet<QString> silentPrefixes{
        QStringLiteral("Shift"), QStringLiteral("Control"), QStringLiteral("Alt"),
        QStringLiteral("Meta"), QStringLiteral("CapsLock"), QStringLiteral("Fn")};
    const QString code = binding.keyCode();
    for (const QString &prefix : silentPrefixes) {
        if (code.startsWith(prefix)) {
            return QString();
        }
    }
    if (code.startsWith(QLatin1Char('F')) && code.size() > 1 && code.at(1).isDigit()) {
        return QString();
    }
    return QStringLiteral(
        "Heads up: %1 still does its normal job and now also starts dictation, "
        "so pressing it types as well.")
        .arg(binding.displayText());
}

bool isWaylandSession()
{
    const QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE").toLower();
    return sessionType == QStringLiteral("wayland")
        || (sessionType.isEmpty() && qEnvironmentVariableIsSet("WAYLAND_DISPLAY"));
}

} // namespace

SingleKeyCaptureButton::SingleKeyCaptureButton(QWidget *parent)
    : QPushButton(QStringLiteral("Record a single key"), parent)
{
    setCheckable(true);
    connect(this, &QPushButton::clicked, this, [this](bool checked) { setArmed(checked); });
}

void SingleKeyCaptureButton::setArmed(bool armed)
{
    m_armed = armed;
    setChecked(armed);
    setText(armed ? QStringLiteral("Press a key…") : QStringLiteral("Record a single key"));
    if (armed) {
        setFocus(Qt::OtherFocusReason);
    }
}

void SingleKeyCaptureButton::keyPressEvent(QKeyEvent *event)
{
    if (!m_armed || event->isAutoRepeat()) {
        QPushButton::keyPressEvent(event);
        return;
    }
    if (const PhysicalKey *key = physicalKeyForEvdev(int(event->nativeScanCode()) - x11KeycodeOffset)) {
        setArmed(false);
        emit keyCaptured(ShortcutBinding::singleKey(QString::fromLatin1(key->code)));
        return;
    }
    // A key with no vocabulary row (e.g. a media key) is ignored, staying armed.
    event->accept();
}

void SingleKeyCaptureButton::focusOutEvent(QFocusEvent *event)
{
    if (m_armed) {
        setArmed(false);
    }
    QPushButton::focusOutEvent(event);
}

QString linuxGlobalShortcutManualInstruction()
{
    return QStringLiteral(
        "Speecher can't register a shortcut on this desktop. In your desktop's keyboard "
        "settings, add a shortcut that runs this command:");
}

QString linuxTrayShortcutNote(bool trayAvailable)
{
    if (trayAvailable) {
        return QStringLiteral(
            "The shortcut works while Speecher is running. Its icon in the "
            "system tray shows that it is ready.");
    }
    return QStringLiteral("The shortcut works while Speecher is running.");
}

QString linuxGlobalShortcutCommand()
{
    const QString homePath = QDir::homePath();
    QString appImagePath = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (!appImagePath.isEmpty()) {
        appImagePath = resolvedPath(appImagePath);
    }
    return globalShortcutInstructionCommand(
        homePath,
        appImagePath,
        resolvedPath(QCoreApplication::applicationFilePath()));
}

LinuxGlobalShortcutSetupPage::LinuxGlobalShortcutSetupPage(
    ApplicationController &controller,
    QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_homePath(QDir::homePath())
    , m_appImagePath(QString::fromLocal8Bit(qgetenv("APPIMAGE")))
    , m_binaryPath(resolvedPath(QCoreApplication::applicationFilePath()))
    , m_waylandSession(isWaylandSession())
{
    if (!m_appImagePath.isEmpty()) {
        m_appImagePath = resolvedPath(m_appImagePath);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    // Installing comes first, and the shortcut controls stay hidden until it
    // has happened: a shortcut bound to the pre-install path would break the
    // moment the install moves the image.
    m_integration = new QWidget(this);
    m_integration->setObjectName(QStringLiteral("appMenuIntegration"));
    auto *integrationLayout = new QVBoxLayout(m_integration);
    integrationLayout->setContentsMargins(0, 0, 0, 0);
    QString installFolder = appImageInstallDirectory(m_homePath);
    if (installFolder.startsWith(m_homePath)) {
        installFolder = QStringLiteral("~") + installFolder.mid(m_homePath.size());
    }
    integrationLayout->addWidget(guidanceLabel(
        QStringLiteral("Installing moves the Speecher AppImage to %1, adds it to your app "
                       "menu, and makes the speecher command available for desktop "
                       "shortcuts. Setup continues once Speecher is installed.")
            .arg(installFolder),
        m_integration));
    auto *integrationRow = new QHBoxLayout;
    m_integrationButton = new QPushButton(
        QStringLiteral("Install Speecher"), m_integration);
    m_integrationStatus = new QLabel(m_integration);
    integrationRow->addWidget(m_integrationButton);
    integrationRow->addWidget(m_integrationStatus, 1);
    integrationLayout->addLayout(integrationRow);
    m_integration->setVisible(!m_appImagePath.isEmpty());
    layout->addWidget(m_integration);

    m_keySequenceControls = new QWidget(this);
    m_keySequenceControls->setObjectName(QStringLiteral("keySequenceShortcut"));
    auto *keyLayout = new QVBoxLayout(m_keySequenceControls);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->addWidget(guidanceLabel(
        QStringLiteral("Press the keys you want to use for dictation."),
        m_keySequenceControls));
    auto *keyRow = new QHBoxLayout;
    m_sequence = new QKeySequenceEdit(m_keySequenceControls);
    m_sequence->setObjectName(QStringLiteral("globalShortcutSequence"));
    m_setShortcut = new QPushButton(QStringLiteral("Set shortcut"), m_keySequenceControls);
    keyRow->addWidget(m_sequence, 1);
    keyRow->addWidget(m_setShortcut);
    keyLayout->addLayout(keyRow);
    layout->addWidget(m_keySequenceControls);

    // Single-key recording: its own capture widget, since QKeySequenceEdit
    // cannot report a bare modifier. Any key records and saves; a warning
    // explains the cost rather than a modal blocking it.
    m_singleKeyControls = new QWidget(this);
    m_singleKeyControls->setObjectName(QStringLiteral("singleKeyShortcut"));
    auto *singleKeyLayout = new QVBoxLayout(m_singleKeyControls);
    singleKeyLayout->setContentsMargins(0, 0, 0, 0);
    singleKeyLayout->addWidget(guidanceLabel(
        QStringLiteral("Or press a single key, such as Right Alt or F13, to use on its own."),
        m_singleKeyControls));
    m_captureKey = new SingleKeyCaptureButton(m_singleKeyControls);
    m_captureKey->setObjectName(QStringLiteral("singleKeyCapture"));
    singleKeyLayout->addWidget(m_captureKey, 0, Qt::AlignLeft);
    m_singleKeyWarning = guidanceLabel(QString(), m_singleKeyControls);
    m_singleKeyWarning->setObjectName(QStringLiteral("singleKeyWarning"));
    singleKeyLayout->addWidget(m_singleKeyWarning);
    layout->addWidget(m_singleKeyControls);

    // The privileged key-watch helper: Wayland's only route to a single key,
    // gated like the AppImage install block above it.
    m_keyHelperControls = new QWidget(this);
    m_keyHelperControls->setObjectName(QStringLiteral("keyHelperInstall"));
    auto *keyHelperLayout = new QVBoxLayout(m_keyHelperControls);
    keyHelperLayout->setContentsMargins(0, 0, 0, 0);
    keyHelperLayout->addWidget(guidanceLabel(
        QStringLiteral("On Wayland, a single-key shortcut needs a small helper that watches for "
                       "that one key. Setting it up asks for administrator permission once; "
                       "Speecher itself stays unprivileged. The helper only allows keys that "
                       "cannot type text: modifiers, Caps Lock and F13 to F24."),
        m_keyHelperControls));
    auto *keyHelperRow = new QHBoxLayout;
    m_keyHelperButton = new QPushButton(QStringLiteral("Set up single-key helper"), m_keyHelperControls);
    m_keyHelperStatus = new QLabel(m_keyHelperControls);
    m_keyHelperStatus->setWordWrap(true);
    keyHelperRow->addWidget(m_keyHelperButton);
    keyHelperRow->addWidget(m_keyHelperStatus, 1);
    keyHelperLayout->addLayout(keyHelperRow);
    m_keyHelperProgress = new QProgressBar(m_keyHelperControls);
    m_keyHelperProgress->setRange(0, 0);
    m_keyHelperProgress->setVisible(false);
    keyHelperLayout->addWidget(m_keyHelperProgress);
    layout->addWidget(m_keyHelperControls);

    m_portalControls = new QWidget(this);
    m_portalControls->setObjectName(QStringLiteral("portalShortcut"));
    auto *portalLayout = new QVBoxLayout(m_portalControls);
    portalLayout->setContentsMargins(0, 0, 0, 0);
    portalLayout->addWidget(guidanceLabel(
        QStringLiteral("Your desktop will ask you to pick a key combination."),
        m_portalControls));
    m_chooseShortcut = new QPushButton(QStringLiteral("Choose shortcut"), m_portalControls);
    portalLayout->addWidget(m_chooseShortcut, 0, Qt::AlignLeft);
    layout->addWidget(m_portalControls);

    m_status = guidanceLabel(QString(), this);
    m_status->setObjectName(QStringLiteral("globalShortcutStatus"));
    layout->addWidget(m_status);

    m_manualControls = new QWidget(this);
    m_manualControls->setObjectName(QStringLiteral("manualShortcut"));
    auto *manualLayout = new QVBoxLayout(m_manualControls);
    manualLayout->setContentsMargins(0, 0, 0, 0);
    manualLayout->addWidget(guidanceLabel(linuxGlobalShortcutManualInstruction(),
                                          m_manualControls));
    auto *commandRow = new QHBoxLayout;
    m_command = new QLabel(m_manualControls);
    m_command->setObjectName(QStringLiteral("globalShortcutCommand"));
    m_command->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_command->setTextInteractionFlags(Qt::TextSelectableByMouse
                                       | Qt::TextSelectableByKeyboard);
    auto *copy = new QToolButton(m_manualControls);
    copy->setText(QStringLiteral("Copy"));
    copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    copy->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    commandRow->addWidget(m_command, 1);
    commandRow->addWidget(copy);
    manualLayout->addLayout(commandRow);
    layout->addWidget(m_manualControls);

    // Not shown on manual-command desktops: their command starts Speecher by
    // itself, so "only while running" would be wrong there.
    m_trayNote = guidanceLabel(QString(), this);
    m_trayNote->setObjectName(QStringLiteral("globalShortcutTrayNote"));
    layout->addWidget(m_trayNote);

    // The shortcut and its behaviour are set together. This is a native combo
    // bound to the same shortcuts/activationMode setting the General page's
    // schema row edits; the wizard page cannot host a SchemaSettingsPage row
    // (that renders a whole settings pane), so it shares the setting rather
    // than keeping a second copy of the value.
    m_activationModeRow = new QWidget(this);
    auto *modeRow = m_activationModeRow;
    auto *modeLayout = new QVBoxLayout(modeRow);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->addWidget(guidanceLabel(QStringLiteral("Shortcut behaviour"), modeRow));
    m_activationMode = new QComboBox(modeRow);
    m_activationMode->setObjectName(QStringLiteral("activationMode"));
    const auto addMode = [this](ShortcutActivationMode mode, const QString &label) {
        m_activationMode->addItem(label, shortcutActivationModeName(mode));
    };
    addMode(ShortcutActivationMode::PushToTalk, QStringLiteral("Push to talk — dictate while held"));
    addMode(ShortcutActivationMode::Toggle, QStringLiteral("Toggle — one press starts, the next stops"));
    addMode(ShortcutActivationMode::Hybrid, QStringLiteral("Hybrid — a tap toggles, holding dictates"));
    modeLayout->addWidget(m_activationMode, 0, Qt::AlignLeft);
    layout->addWidget(modeRow);

    layout->addStretch();

    const auto currentMode = shortcutActivationModeName(
        m_controller.settings()->shortcutActivationMode());
    m_activationMode->setCurrentIndex(m_activationMode->findData(currentMode));
    connect(m_activationMode, &QComboBox::currentIndexChanged, this, [this] {
        m_controller.settings()->setShortcutActivationMode(
            shortcutActivationModeFromName(m_activationMode->currentData().toString()));
    });
    connect(m_captureKey, &SingleKeyCaptureButton::keyCaptured, this,
            [this](const ShortcutBinding &binding) { saveSingleKey(binding); });
    connect(m_keyHelperButton, &QPushButton::clicked, this, [this] { installKeyHelper(); });

    connect(m_sequence,
            &QKeySequenceEdit::keySequenceChanged,
            m_setShortcut,
            [this](const QKeySequence &sequence) {
                m_setShortcut->setEnabled(
                    !sequence.isEmpty() && sequence != m_controller.globalShortcut().combination());
            });
    connect(m_setShortcut, &QPushButton::clicked, this, [this] { setShortcut(); });
    connect(m_chooseShortcut, &QPushButton::clicked, this, [this] { chooseShortcut(); });
    connect(copy, &QToolButton::clicked, this, [this, copy] {
        QGuiApplication::clipboard()->setText(m_command->text().remove(QChar(0x200B)));
        copy->setIcon(QIcon::fromTheme(
            QStringLiteral("checkmark"),
            QIcon::fromTheme(QStringLiteral("dialog-ok-apply"))));
        QTimer::singleShot(1500, copy, [copy] {
            copy->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        });
    });
    connect(m_integrationButton,
            &QPushButton::clicked,
            this,
            [this] { installIntegration(); });
    connect(&m_controller,
            &ApplicationController::globalShortcutChanged,
            this,
            [this] { refresh(); });
    connect(&m_controller,
            &ApplicationController::globalShortcutSupportChanged,
            this,
            [this] { refresh(); });
    connect(&m_controller,
            &ApplicationController::globalShortcutRegistrationFinished,
            this,
            [this](bool bound, const QString &detail) {
                showRegistrationResult(bound, detail);
            });

    refresh();
}

// The settings-embedded instance has no other trigger after the wizard's
// install moves the image: coming back to the page must not keep showing a
// manual command for the deleted path.
void LinuxGlobalShortcutSetupPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
}

void LinuxGlobalShortcutSetupPage::hideAppMenuIntegration()
{
    m_integration->hide();
    m_integrationHidden = true;
}

void LinuxGlobalShortcutSetupPage::hideActivationMode()
{
    m_activationModeRow->hide();
}

bool LinuxGlobalShortcutSetupPage::installRequired() const
{
    // A command link from an earlier version can point at an image still in
    // Downloads; that is not installed either — the move is part of the deal.
    return !m_integrationHidden && !m_appImagePath.isEmpty()
        && (!appImageIntegrationInstalled(m_homePath, m_appImagePath)
            || !appImageInInstallFolder(m_homePath, m_appImagePath));
}

bool LinuxGlobalShortcutSetupPage::stepComplete() const
{
    if (installRequired()) {
        return false;
    }
    if (!m_controller.globalShortcutSupportKnown()) {
        return false;
    }
    if (!m_controller.globalShortcutsSupported()) {
        return true;
    }
    return !m_controller.globalShortcutDisplay().isEmpty();
}

void LinuxGlobalShortcutSetupPage::installIntegration()
{
    QString error;
    QString installedPath;
    if (!relocateAppImage(m_homePath, m_appImagePath, &installedPath, &error)) {
        m_integrationStatus->setText(error);
        return;
    }
    if (installedPath != m_appImagePath) {
        // Everything that resolves the image path later (updates, restart,
        // shortcut commands) reads APPIMAGE, so the move has to land there.
        m_appImagePath = installedPath;
        qputenv("APPIMAGE", QFile::encodeName(installedPath));
    }
    if (!installAppImageIntegration(m_homePath,
                                    m_appImagePath,
                                    QCoreApplication::applicationDirPath(),
                                    &error)) {
        m_integrationStatus->setText(error);
        return;
    }
    m_integrationStatus->clear();
    refresh();
}

void LinuxGlobalShortcutSetupPage::setShortcut()
{
    QString error;
    if (!m_controller.setGlobalShortcut(m_sequence->keySequence(), &error)) {
        m_status->setText(error.isEmpty() ? QStringLiteral("Couldn't set the shortcut.")
                                          : error);
        return;
    }
    m_setShortcut->setEnabled(false);
    m_status->setText(shortcutSetStatus(m_controller.globalShortcutDisplay()));
}

void LinuxGlobalShortcutSetupPage::saveSingleKey(const ShortcutBinding &binding)
{
    // A refusal is shown, never saved: a Wayland user asking for a letter is
    // told why rather than getting a binding that never fires.
    const QString reason = m_controller.globalShortcutUnsupportedBindingReason(binding);
    if (!reason.isEmpty()) {
        m_singleKeyWarning->clear();
        m_status->setText(reason);
        return;
    }
    QString error;
    if (!m_controller.setGlobalShortcut(binding, &error)) {
        m_singleKeyWarning->clear();
        m_status->setText(error.isEmpty() ? QStringLiteral("Couldn't set the shortcut.") : error);
        return;
    }
    m_singleKeyWarning->setText(singleKeyTypingWarning(binding));
    m_status->setText(shortcutSetStatus(m_controller.globalShortcutDisplay()));
}

void LinuxGlobalShortcutSetupPage::installKeyHelper()
{
    m_keyHelperButton->setEnabled(false);
    m_keyHelperProgress->setVisible(true);
    m_keyHelperStatus->setText(QStringLiteral("Setting up the key helper…"));
    auto *thread = QThread::create([] {
        QString error;
        if (!KeywatchSetup::install(&error)) {
            qWarning("key helper install failed: %s", qPrintable(error));
        }
    });
    const QPointer<LinuxGlobalShortcutSetupPage> guard(this);
    connect(thread, &QThread::finished, this, [this, guard] {
        if (!guard) {
            return;
        }
        m_keyHelperProgress->setVisible(false);
        refreshKeyHelper();
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void LinuxGlobalShortcutSetupPage::chooseShortcut()
{
    m_chooseShortcut->setEnabled(false);
    m_status->setText(QStringLiteral("Waiting for your desktop…"));
    m_controller.registerGlobalShortcut();
}

void LinuxGlobalShortcutSetupPage::refresh()
{
    refreshControls();
    // Only a real change may leave this widget: refresh() runs from
    // showEvent(), and an unconditional emit loops through the assistant's
    // gate update, whose button changes deliver new show events.
    const bool complete = stepComplete();
    if (m_notifiedStepComplete != complete) {
        m_notifiedStepComplete = complete;
        emit stepCompleteChanged();
    }
}

void LinuxGlobalShortcutSetupPage::refreshControls()
{
    // Another instance (the wizard's, next to this settings-embedded one) may
    // have moved the image and updated APPIMAGE since construction.
    const QString appImage = QString::fromLocal8Bit(qgetenv("APPIMAGE"));
    if (!appImage.isEmpty()) {
        m_appImagePath = resolvedPath(appImage);
    }
    // Let the long path wrap at its separators inside a narrow card; the
    // zero-width spaces are stripped again when the command is copied.
    m_command->setWordWrap(true);
    m_command->setText(QString(globalShortcutInstructionCommand(
        m_homePath, m_appImagePath, m_binaryPath)).replace(QLatin1Char('/'), QStringLiteral("/\u200B")));
    if (!m_appImagePath.isEmpty()) {
        const bool installed = appImageIntegrationInstalled(m_homePath, m_appImagePath)
            && appImageInInstallFolder(m_homePath, m_appImagePath);
        m_integrationButton->setText(
            installed ? QStringLiteral("Installed")
                      : QStringLiteral("Install Speecher"));
        m_integrationButton->setEnabled(!installed);
    }

    const bool known = m_controller.globalShortcutSupportKnown();
    const bool supported = m_controller.globalShortcutsSupported();
    const bool desktopChooser = m_controller.globalShortcutUsesDesktopChooser();
    // Until the install has moved the image, every shortcut control is
    // premature: the manual command would quote a path the install is about
    // to remove.
    const bool ready = !installRequired();
    m_keySequenceControls->setVisible(ready && known && supported && !desktopChooser);
    m_portalControls->setVisible(ready && (!known || (supported && desktopChooser)));
    m_manualControls->setVisible(ready && known && !supported);
    // A single key is watched by Speecher itself, so it does not need the
    // desktop's combination service; it shows whenever the step is ready.
    m_singleKeyControls->setVisible(ready && known);
    m_keyHelperControls->setVisible(ready && known && m_waylandSession);
    if (ready && known && m_waylandSession) {
        refreshKeyHelper();
    }
    m_status->setVisible(ready && (!known || supported));
    m_trayNote->setText(linuxTrayShortcutNote(QSystemTrayIcon::isSystemTrayAvailable()));
    m_trayNote->setVisible(ready && known && supported);
    if (!ready) {
        return;
    }

    if (!known) {
        m_chooseShortcut->setEnabled(false);
        m_status->setText(QStringLiteral("Checking your desktop…"));
        return;
    }
    if (!supported) {
        return;
    }
    if (desktopChooser) {
        m_chooseShortcut->setEnabled(true);
        const QString display = m_controller.globalShortcutDisplay();
        if (display != m_displayedShortcut) {
            m_displayedShortcut = display;
            m_status->setText(display.isEmpty() ? QString() : shortcutSetStatus(display));
        }
        return;
    }

    if (!m_sequence->hasFocus()) {
        const QSignalBlocker blocker(m_sequence);
        m_sequence->setKeySequence(m_controller.globalShortcut().combination());
    }
    m_setShortcut->setEnabled(
        !m_sequence->keySequence().isEmpty()
        && m_sequence->keySequence() != m_controller.globalShortcut().combination());
}

void LinuxGlobalShortcutSetupPage::refreshKeyHelper()
{
    const KeywatchSetupStatus status = KeywatchSetup::probe();
    m_keyHelperStatus->setText(status.detail);
    m_keyHelperButton->setEnabled(!status.ready() && !m_keyHelperProgress->isVisible());
    m_keyHelperButton->setText(status.ready() ? QStringLiteral("Key helper ready")
                                              : QStringLiteral("Set up single-key helper"));
}

void LinuxGlobalShortcutSetupPage::showRegistrationResult(bool bound,
                                                           const QString &detail)
{
    m_chooseShortcut->setEnabled(m_controller.globalShortcutsSupported());
    const QString display = m_controller.globalShortcutDisplay();
    if (bound && !display.isEmpty()) {
        m_displayedShortcut = display;
        m_status->setText(shortcutSetStatus(display));
    } else {
        m_status->setText(detail);
    }
    const bool complete = stepComplete();
    if (m_notifiedStepComplete != complete) {
        m_notifiedStepComplete = complete;
        emit stepCompleteChanged();
    }
}

} // namespace speecher
