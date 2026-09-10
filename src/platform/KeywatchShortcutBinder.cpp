#include "platform/KeywatchShortcutBinder.h"

#include "platform/KeywatchSetup.h"
#include "setup/KeywatchProtocol.h"

#include <QLocalSocket>
#include <QTimer>

#include <cstring>
#include <ctime>

namespace speecher {
namespace {

constexpr int reconnectDelayMs = 1000;

quint64 monotonicUsec()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return quint64(now.tv_sec) * 1000000 + quint64(now.tv_nsec) / 1000;
}

const char *refusalText(keywatch::Refusal refusal)
{
    switch (refusal) {
    case keywatch::Refusal::None: return "accepted";
    case keywatch::Refusal::BadVersion: return "the helper speaks another protocol version";
    case keywatch::Refusal::KeyNotPermitted: return "the helper does not permit that key";
    case keywatch::Refusal::AlreadyWatching: return "this connection already watches a key";
    case keywatch::Refusal::TooManyRequests: return "too many requests; try again in a minute";
    case keywatch::Refusal::NoSession: return "no active session for this user";
    }
    return "unknown reason";
}

} // namespace

KeywatchShortcutBinder::KeywatchShortcutBinder(QObject *parent)
    : SingleKeyShortcutBinder(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected, this, [this] {
        const keywatch::WatchRequest request{keywatch::protocolVersion, m_keyId};
        m_replied = false;
        m_socket->write(reinterpret_cast<const char *>(&request), sizeof(request));
    });
    connect(m_socket, &QLocalSocket::readyRead, this, [this] { readFromDaemon(); });
    connect(m_socket, &QLocalSocket::disconnected, this, [this] { reconnectLater(); });
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this] { reconnectLater(); });
}

bool KeywatchShortcutBinder::supported() const
{
    return KeywatchSetup::probe().ready();
}

QString KeywatchShortcutBinder::unsupportedBindingReason(const ShortcutBinding &binding) const
{
    const QString reason = SingleKeyShortcutBinder::unsupportedBindingReason(binding);
    if (!reason.isEmpty()) {
        return reason;
    }
    if (!keywatch::permittedKeyByCode(binding.keyCode().toStdString())) {
        return QStringLiteral(
            "On Wayland, Speecher's key helper watches only keys that cannot type text: "
            "Shift, Ctrl, Alt, Meta, Caps Lock and F13 to F24. %1 is not one of them.")
            .arg(binding.displayText());
    }
    const KeywatchSetupStatus status = KeywatchSetup::probe();
    return status.ready() ? QString() : status.detail;
}

QString KeywatchShortcutBinder::watch(const PhysicalKey &key)
{
    unwatch();
    m_keyId = keywatch::permittedKeyByCode(key.code)->id;
    connectToDaemon();
    return QString();
}

void KeywatchShortcutBinder::unwatch()
{
    m_keyId = 0;
    m_socket->abort();
}

// The daemon stamps every event with CLOCK_MONOTONIC, so a press that
// happened while delivery was injecting keys can be told from a real one.
void KeywatchShortcutBinder::resuming()
{
    m_ignoreDownBeforeUsec = monotonicUsec();
}

void KeywatchShortcutBinder::connectToDaemon()
{
    m_socket->connectToServer(QString::fromLatin1(keywatch::socketPath));
}

void KeywatchShortcutBinder::reconnectLater()
{
    if (m_keyId == 0) {
        return;
    }
    keyUp();
    QTimer::singleShot(reconnectDelayMs, this, [this] {
        if (m_keyId != 0 && m_socket->state() == QLocalSocket::UnconnectedState) {
            connectToDaemon();
        }
    });
}

void KeywatchShortcutBinder::readFromDaemon()
{
    if (!m_replied) {
        if (m_socket->bytesAvailable() < qint64(sizeof(keywatch::WatchReply))) {
            return;
        }
        keywatch::WatchReply reply{};
        m_socket->read(reinterpret_cast<char *>(&reply), sizeof(reply));
        m_replied = true;
        if (reply.refusal != quint8(keywatch::Refusal::None)) {
            qWarning("The key helper refused the watch: %s",
                     refusalText(keywatch::Refusal(reply.refusal)));
            m_keyId = 0;
            m_socket->abort();
            return;
        }
    }
    while (m_socket->bytesAvailable() >= qint64(sizeof(keywatch::KeyEvent))) {
        keywatch::KeyEvent event{};
        m_socket->read(reinterpret_cast<char *>(&event), sizeof(event));
        if (event.down) {
            if (event.monotonicUsec >= m_ignoreDownBeforeUsec) {
                keyDown();
            }
        } else {
            keyUp();
        }
    }
}

} // namespace speecher
