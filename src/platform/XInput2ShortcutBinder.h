#pragma once

#include "platform/SingleKeyShortcutBinder.h"

struct _XDisplay;
class QSocketNotifier;

namespace speecher {

// Watches one key on X11 through XInput2 raw events on the root window. Raw
// events arrive whatever window has focus and involve no grab, so the key
// keeps doing its normal job as well.
class XInput2ShortcutBinder final : public SingleKeyShortcutBinder {
    Q_OBJECT

public:
    explicit XInput2ShortcutBinder(QObject *parent = nullptr);
    ~XInput2ShortcutBinder() override;

    bool supported() const override;
    QString unsupportedBindingReason(const ShortcutBinding &binding) const override;

protected:
    QString watch(const PhysicalKey &key) override;
    void unwatch() override;
    void resuming() override;

private:
    void selectRawKeyEvents(bool select);
    void readEvents();

    _XDisplay *m_display = nullptr;
    int m_xiOpcode = 0;
    int m_keycode = 0;
    QSocketNotifier *m_notifier = nullptr;
};

} // namespace speecher
