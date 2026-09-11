#pragma once

#include <QString>
#include <QStringList>

namespace speecher::helpers {

enum class HelperAction {
    Install,
    Remove,
};

QString currentUserName();
// The passwd database's answer, which applies from the next sign-in.
bool userInGroup(const char *group, const QString &userName);
// This process's supplementary groups, which is what the kernel checks now.
bool currentSessionInGroup(const char *group);
bool runProgram(const QString &program,
                const QStringList &arguments,
                QString *error,
                int timeoutMs = 60000);
// Runs the setup helper as root through pkexec with a fixed argv: the helper,
// --install or --remove, --user and the current user. Root never executes
// from a user-writable path: a helper not already at its root-owned installed
// path (an AppImage mount, a build directory) is first copied, with its
// companion files, to a root-owned directory by pkexec'd /usr/bin/install,
// and that copy is what runs.
bool runSetupHelper(const char *installedHelperPath,
                    const QStringList &companionFileNames,
                    HelperAction action,
                    QString *error);

} // namespace speecher::helpers
