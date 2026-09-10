#pragma once

#include "platform/SingleKeyShortcutBinder.h"

class QLocalSocket;

namespace speecher {

// Watches one key on Wayland through speecher-keywatchd, the privileged helper
// that reads the keyboards and reports only whether that one key is down. The
// helper watches only keys that cannot spell text, which is reported per
// binding so the setup page can say so.
class KeywatchShortcutBinder final : public SingleKeyShortcutBinder {
    Q_OBJECT

public:
    explicit KeywatchShortcutBinder(QObject *parent = nullptr);

    bool supported() const override;
    QString unsupportedBindingReason(const ShortcutBinding &binding) const override;

protected:
    QString watch(const PhysicalKey &key) override;
    void unwatch() override;
    void resuming() override;

private:
    void connectToDaemon();
    void readFromDaemon();
    void reconnectLater();

    QLocalSocket *m_socket;
    quint8 m_keyId = 0;
    bool m_replied = false;
    quint64 m_ignoreDownBeforeUsec = 0;
};

} // namespace speecher
