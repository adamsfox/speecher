#include "providers/NativeCredentialStorage.h"

#ifdef Q_OS_MACOS
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#elif defined(Q_OS_WIN)
#include <QStringDecoder>
#include <windows.h>
#include <wincred.h>
#endif

namespace speecher {

#ifdef Q_OS_MACOS
static bool finishKeychainTool(QProcess &process, QString *error)
{
    if (!process.waitForStarted(1000) || !process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        *error = QStringLiteral("macOS Keychain tool did not finish; check Keychain access");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        // Neither stdout nor stderr is safe to include in errors.
        *error = QStringLiteral("macOS Keychain operation failed (exit %1); check Keychain access or sign in again")
                     .arg(process.exitCode());
        return false;
    }
    return true;
}
#endif

QByteArray readNativeCredential(const QByteArray &service, const QByteArray &account, QString *error)
{
#ifdef Q_OS_MACOS
    // Apple tools retain their Keychain partition across Speecher builds.
    QProcess process;
    process.start(QStringLiteral("/usr/bin/security"),
                  {QStringLiteral("find-generic-password"), QStringLiteral("-s"), QString::fromUtf8(service),
                   QStringLiteral("-a"), QString::fromUtf8(account), QStringLiteral("-w")});
    process.closeWriteChannel();
    if (!finishKeychainTool(process, error)) return {};
    QByteArray bytes = process.readAllStandardOutput();
    if (bytes.endsWith('\n')) bytes.chop(1);
    // security prints non-printable bytes as hex. A JSON object cannot itself
    // consist solely of hex digits, so decoding is unambiguous here.
    const QByteArray decoded = QByteArray::fromHex(bytes);
    if (!bytes.isEmpty() && decoded.toHex() == bytes.toLower()) bytes = decoded;
    return bytes;
#elif defined(Q_OS_WIN)
    const QString target = QString::fromUtf8(account + '.' + service);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
        *error = QStringLiteral("Could not read Windows Credential Manager login (%1); sign in with codex login")
                     .arg(GetLastError());
        return {};
    }
    // Rust keyring's set_password stores UTF-16LE without a terminating NUL.
    QStringDecoder decoder(QStringDecoder::Utf16LE, QStringConverter::Flag::Stateless);
    const QString value = decoder(QByteArrayView(reinterpret_cast<const char *>(credential->CredentialBlob),
                                                credential->CredentialBlobSize));
    const bool invalid = credential->CredentialBlobSize % 2 != 0 || decoder.hasError();
    CredFree(credential);
    if (invalid) {
        *error = QStringLiteral("Windows Credential Manager login contains invalid UTF-16");
        return {};
    }
    return value.toUtf8();
#else
    Q_UNUSED(service);
    Q_UNUSED(account);
    *error = QStringLiteral("Native credential storage is unavailable on this platform");
    return {};
#endif
}

bool writeNativeCredential(const QByteArray &service, const QByteArray &account,
                           const QByteArray &bytes, QString *error)
{
#ifdef Q_OS_MACOS
    // A native write from Speecher re-stamps the partition with this build's
    // cdhash. Execute the in-place update in an Apple-signed process instead.
    // The constant script takes data only over stdin, without security -i's
    // command-length limit or add-generic-password -U's create-if-missing.
    static const QString script = QStringLiteral(R"JS(
ObjC.import('Foundation');
ObjC.import('Security');
const input = $.NSFileHandle.fileHandleWithStandardInput.readDataToEndOfFile;
const request = JSON.parse(ObjC.unwrap($.NSString.alloc.initWithDataEncoding(input, $.NSUTF8StringEncoding)));
const service = $(request.service);
const account = $(request.account);
const data = $.NSData.alloc.initWithBase64EncodedStringOptions($(request.data), 0);
$.SecKeychainSetUserInteractionAllowed(false);
const item = Ref();
let status = $.SecKeychainFindGenericPassword($.nil,
    Number(service.lengthOfBytesUsingEncoding($.NSUTF8StringEncoding)), service.UTF8String,
    Number(account.lengthOfBytesUsingEncoding($.NSUTF8StringEncoding)), account.UTF8String,
    null, null, item);
if (status !== 0) throw new Error('Keychain lookup failed');
status = $.SecKeychainItemModifyAttributesAndData(item[0], $.nil, Number(data.length), data.bytes);
if (status !== 0) throw new Error('Keychain update failed');
)JS");
    QProcess process;
    process.start(QStringLiteral("/usr/bin/osascript"),
                  {QStringLiteral("-l"), QStringLiteral("JavaScript"), QStringLiteral("-e"), script});
    process.write(QJsonDocument(QJsonObject{
        {QStringLiteral("service"), QString::fromUtf8(service)},
        {QStringLiteral("account"), QString::fromUtf8(account)},
        {QStringLiteral("data"), QString::fromLatin1(bytes.toBase64())}
    }).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();
    return finishKeychainTool(process, error);
#elif defined(Q_OS_WIN)
    const QString target = QString::fromUtf8(account + '.' + service);
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
        *error = QStringLiteral("Could not find Windows Credential Manager login for refresh (%1)").arg(GetLastError());
        return false;
    }
    const QString value = QString::fromUtf8(bytes);
    // Windows is little-endian; retain the existing credential's metadata.
    credential->CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<ushort *>(value.utf16()));
    credential->CredentialBlobSize = value.size() * sizeof(ushort);
    const bool saved = CredWriteW(credential, 0);
    const DWORD status = saved ? ERROR_SUCCESS : GetLastError();
    CredFree(credential);
    if (!saved) *error = QStringLiteral("Could not save refreshed Windows Credential Manager login (%1)").arg(status);
    return saved;
#else
    Q_UNUSED(service);
    Q_UNUSED(account);
    Q_UNUSED(bytes);
    *error = QStringLiteral("Native credential storage is unavailable on this platform");
    return false;
#endif
}

} // namespace speecher
