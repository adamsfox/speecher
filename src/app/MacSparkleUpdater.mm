#include "app/MacSparkleUpdater.h"

#include "core/SettingsStore.h"
#include "dictation/DictationSession.h"

#include <QDebug>

#import <Sparkle/Sparkle.h>

#include <utility>

namespace speecher {
namespace {

NSString *const stableFeedUrl = @"https://firemonster612.github.io/speecher/appcast-stable.xml";
NSString *const nightlyFeedUrl = @"https://firemonster612.github.io/speecher/appcast-nightly.xml";

bool restartSafe(DictationState state)
{
    return state == DictationState::Idle || state == DictationState::Error;
}

} // namespace
} // namespace speecher

@interface SpeecherSparkleDelegate : NSObject <SPUUpdaterDelegate, SUVersionComparison>
- (void)setNightly:(BOOL)nightly allowStableReplacement:(BOOL)allowStableReplacement;
@end

@implementation SpeecherSparkleDelegate {
    BOOL _nightly;
    BOOL _allowStableReplacement;
    NSString *_runningVersion;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _runningVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
    }
    return self;
}

- (void)setNightly:(BOOL)nightly allowStableReplacement:(BOOL)allowStableReplacement
{
    _nightly = nightly;
    _allowStableReplacement = allowStableReplacement;
}

- (SUAppcastItem *)bestValidUpdateInAppcast:(SUAppcast *)appcast
                                  forUpdater:(SPUUpdater *)_updater
{
    return _allowStableReplacement ? appcast.items.firstObject : nil;
}

- (id<SUVersionComparison>)versionComparatorForUpdater:(SPUUpdater *)_updater
{
    return self;
}

- (NSComparisonResult)compareVersion:(NSString *)versionA toVersion:(NSString *)versionB
{
    if (_allowStableReplacement && ![versionA isEqualToString:versionB]) {
        if ([versionB isEqualToString:_runningVersion]) {
            return NSOrderedDescending;
        }
        if ([versionA isEqualToString:_runningVersion]) {
            return NSOrderedAscending;
        }
    }
    return [[SUStandardVersionComparator defaultComparator]
        compareVersion:versionA
        toVersion:versionB];
}

- (void)updaterDidNotFindUpdate:(SPUUpdater *)_updater
{
    _allowStableReplacement = NO;
}

- (void)updater:(SPUUpdater *)_updater
        userDidMakeChoice:(SPUUserUpdateChoice)_choice
        forUpdate:(SUAppcastItem *)_updateItem
        state:(SPUUserUpdateState *)_state
{
    _allowStableReplacement = NO;
}

- (void)updater:(SPUUpdater *)_updater didAbortWithError:(NSError *)_error
{
    _allowStableReplacement = NO;
}

- (NSString *)feedURLStringForUpdater:(SPUUpdater *)_updater
{
    // Update end-to-end tests point the check at a local server.
    NSString *override = NSProcessInfo.processInfo.environment[@"SPEECHER_APPCAST_URL"];
    if (override.length > 0) {
        return override;
    }
    return _nightly ? speecher::nightlyFeedUrl : speecher::stableFeedUrl;
}

@end

// Sparkle's questions and progress reports, translated into plain calls on the
// updater's driver seam so our banners own the whole UX. Sparkle keeps this
// object alive inside the updater, which can outlive a MacSparkleUpdater torn
// down at shutdown, hence the disown.
@interface SpeecherSparkleUserDriver : NSObject <SPUUserDriver>
- (instancetype)initWithOwner:(speecher::MacSparkleUpdater *)owner;
- (void)disown;
@end

@implementation SpeecherSparkleUserDriver {
    speecher::MacSparkleUpdater *_owner;
}

- (instancetype)initWithOwner:(speecher::MacSparkleUpdater *)owner
{
    self = [super init];
    if (self) {
        _owner = owner;
    }
    return self;
}

- (void)disown
{
    _owner = nullptr;
}

- (void)showUpdatePermissionRequest:(SPUUpdatePermissionRequest *)request
                              reply:(void (^)(SUUpdatePermissionResponse *))reply
{
    reply([[SUUpdatePermissionResponse alloc]
        initWithAutomaticUpdateChecks:(_owner && _owner->driverWantsAutomaticChecks())
                    sendSystemProfile:NO]);
}

