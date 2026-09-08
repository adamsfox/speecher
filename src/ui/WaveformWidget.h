#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include <QWidget>

class QHideEvent;
class QShowEvent;

namespace speecher {

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Waveform, Dots, Frozen, Message, Status };

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
    void paintWaveform(QPainter &painter, const QColor &bar);
    void paintDots(QPainter &painter, const QColor &bar);
    void paintMessage(QPainter &painter, const QColor &bar);
    void paintStatus(QPainter &painter, const QColor &bar);

    QTimer m_timer;
    // The clock drives the traveling wave's phase and the frame delta; it
    // keeps running across Frozen spells so freezing never rewinds the wave.
    QElapsedTimer m_clock;
    QString m_message;
    qint64 m_lastFrameMs = 0;
    qint64 m_lastAverageMs = 0;
    float m_dbFloor = 0.0f;
    float m_levelSum = 0.0f;
    int m_levelCount = 0;
    float m_targetLevel = 0.0f;
    float m_smoothedLevel = 0.0f;
    float m_idlePhase = 0.0f;
    Mode m_mode = Mode::Waveform;
};

} // namespace speecher
