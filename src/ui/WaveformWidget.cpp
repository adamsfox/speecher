#include "ui/WaveformWidget.h"

#include <QApplication>
#include <QFont>
#include <QHideEvent>
#include <QPainter>
#include <QPalette>
#include <QShowEvent>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace speecher {
namespace {

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

// The pill's width when it holds the waveform or the dots; a message widens
// it. Both dimensions follow Wispr Flow's active status pill (50x30).
constexpr int pillWidth = 50;
constexpr int pillHeight = 30;

// The waveform model is a faithful port of Wispr Flow's status-bar bars
// (v1.6.793): ten 2x2px rounded dots, each scaled vertically by
//
//   audioScale * bulge * wave
//
// where audioScale is the smoothed mic level times a gain of 5 floored at 1,
// bulge weights bars towards the centre (1 - distance^2 / 48), and wave is a
// 1s keyframe loop (1 -> 1.2 -> 1.5 -> 1.1 -> 1.3 -> 1, ease-in-out between
// keyframes) whose phase trails 0.1s per bar, so a crest travels across the
// row once per second and wraps seamlessly.
constexpr int barCount = 10;
constexpr qreal barWidth = 2.0;
constexpr qreal barGap = 2.0;
constexpr qreal barDotHeight = 2.0;
constexpr qreal barRadius = 0.5;
constexpr qreal audioGain = 5.0;
constexpr int levelAverageMs = 150;

struct WaveKeyframe {
    qreal at;
    qreal value;
};
constexpr WaveKeyframe waveKeyframes[] = {
    {0.0, 1.0}, {0.2, 1.2}, {0.4, 1.5}, {0.8, 1.1}, {0.9, 1.3}, {1.0, 1.0}};

// CSS ease-in-out, cubic-bezier(0.42, 0, 0.58, 1): solve x(t) = s for t by
// Newton's method, then return y(t). With y control points 0 and 1, y(t)
// reduces to t^2 * (3 - 2t).
qreal easeInOut(qreal s)
{
    constexpr qreal p1x = 0.42;
    constexpr qreal p2x = 0.58;
    qreal t = s;
    for (int i = 0; i < 6; ++i) {
        const qreal oneMinusT = 1.0 - t;
        const qreal x = 3.0 * p1x * t * oneMinusT * oneMinusT
            + 3.0 * p2x * t * t * oneMinusT + t * t * t;
        const qreal dx = 3.0 * p1x * (1.0 - 4.0 * t + 3.0 * t * t)
            + 3.0 * p2x * (2.0 * t - 3.0 * t * t) + 3.0 * t * t;
        if (dx <= 0.0) {
            break;
        }
        t = std::clamp(t - (x - s) / dx, 0.0, 1.0);
    }
    return t * t * (3.0 - 2.0 * t);
}

qreal waveMultiplier(qreal phase)
{
    constexpr int segments = int(std::size(waveKeyframes)) - 1;
    for (int i = 0; i < segments; ++i) {
        const WaveKeyframe &from = waveKeyframes[i];
        const WaveKeyframe &to = waveKeyframes[i + 1];
        if (phase > to.at) {
            continue;
        }
        const qreal progress = (phase - from.at) / (to.at - from.at);
        return from.value + (to.value - from.value) * easeInOut(progress);
    }
    return waveKeyframes[segments].value;
}

} // namespace

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(pillWidth, pillHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_clock.start();
    // 16ms matches the display-rate loop the smoothing constant below was
    // tuned for; the per-frame factor is frame-count based, not time based.
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_mode == Mode::Frozen) {
            return;
        }
        const qint64 now = m_clock.elapsed();
        // Clamped so a Frozen spell or a missed tick can't jump the dots.
        const float dt = std::min(qreal(now - m_lastFrameMs) / 1000.0, 0.1);
        m_lastFrameMs = now;
        m_idlePhase += 14.2f * dt;
        if (m_mode == Mode::Waveform) {
            // Levels arrive per capture chunk; every 150ms their mean becomes
            // the smoothing target. An empty window holds the previous target,
            // mirroring Wispr Flow's main process, which skips silent ticks.
            if (now - m_lastAverageMs >= levelAverageMs) {
                if (m_levelCount > 0) {
                    m_targetLevel = m_levelSum / float(m_levelCount);
                    m_levelSum = 0.0f;
                    m_levelCount = 0;
                }
                m_lastAverageMs = now;
            }
            // Per-frame exponential smoothing, quantised to 0.01 steps.
            m_smoothedLevel = std::floor(
                                  (m_smoothedLevel * 0.85f + m_targetLevel * 0.15f) * 100.0f)
                / 100.0f;
        }
        update();
    });
}