- (void)showUserInitiatedUpdateCheckWithCancellation:(void (^)(void))cancellation
{
    if (_owner) {
        _owner->driverCheckStarted();
    }
}

- (void)showUpdateFoundWithAppcastItem:(SUAppcastItem *)appcastItem
                                 state:(SPUUserUpdateState *)state
                                 reply:(void (^)(SPUUserUpdateChoice))reply
{
    if (!_owner) {
        reply(SPUUserUpdateChoiceDismiss);
        return;
    }
    void (^replyCopy)(SPUUserUpdateChoice) = [reply copy];
    const auto answer = [replyCopy](speecher::MacSparkleUpdater::Reply choice) {
        replyCopy(choice == speecher::MacSparkleUpdater::Reply::Install
                      ? SPUUserUpdateChoiceInstall
                      : SPUUserUpdateChoiceDismiss);
    };
    // Installing means the update was already staged in the background; the
    // only thing left to ask for is the relaunch, which is ReadyToRestart.
    if (state.stage == SPUUserUpdateStageInstalling) {
        _owner->driverReadyToRestart(answer);
    } else {
        _owner->driverUpdateFound(QString::fromNSString(appcastItem.displayVersionString),
                                  answer);
    }
}

- (void)showUpdateReleaseNotesWithDownloadData:(SPUDownloadData *)downloadData
{
    // The What's New pane shows release notes; Sparkle's copy goes unused.
}

- (void)showUpdateReleaseNotesFailedToDownloadWithError:(NSError *)error
{
}

- (void)showUpdateNotFoundWithError:(NSError *)error acknowledgement:(void (^)(void))acknowledgement
{
    acknowledgement();
    if (_owner) {
        _owner->driverUpToDate();
    }
}

- (void)showUpdaterError:(NSError *)error acknowledgement:(void (^)(void))acknowledgement
{
    acknowledgement();
    if (_owner) {
        _owner->driverFailed(QString::fromNSString(error.localizedDescription));
    }
}

- (void)showDownloadInitiatedWithCancellation:(void (^)(void))cancellation
{
    if (_owner) {
        _owner->driverDownloadStarted();
    }
}

- (void)showDownloadDidReceiveExpectedContentLength:(uint64_t)expectedContentLength
{
    if (_owner) {
        _owner->driverDownloadExpects(qint64(expectedContentLength));
    }
}

- (void)showDownloadDidReceiveDataOfLength:(uint64_t)length
{
    if (_owner) {
        _owner->driverDownloadReceived(qint64(length));
    }
}

- (void)showDownloadDidStartExtractingUpdate
{
}

- (void)showExtractionReceivedProgress:(double)progress
{
}

- (void)showReadyToInstallAndRelaunch:(void (^)(SPUUserUpdateChoice))reply
{
    if (!_owner) {
        reply(SPUUserUpdateChoiceDismiss);
        return;
    }
    void (^replyCopy)(SPUUserUpdateChoice) = [reply copy];
    _owner->driverReadyToRestart([replyCopy](speecher::MacSparkleUpdater::Reply choice) {
        replyCopy(choice == speecher::MacSparkleUpdater::Reply::Install
                      ? SPUUserUpdateChoiceInstall
                      : SPUUserUpdateChoiceDismiss);
    });
}

- (void)showInstallingUpdateWithApplicationTerminated:(BOOL)applicationTerminated
                          retryTerminatingApplication:(void (^)(void))retryTerminatingApplication
{
    if (_owner) {
        _owner->driverInstalling();
    }
}

- (void)showUpdateInstalledAndRelaunched:(BOOL)relaunched acknowledgement:(void (^)(void))acknowledgement
{
    acknowledgement();
}

- (void)dismissUpdateInstallation
{
    if (_owner) {
        _owner->driverSessionEnded();
    }
}

@end

