#include "output/HelperInstall.h"

#include "output/HelperPath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace speecher::helpers {
namespace {

// Where a helper that does not already live at a root-owned path is staged,
// by root, before root executes it.
constexpr auto rootStageDirectory = "/usr/local/lib/speecher/setup";

} // namespace

QString currentUserName()
{
    if (const passwd *pw = getpwuid(getuid())) {
        return QString::fromLocal8Bit(pw->pw_name);
    }
    return qEnvironmentVariable("USER");
}

bool userInGroup(const char *group, const QString &userName)
{
    const QByteArray user = userName.toLocal8Bit();
    const struct group *grp = getgrnam(group);
    if (!grp) {
        return false;
    }
    for (char **member = grp->gr_mem; member && *member; ++member) {
        if (user == *member) {
            return true;
        }
    }
    const passwd *pw = getpwnam(user.constData());
    return pw && pw->pw_gid == grp->gr_gid;
}

bool currentSessionInGroup(const char *group)
{
    const struct group *grp = getgrnam(group);
    if (!grp) {
        return false;
    }
    const int count = getgroups(0, nullptr);
    if (count <= 0) {
        return false;
    }
    QList<gid_t> groups(count);
    if (getgroups(count, groups.data()) < 0) {
        return false;
    }
    return groups.contains(grp->gr_gid) || getegid() == grp->gr_gid;
}

bool runProgram(const QString &program,
                const QStringList &arguments,
                QString *error,
                int timeoutMs)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("Could not start %1").arg(program);
        }
        return false;
    }
    if (!process.waitForFinished(timeoutMs)
        || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        process.kill();
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (error) {
            *error = stderrText.isEmpty() ? QStringLiteral("%1 failed").arg(program) : stderrText;
        }
        return false;
    }
    return true;
}

bool runSetupHelper(const char *installedHelperPath,
                    const QStringList &companionFileNames,
                    HelperAction action,
                    QString *error)
{
    const QString pkexec = QStandardPaths::findExecutable(QStringLiteral("pkexec"));
    if (pkexec.isEmpty()) {
        if (error) {
            *error = QStringLiteral("pkexec is not installed");
        }
        return false;
    }
    const QString user = currentUserName();
    if (user.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Could not determine the current user");
        }
        return false;
    }
    // Root never executes from a user-writable path. The installed libexec
    // path is root-owned and runs directly; a helper found anywhere else (an
    // AppImage mount, a build directory) is first copied to a root-owned
    // directory by pkexec'd /usr/bin/install — fixed tooling, so nothing a
    // user can rewrite runs as root — and the root-owned copy is what runs.
    // The two prompts this costs are honest: one names install, one the helper.
    QString helper = resolvedHelperPath(installedHelperPath);
    if (helper != QLatin1StringView(installedHelperPath)) {
        const QDir sourceDirectory = QFileInfo(helper).dir();
        const QString helperName = QFileInfo(helper).fileName();

        // The helper usually sits in the AppImage's FUSE mount, which only the
        // user who mounted it can read: pkexec's root gets EACCES from the
        // mount itself, whatever the file's own mode, so staging straight from
        // there fails with "cannot stat". Copy the helper and its companions
        // onto a normal filesystem first, as this unprivileged process which
        // can read the mount, so root can then read them to stage.
        QTemporaryDir readable;
        if (!readable.isValid()) {
            if (error) {
                *error = QStringLiteral("Could not create a temporary directory for the key helper.");
            }
            return false;
        }
        QStringList names{helperName};
        names.append(companionFileNames);
        QStringList sources;
        for (const QString &name : names) {
            const QString destination = readable.filePath(name);
            QFile::remove(destination);
            if (!QFile::copy(sourceDirectory.filePath(name), destination)) {
                if (error) {
                    *error = QStringLiteral("Could not read the bundled %1.").arg(name);
                }
                return false;
            }
            sources.append(destination);
        }

        QStringList stageArguments{QStringLiteral("/usr/bin/install"),
                                   QStringLiteral("-o"), QStringLiteral("root"),
                                   QStringLiteral("-g"), QStringLiteral("root"),
                                   QStringLiteral("-m"), QStringLiteral("0755"),
                                   QStringLiteral("-D"),
                                   QStringLiteral("-t"),
                                   QLatin1StringView(rootStageDirectory)};
        stageArguments.append(sources);
        if (!runProgram(pkexec, stageArguments, error, 5 * 60 * 1000)) {
            return false;
        }
        helper = QLatin1StringView(rootStageDirectory) + QLatin1Char('/') + helperName;
    }
    return runProgram(pkexec,
                      {helper,
                       action == HelperAction::Install ? QStringLiteral("--install")
                                                       : QStringLiteral("--remove"),
                       QStringLiteral("--user"),
                       user},
                      error,
                      5 * 60 * 1000);
}

} // namespace speecher::helpers