void WaveformWidget::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    QWidget::hideEvent(event);
}

void WaveformWidget::showEvent(QShowEvent *event)
{
    m_lastFrameMs = m_clock.elapsed();
    m_timer.start();
    QWidget::showEvent(event);
}

void WaveformWidget::setLevel(float level)
{
    if (m_mode != Mode::Waveform) {
        return;
    }
    // Zero is the session's idle marker, not a captured chunk; feeding it to
    // the floor would slam the floor to the -60dB clamp.
    if (level <= 0.0f) {
        return;
    }
    // Wispr Flow's level mapping: the chunk's amplitude in dB through an
    // adaptive noise floor. The floor tracks the quietest chunk seen (clamped
    // at -60dB) and maps to 0; floor + 20dB maps to 1. The floor persists
    // across sessions so the room's noise stays calibrated.
    const float db = 20.0f * std::log10(level);
    if (db < m_dbFloor) {
        m_dbFloor = std::max(-60.0f, db);
    }
    m_levelSum += std::clamp((db - m_dbFloor) / 20.0f, 0.0f, 1.0f);
    ++m_levelCount;
}

void WaveformWidget::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    if (mode != Mode::Message && mode != Mode::Status) {
        m_message.clear();
        setFixedWidth(pillWidth);
    }
    // Entering Waveform starts a fresh capture; leaving keeps the smoothed
    // level so Frozen shows the last frame. The dB floor survives on purpose.
    if (mode == Mode::Waveform) {
        m_targetLevel = 0.0f;
        m_smoothedLevel = 0.0f;
        m_levelSum = 0.0f;
        m_levelCount = 0;
        m_lastAverageMs = m_clock.elapsed();
    }
    update();
}

void WaveformWidget::setStatusText(const QString &text)
{
    m_message = text.simplified();
    m_mode = m_message.isEmpty() ? Mode::Waveform : Mode::Status;
    m_targetLevel = 0.0f;
    setFixedWidth(m_mode == Mode::Status
        ? std::max(pillWidth, fontMetrics().horizontalAdvance(m_message) + 32)
        : pillWidth);
    update();
}

void WaveformWidget::setMessage(const QString &message)
{
    m_message = message.simplified();
    m_mode = m_message.isEmpty() ? Mode::Waveform : Mode::Message;
    m_targetLevel = 0.0f;
    // At the waveform's width anything longer than "Input sent" clips against
    // the pill; the text margins match paintMessage's 12px insets.
    setFixedWidth(m_mode == Mode::Message
        ? std::max(pillWidth, fontMetrics().horizontalAdvance(m_message) + 32)
        : pillWidth);
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette p = QApplication::palette();
    const QColor pill = p.color(QPalette::Base);
    const QColor bar = p.color(QPalette::Text);
    const QColor stroke = withAlpha(p.color(QPalette::Mid), 150);
    // One device pixel, centered on the device-pixel grid: on fractionally
    // scaled displays a logical 1px+ stroke lands between device pixels and
    // renders as a soft 2px blur ring.
    const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
    const qreal penWidth = 1.0 / dpr;
    const qreal inset = penWidth / 2.0;
    painter.setPen(QPen(stroke, penWidth));
    painter.setBrush(pill);
    const QRectF pillRect = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
    painter.drawRoundedRect(pillRect, pillRect.height() / 2.0, pillRect.height() / 2.0);

    if (m_mode == Mode::Message) {
        paintMessage(painter, bar);
    } else if (m_mode == Mode::Status) {
        paintStatus(painter, bar);
    } else if (m_mode == Mode::Dots) {
        paintDots(painter, bar);
    } else {
        // Frozen keeps the bars at their last heights but drops them to the
        // 40% alpha Wispr Flow uses once the mic is no longer capturing.
        paintWaveform(painter, m_mode == Mode::Frozen ? withAlpha(bar, 102) : bar);
    }
}