namespace speecher {

struct MacSparkleUpdater::Native {
    __strong SpeecherSparkleDelegate *delegate = nil;
    __strong SpeecherSparkleUserDriver *userDriver = nil;
    __strong SPUUpdater *updater = nil;
};

MacSparkleUpdater::MacSparkleUpdater(SettingsStore *settings,
                                     DictationSession *session,
                                     QObject *parent)
    : UpdateController(parent)
    , m_settings(settings)
    , m_session(session)
    , m_native(std::make_unique<Native>())
{
    m_native->delegate = [[SpeecherSparkleDelegate alloc] init];
    m_native->userDriver = [[SpeecherSparkleUserDriver alloc] initWithOwner:this];
    m_native->updater = [[SPUUpdater alloc] initWithHostBundle:NSBundle.mainBundle
                                             applicationBundle:NSBundle.mainBundle
                                                    userDriver:m_native->userDriver
                                                      delegate:m_native->delegate];
    m_dismissedVersion = m_settings->updatesDismissedVersion();
    applySettings();
    connect(settings,
            &SettingsStore::updateSettingsChanged,
            this,
            &MacSparkleUpdater::applySettings);
    connect(m_session, &DictationSession::stateChanged, this, [this] {
        if (m_state == State::RestartPending && restartSafe(m_session->state())) {
            finishRestart();
        }
    });
}

MacSparkleUpdater::~MacSparkleUpdater()
{
    [m_native->userDriver disown];
}

void MacSparkleUpdater::start()
{
    NSError *error = nil;
    if (![m_native->updater startUpdater:&error]) {
        qWarning().noquote() << "Sparkle updater failed to start:"
                             << QString::fromNSString(error.localizedDescription);
    }
}

UpdateController::State MacSparkleUpdater::state() const
{
    return m_state;
}

QString MacSparkleUpdater::currentVersion() const
{
    return QStringLiteral(SPEECHER_VERSION);
}

QString MacSparkleUpdater::availableVersion() const
{
    return m_availableVersion;
}

int MacSparkleUpdater::downloadPercent() const
{
    return m_downloadPercent;
}

QString MacSparkleUpdater::errorMessage() const
{
    return m_error;
}

bool MacSparkleUpdater::isAppImage() const { return false; }
bool MacSparkleUpdater::supportsAutomaticDownloads() const { return true; }

bool MacSparkleUpdater::bannerVisible() const
{
    if (m_state == State::Error) {
        return true;
    }
    if (m_state == State::UpdateAvailable) {
        return m_availableVersion != m_dismissedVersion;
    }
    return m_state == State::Downloading || m_state == State::ReadyToRestart
        || m_state == State::RestartPending || m_state == State::Restarting;
}

// Sparkle keeps a silent scheduled check's failure to itself (the user driver
// only hears about errors after something was shown), so there is no failure
// count to report here.
bool MacSparkleUpdater::repeatedAutomaticCheckFailure() const { return false; }
bool MacSparkleUpdater::manualInstallRequired() const { return false; }

bool MacSparkleUpdater::stableReplacementAvailable() const
{
    return m_state == State::UpdateAvailable && !m_nightlyChannel
        && currentVersion().contains(QStringLiteral("-nightly"));
}

void MacSparkleUpdater::checkForUpdates(UpdateChannel channel)
{
    const bool nightly = channel == UpdateChannel::Nightly;
    const bool stableReplacement = !nightly
        && currentVersion().contains(QStringLiteral("-nightly"));
    m_nightlyChannel = nightly;
    [m_native->delegate setNightly:nightly allowStableReplacement:stableReplacement];
    [m_native->updater checkForUpdates];
}

void MacSparkleUpdater::updateNow()
{
    if (m_state == State::ReadyToRestart) {
        restartNow();
        return;
    }
    if (m_state == State::UpdateAvailable && m_updateReply) {
        auto reply = std::exchange(m_updateReply, nullptr);
        reply(Reply::Install);
        return;
    }
    if (m_state == State::Error || m_state == State::CheckFailed) {
        checkForUpdates(m_settings->updateChannel());
    }
}

void MacSparkleUpdater::installAndRestart()
{
    // Only arm the automatic restart when an update is actually in hand; in
    // error states this button retries the check, and a leftover flag would
    // turn a later background download into a surprise restart.
    if (m_state == State::UpdateAvailable || m_state == State::Downloading
        || m_state == State::ReadyToRestart) {
        m_restartWhenReady = true;
    }
    updateNow();
}

void MacSparkleUpdater::restartNow()
{
    if (m_state != State::ReadyToRestart || !m_installReply) {
        return;
    }
    m_pendingRestoreState = restoreState();
    if (!restartSafe(m_session->state())) {
        setState(State::RestartPending);
        return;
    }
    finishRestart();
}

void MacSparkleUpdater::finishRestart()
{
    if (!m_installReply) {
        return;
    }
    if (!m_pendingRestoreState.isEmpty()) {
        m_settings->setUpdatesRestoreState(m_pendingRestoreState);
    }
    setState(State::Restarting);
    auto install = std::exchange(m_installReply, nullptr);
    install(Reply::Install);
}

void MacSparkleUpdater::dismissAvailableVersion()
{
    if (m_state == State::Error) {
        setState(State::Idle);
        return;
    }
    if (m_state != State::UpdateAvailable || m_availableVersion.isEmpty()) {
        return;
    }
    m_dismissedVersion = m_availableVersion;
    m_settings->setUpdatesDismissedVersion(m_dismissedVersion);
    emit changed();
    if (m_updateReply) {
        auto reply = std::exchange(m_updateReply, nullptr);
        reply(Reply::Dismiss);
    }
}

void MacSparkleUpdater::driverCheckStarted()
{
    setState(State::Checking);
}

void MacSparkleUpdater::driverUpdateFound(const QString &version, ReplyHandler reply)
{
    m_availableVersion = version;
    m_updateReply = std::move(reply);
    setState(State::UpdateAvailable);
}

void MacSparkleUpdater::driverUpToDate()
{
    m_availableVersion.clear();
    setState(State::UpToDate);
}

void MacSparkleUpdater::driverDownloadStarted()
{
    m_downloadTotal = 0;
    m_downloadReceived = 0;
    m_downloadPercent = 0;
    setState(State::Downloading);
}

void MacSparkleUpdater::driverDownloadExpects(qint64 totalBytes)
{
    m_downloadTotal = totalBytes;
}

void MacSparkleUpdater::driverDownloadReceived(qint64 bytes)
{
    if (m_downloadTotal <= 0) {
        return;
    }
    m_downloadReceived += bytes;
    const int percent = int(qMin<qint64>(100, m_downloadReceived * 100 / m_downloadTotal));
    if (percent != m_downloadPercent) {
        m_downloadPercent = percent;
        emit changed();
    }
}

void MacSparkleUpdater::driverReadyToRestart(ReplyHandler install)
{
    m_installReply = std::move(install);
    m_downloadPercent = 100;
    setState(State::ReadyToRestart);
    if (m_restartWhenReady) {
        m_restartWhenReady = false;
        restartNow();
    }
}

void MacSparkleUpdater::driverInstalling()
{
    setState(State::Restarting);
}

void MacSparkleUpdater::driverFailed(const QString &message)
{
    m_updateReply = nullptr;
    m_installReply = nullptr;
    setState(m_state == State::Checking ? State::CheckFailed : State::Error, message);
}

void MacSparkleUpdater::driverSessionEnded()
{
    // Sparkle ends every update session through here, including ones whose
    // outcome the banner must keep showing: an error waiting for "Try again",
    // and the restart that explains why the app is about to exit.
    m_updateReply = nullptr;
    m_installReply = nullptr;
    switch (m_state) {
    case State::Idle:
    case State::CheckFailed:
    case State::UpToDate:
    case State::Error:
    case State::Restarting:
        return;
    default:
        break;
    }
    m_availableVersion.clear();
    m_restartWhenReady = false;
    setState(State::Idle);
}

bool MacSparkleUpdater::driverWantsAutomaticChecks() const
{
    return m_settings->autoCheckUpdates();
}

void MacSparkleUpdater::applySettings()
{
    m_nightlyChannel = m_settings->updateChannel() == UpdateChannel::Nightly;
    [m_native->delegate setNightly:m_nightlyChannel allowStableReplacement:NO];
    SPUUpdater *updater = m_native->updater;
    updater.automaticallyChecksForUpdates = m_settings->autoCheckUpdates();
    updater.automaticallyDownloadsUpdates = m_settings->autoInstallUpdates();
    updater.updateCheckInterval = m_settings->updateCheckIntervalMinutes() * 60;
}

void MacSparkleUpdater::setState(State state, const QString &error)
{
    if (state == State::Error || state == State::CheckFailed) {
        m_restartWhenReady = false;
    }
    m_state = state;
    m_error = error;
    emit changed();
}

} // namespace speecher
