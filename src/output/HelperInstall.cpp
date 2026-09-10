#include "output/HelperInstall.h"

#include "output/HelperPath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

namespace speecher::helpers {
namespace {

QString stableHelperDirectory()
{
    const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    const QString root = dataHome.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        : dataHome;
    return QDir(root).filePath(QStringLiteral("speecher/libexec"));
}

bool verifyHelperCopy(const QString &sourcePath, const QString &destinationPath, QString *error)
{
    QFile source(sourcePath);
    QFile destination(destinationPath);
    if (!source.open(QIODevice::ReadOnly)
        || !destination.open(QIODevice::ReadOnly)
        || source.readAll() != destination.readAll()
        || source.error() != QFileDevice::NoError
        || destination.error() != QFileDevice::NoError) {
        if (error) {
            *error = QStringLiteral("The local copy of %1 could not be verified")
                         .arg(QFileInfo(sourcePath).fileName());
        }
        return false;
    }
    return true;
}

bool copyHelper(const QString &sourcePath, const QString &destinationPath, QString *error)
{
    const QString name = QFileInfo(sourcePath).fileName();
    QFile source(sourcePath);
    if (!QFileInfo(source).isFile() || !QFileInfo(source).isExecutable()
        || !source.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("The bundled %1 is missing or not executable").arg(name);
        }
        return false;
    }
    const QByteArray contents = source.readAll();
    if (source.error() != QFileDevice::NoError) {
        if (error) {
            *error = QStringLiteral("Could not read the bundled %1").arg(name);
        }
        return false;
    }

    const QFileInfo destination(destinationPath);
    const QString directory = destination.dir().absolutePath();
    if (!destination.dir().mkpath(QStringLiteral("."))
        || !QFile::setPermissions(directory,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)) {
        if (error) {
            *error = QStringLiteral("Could not create the local helper directory");
        }
        return false;
    }
    QSaveFile copy(destinationPath);
    if (QFileInfo::exists(destinationPath)
        && !QFile::setPermissions(destinationPath,
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner)) {
        if (error) {
            *error = QStringLiteral("Could not replace the local copy of %1").arg(name);
        }
        return false;
    }
    if (!copy.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Could not open the local copy of %1: %2")
                         .arg(name, copy.errorString());
        }
        return false;
    }
    if (copy.write(contents) != contents.size() || !copy.commit()) {
        if (error) {
            *error = QStringLiteral("Could not install the local copy of %1: %2")
                         .arg(name, copy.errorString());
        }
        return false;
    }
    if (!QFile::setPermissions(destinationPath,
                               QFileDevice::ReadOwner | QFileDevice::ExeOwner)) {
        if (error) {
            *error = QStringLiteral("Could not secure the local copy of %1").arg(name);
        }
        return false;
    }

    if (!verifyHelperCopy(sourcePath, destinationPath, error)) {
        QFile::remove(destinationPath);
        return false;
    }
    return true;
}

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

QString stagedHelperPath(const char *installedHelperPath,
                         const QStringList &companionFileNames,
                         QString *error)
{
    const QString helper = resolvedHelperPath(installedHelperPath);
    if (!qEnvironmentVariableIsSet("APPIMAGE")) {
        return helper;
    }
    const QDir bundledDirectory = QFileInfo(helper).dir();
    const QDir stableDirectory(stableHelperDirectory());
    QStringList fileNames = companionFileNames;
    fileNames.prepend(QFileInfo(helper).fileName());
    for (const QString &fileName : std::as_const(fileNames)) {
        if (!copyHelper(bundledDirectory.filePath(fileName),
                        stableDirectory.filePath(fileName),
                        error)) {
            return QString();
        }
    }
    return stableDirectory.filePath(QFileInfo(helper).fileName());
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
    const QString helper = stagedHelperPath(installedHelperPath, companionFileNames, error);
    if (helper.isEmpty()) {
        return false;
    }
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        const QDir bundledDirectory = QFileInfo(resolvedHelperPath(installedHelperPath)).dir();
        const QDir stableDirectory = QFileInfo(helper).dir();
        QStringList fileNames = companionFileNames;
        fileNames.prepend(QFileInfo(helper).fileName());
        for (const QString &fileName : std::as_const(fileNames)) {
            if (!verifyHelperCopy(bundledDirectory.filePath(fileName),
                                  stableDirectory.filePath(fileName),
                                  error)) {
                return false;
            }
        }
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
