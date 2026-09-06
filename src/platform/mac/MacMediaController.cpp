#include "platform/mac/MacMediaController.h"

#include <utility>

namespace speecher {

MacMediaController::MacMediaController(std::function<QStringList()> runningPlayers,
                                     ScriptRunner runScript, QObject *parent)
    : MediaController(parent)
    , m_runningPlayers(std::move(runningPlayers))
    , m_runScript(std::move(runScript))
{
}

void MacMediaController::pausePlaying()
{
    m_pauseRequested = true;
    applyRequestedState();
}

void MacMediaController::resumePaused()
{
    m_pauseRequested = false;
    applyRequestedState();
}

void MacMediaController::applyRequestedState()
{
    if (m_scriptRunning) return;
    const Action action = m_pauseRequested ? Action::Pause : Action::Resume;
    const QStringList players = m_pauseRequested ? m_runningPlayers() : m_pausedPlayers;
    if (players.isEmpty()) return;
    m_scriptRunning = true;
    m_runScript(action, players, [this, action](const QStringList &paused) {
        m_scriptRunning = false;
        if (action == Action::Pause) {
            for (const QString &player : paused) {
                if (!m_pausedPlayers.contains(player)) m_pausedPlayers << player;
            }
        } else {
            m_pausedPlayers.clear();
        }
        // Only reconcile a change of intent. Retrying an unchanged pause would
        // spin forever when all players were already paused or access was denied.
        if (m_pauseRequested != (action == Action::Pause)) applyRequestedState();
    });
}

} // namespace speecher