void WaveformWidget::paintWaveform(QPainter &painter, const QColor &bar)
{
    const qreal audioScale = std::max(1.0f, float(audioGain) * m_smoothedLevel);
    const qreal totalWidth = barCount * barWidth + (barCount - 1) * barGap;
    const qreal startX = (width() - totalWidth) / 2.0;
    const qreal phase = qreal(m_clock.elapsed() % 1000) / 1000.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(bar);
    for (int i = 0; i < barCount; ++i) {
        const qreal distance = std::abs((barCount - 1) / 2.0 - i);
        const qreal bulge = std::max(0.0, 1.0 - distance * distance / 48.0);
        // The bar's animation trails the clock by 0.1s per index; the wrap at
        // 1s means bar 0 and bar 9 sit a tenth of a cycle apart, seamlessly.
        const qreal barPhase = phase - 0.1 * i;
        const qreal wave = waveMultiplier(barPhase - std::floor(barPhase));
        const qreal h = barDotHeight * audioScale * bulge * wave;
        const qreal x = startX + i * (barWidth + barGap);
        painter.drawRoundedRect(QRectF(x, (height() - h) / 2.0, barWidth, h),
                                barRadius, barRadius);
    }
}

void WaveformWidget::paintDots(QPainter &painter, const QColor &bar)
{
    const int radius = 4;
    const int gap = 8;
    const int totalWidth = radius * 6 + gap * 2;
    const int startX = (width() - totalWidth) / 2 + radius;
    const int centerY = height() / 2;
    const float phase = std::fmod(m_idlePhase * 0.45f, 3.0f);
    for (int i = 0; i < 3; ++i) {
        const float distance = std::abs(phase - i);
        const float wrappedDistance = std::min(distance, 3.0f - distance);
        const float alpha = 0.26f + 0.74f * std::clamp(1.0f - wrappedDistance, 0.0f, 1.0f);
        QColor dot = bar;
        dot.setAlphaF(alpha);
        painter.setBrush(dot);
        painter.drawEllipse(QPointF(startX + i * (radius * 2 + gap), centerY), radius, radius);
    }
}

void WaveformWidget::paintStatus(QPainter &painter, const QColor &bar)
{
    QFont font = this->font();
    font.setWeight(QFont::Normal);
    painter.setFont(font);
    const QRect textRect = rect().adjusted(12, 0, -12, 0);

    QColor dim = bar;
    dim.setAlphaF(0.38f);
    painter.setPen(dim);
    painter.drawText(textRect, Qt::AlignCenter, m_message);

    // A soft highlight band sweeps the text left to right and loops, so the
    // word reads as "in progress" without a spinner. The band travels one
    // widget width plus its own width per loop; m_idlePhase advances ~14/s.
    const qreal band = width() * 0.55;
    const qreal travel = width() + band * 2.0;
    const qreal pos = std::fmod(qreal(m_idlePhase) * 11.0, travel) - band;
    QLinearGradient sweep(pos, 0, pos + band, 0);
    QColor clear = bar;
    clear.setAlphaF(0.0f);
    sweep.setColorAt(0.0, clear);
    sweep.setColorAt(0.5, bar);
    sweep.setColorAt(1.0, clear);
    painter.setPen(QPen(QBrush(sweep), 0));
    painter.drawText(textRect, Qt::AlignCenter, m_message);
}

void WaveformWidget::paintMessage(QPainter &painter, const QColor &bar)
{
    // The widget's own font is the application font, so the message follows
    // the desktop's font and size choices.
    QFont font = this->font();
    font.setWeight(QFont::Normal);
    painter.setFont(font);
    painter.setPen(bar);
    painter.drawText(rect().adjusted(12, 0, -12, 0), Qt::AlignCenter, m_message);
}

} // namespace speecher
