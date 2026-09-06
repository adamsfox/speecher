#pragma once

#include "app/UpdateController.h"

#include <functional>
#include <memory>

namespace speecher {

class DictationSession;
class SettingsStore;

class MacSparkleUpdater final : public UpdateController {
    Q_OBJECT

public:
    MacSparkleUpdater(SettingsStore *settings,
                      DictationSession *session,
                      QObject *parent = nullptr);
    ~MacSparkleUpdater() override;

    void start() override;
    State state() const override;
    QString currentVersion() const override;
    QString availableVersion() const override;
    int downloadPercent() const override;
    QString errorMessage() const override;
    bool isAppImage() const override;
    bool supportsAutomaticDownloads() const override;
    bool bannerVisible() const override;
    bool repeatedAutomaticCheckFailure() const override;
    bool manualInstallRequired() const override;
    bool stableReplacementAvailable() const override;

    // What our banners answer to Sparkle's "update found" and "ready to
    // install and relaunch" questions.
    enum class Reply { Install, Dismiss };
    using ReplyHandler = std::function<void(Reply)>;

    // The seam the Sparkle user driver drives, public so off-device tests can
    // walk the state machine without an appcast. Sparkle calls its user driver
    // on the main thread, which is also Qt's.
    void driverCheckStarted();
    void driverUpdateFound(const QString &version, ReplyHandler reply);
    void driverUpToDate();
    void driverDownloadStarted();
    void driverDownloadExpects(qint64 totalBytes);
    void driverDownloadReceived(qint64 bytes);
    void driverReadyToRestart(ReplyHandler install);
    void driverInstalling();
    void driverFailed(const QString &message);
    void driverSessionEnded();
    bool driverWantsAutomaticChecks() const;

public slots:
    void checkForUpdates(UpdateChannel channel) override;
    void updateNow() override;
    void installAndRestart() override;
    void dismissAvailableVersion() override;

private:
    struct Native;
    void applySettings();
    void setState(State state, const QString &error = {});
    void restartNow();
    void finishRestart();

    SettingsStore *m_settings;
    DictationSession *m_session;
    std::unique_ptr<Native> m_native;
    State m_state = State::Idle;
    QString m_availableVersion;
    QString m_error;
    QString m_dismissedVersion;
    // Captured at the moment of the restart request: a restart deferred to the
    // end of a dictation still restores what the user was doing when they asked.
    QString m_pendingRestoreState;
    ReplyHandler m_updateReply;
    ReplyHandler m_installReply;
    qint64 m_downloadTotal = 0;
    qint64 m_downloadReceived = 0;
    int m_downloadPercent = 0;
    bool m_restartWhenReady = false;
    bool m_nightlyChannel = false;
};

} // namespace speecher
