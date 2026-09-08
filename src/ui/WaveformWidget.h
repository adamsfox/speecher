#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include <QWidget>

class QFontMetrics;
class QHideEvent;
class QShowEvent;

namespace speecher {

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Waveform, Dots, Frozen, Message, Status };

    // The audio half of Wispr Flow's waveform, kept apart from the painting so
    // the mapping can be tested without a widget: every capture chunk is
    // mapped through an adaptive noise floor, the chunks are averaged over
    // 150ms windows, and each window's mean is smoothed once per frame.
    class LevelModel {
    public:
        // One capture chunk. A level of zero is silence, which pulls the bars
        // back down but does not train the noise floor.
        void addChunk(float level);
        // One display frame.
        void advance(qint64 nowMs);
        // Starts a fresh capture. The noise floor survives, as it does in
        // Wispr Flow, so the room stays calibrated across sessions.
        void restart(qint64 nowMs);
        float audioScale() const;

    private:
        float m_dbFloor = 0.0f;
        float m_windowSum = 0.0f;
        int m_windowCount = 0;
        qint64 m_windowStartMs = 0;
        float m_target = 0.0f;
        float m_smoothed = 0.0f;
    };

    explicit WaveformWidget(QWidget *parent = nullptr);

public slots:
    void setLevel(float level);
    void setMode(Mode mode);
    void setMessage(const QString &message);
    void setStatusText(const QString &text);

protected:
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void applyGeometry();
    void paintWaveform(QPainter &painter, const QColor &bar);
    void paintDots(QPainter &painter, const QColor &bar);
    void paintMessage(QPainter &painter, const QColor &bar);
    void paintStatus(QPainter &painter, const QColor &bar);

    QTimer m_timer;
    // Drives the frame delta and the level model's windows. It keeps running
    // across Frozen spells; the wave phase, not the clock, is what freezes.
    QElapsedTimer m_clock;
    QString m_message;
    LevelModel m_level;
    qint64 m_lastFrameMs = 0;
    float m_wavePhase = 0.0f;
    float m_idlePhase = 0.0f;
    Mode m_mode = Mode::Waveform;
};

} // namespace speecher
