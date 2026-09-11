#include "platform/KeywatchSetup.h"

#include "output/HelperInstall.h"
#include "setup/KeywatchProtocol.h"

#include <QFileInfo>

#include <unistd.h>

namespace speecher {
namespace {

constexpr auto groupName = "speecher-keywatch";
constexpr auto socketUnitPath = "/etc/systemd/system/speecher-keywatchd.socket";
constexpr auto daemonFileName = "speecher-keywatchd";
#ifndef SPEECHER_KEYWATCH_HELPER_PATH
#define SPEECHER_KEYWATCH_HELPER_PATH "/usr/libexec/speecher/speecher-keywatch-setup"
#endif

} // namespace

KeywatchSetupStatus KeywatchSetup::evaluate(const KeywatchProbeFacts &facts)
{
    if (!facts.socketUnitInstalled) {
        return {KeywatchSetupState::NotInstalled,
                QStringLiteral("Not installed"),
                QStringLiteral("The key helper is not installed.")};
    }
    if (!facts.socketExists) {
        return {KeywatchSetupState::DaemonNotRunning,
                QStringLiteral("Installed, not running"),
                QStringLiteral("The key helper's socket is not active.")};
    }
    if (!facts.socketWritable) {
        if (facts.userInGroup && !facts.currentSessionInGroup) {
            return {KeywatchSetupState::NeedsSignOut,
                    QStringLiteral("Needs sign out"),
                    QStringLiteral("Sign out and back in so this session can reach the key helper.")};
        }
        return {KeywatchSetupState::NotInstalled,
                QStringLiteral("Not set up for this user"),
                QStringLiteral("Your user is not in the %1 group. Install the key helper again.")
                    .arg(QLatin1StringView(groupName))};
    }
    return {KeywatchSetupState::Ready,
            QStringLiteral("Ready"),
            QStringLiteral("The key helper is ready.")};
}

KeywatchSetupStatus KeywatchSetup::probe()
{
    KeywatchProbeFacts facts;
    facts.socketUnitInstalled = QFileInfo::exists(QLatin1StringView(socketUnitPath));
    const QFileInfo socket(QLatin1StringView(keywatch::socketPath));
    facts.socketExists = socket.exists();
    // QFileInfo::isWritable() answers from the owner's point of view for a
    // socket node; access() asks the kernel about this process.
    facts.socketWritable = facts.socketExists && access(keywatch::socketPath, W_OK) == 0;
    const QString user = helpers::currentUserName();
    facts.userInGroup = !user.isEmpty() && helpers::userInGroup(groupName, user);
    facts.currentSessionInGroup = helpers::currentSessionInGroup(groupName);
    return evaluate(facts);
}

bool KeywatchSetup::install(QString *error)
{
    return helpers::runSetupHelper(SPEECHER_KEYWATCH_HELPER_PATH,
                                   {QLatin1StringView(daemonFileName)},
                                   helpers::HelperAction::Install,
                                   error);
}

bool KeywatchSetup::remove(QString *error)
{
    return helpers::runSetupHelper(SPEECHER_KEYWATCH_HELPER_PATH,
                                   {QLatin1StringView(daemonFileName)},
                                   helpers::HelperAction::Remove,
                                   error);
}

} // namespace speecher
