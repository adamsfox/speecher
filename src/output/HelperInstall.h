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
// The helper to hand pkexec: its installed path, or from an AppImage a
// per-user copy that outlives the mount, made and verified here along with
// the companion files that must sit beside it. Empty on failure.
QString stagedHelperPath(const char *installedHelperPath,
                         const QStringList &companionFileNames,
                         QString *error);
// Runs the staged helper as root through pkexec with a fixed argv: the
// helper, --install or --remove, --user and the current user. Every AppImage
// copy is verified again right before the prompt shows its path. A same-uid
// process can still replace a copy after that; the risk is accepted because
// such a process can already inject into Speecher, while an fd or shell path
// would make pkexec's prompt unreadable.
bool runSetupHelper(const char *installedHelperPath,
                    const QStringList &companionFileNames,
                    HelperAction action,
                    QString *error);

} // namespace speecher::helpers
