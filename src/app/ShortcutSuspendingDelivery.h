#pragma once

#include "dictation/DictationPorts.h"
#include "platform/GlobalShortcutBinder.h"

namespace speecher {

// Text delivery injects keystrokes, and a single-key binder watching the
// injected key would take them for the user's finger. Suspending exactly for
// the duration of the delivery call closes that loop: the single-key binders
// gate events while suspended and drain their sources before the suspension
// lifts, so nothing injected leaks through after resume either.
class ShortcutSuspendingDelivery : public TextDeliveryAdapter {
    Q_OBJECT

public:
    ShortcutSuspendingDelivery(TextDeliveryAdapter *inner,
                               GlobalShortcutBinder *binder,
                               QObject *parent = nullptr)
        : TextDeliveryAdapter(parent)
        , m_inner(inner)
        , m_binder(binder)
    {
    }

    DeliveryResult deliver(const OutputSettings &settings,
                           const DeliveryContent &content,
                           const Target &target) override
    {
        m_binder->suspend();
        const DeliveryResult result = m_inner->deliver(settings, content, target);
        m_binder->resume();
        return result;
    }

private:
    TextDeliveryAdapter *m_inner;
    GlobalShortcutBinder *m_binder;
};

} // namespace speecher
