#include "platform/win/WinMediaController.h"

#include <QDebug>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace speecher {

using MediaSession = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession;
using PlaybackStatus = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus;

// Called on the controller's STA; WinRT awaits resume in that apartment.
// The shared state outlives the QObject while an operation is pending.
struct WinMediaState {
    quint64 generation = 0;
    bool pauseDesired = false;
    bool running = false;
    std::vector<MediaSession> paused;
};

namespace {

winrt::fire_and_forget applyRequestedState(std::shared_ptr<WinMediaState> state)
{
    if (state->running) {
        co_return;
    }
    state->running = true;
    quint64 generation;
    do {
        generation = state->generation;
        if (state->pauseDesired) {
            try {
                const auto manager = co_await winrt::Windows::Media::Control::
                    GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
                for (const MediaSession &session : manager.GetSessions()) {
                    if (!state->pauseDesired) {
                        break;
                    }
                    try {
                        if (session.GetPlaybackInfo().PlaybackStatus() == PlaybackStatus::Playing
                            && co_await session.TryPauseAsync()
                            && std::find(state->paused.begin(), state->paused.end(), session)
                                == state->paused.end()) {
                            state->paused.push_back(session);
                        }
                    } catch (const winrt::hresult_error &error) {
                        qWarning() << "media pause failed:"
                                   << QString::fromWCharArray(error.message().c_str());
                    }
                }
            } catch (const winrt::hresult_error &error) {
                qWarning() << "media sessions unavailable:"
                           << QString::fromWCharArray(error.message().c_str());
            }
        } else {
            // Keep failed and unattempted players owned. A later request may
            // retry them; one closed player must not strand the others.
            const auto sessions = std::exchange(state->paused, {});
            for (const MediaSession &session : sessions) {
                bool resumed = false;
                if (!state->pauseDesired) {
                    try {
                        resumed = co_await session.TryPlayAsync();
                    } catch (const winrt::hresult_error &error) {
                        qWarning() << "media resume failed:"
                                   << QString::fromWCharArray(error.message().c_str());
                    }
                }
                if (!resumed) {
                    state->paused.push_back(session);
                }
            }
        }
        // Reconcile requests that arrived during an await, but do not spin
        // on a failed operation when the requested state has not changed.
    } while (state->generation != generation);
    state->running = false;
}

} // namespace

WinMediaController::WinMediaController(QObject *parent)
    : MediaController(parent)
    , m_native(std::make_shared<WinMediaState>())
{
}

WinMediaController::~WinMediaController()
{
    resumePaused();
}

void WinMediaController::pausePlaying()
{
    m_native->pauseDesired = true;
    ++m_native->generation;
    applyRequestedState(m_native);
}

void WinMediaController::resumePaused()
{
    m_native->pauseDesired = false;
    ++m_native->generation;
    applyRequestedState(m_native);
}

} // namespace speecher
