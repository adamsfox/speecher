#include "ui/WaveformWidget.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
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

// One pill for every state, so it never changes size between listening, the
// delivery receipt and the status shimmer. Wispr Flow's own pill is 50x30 and
// stands alone against a screen edge; Speecher's sits directly above the
// transcript pill, so it takes that pill's size instead.
constexpr int pillWidth = 126;
constexpr int pillHeight = 48;
// Bar geometry is Wispr Flow's 2px scaled by the pill's height ratio (48/30).
constexpr qreal referencePillHeight = 30.0;
constexpr qreal pillScale = pillHeight / referencePillHeight;

// The waveform is a port of Wispr Flow's status-bar bars (v1.6.793): a row of
// rounded dots, 2x2px before pillScale, each scaled vertically about its
// centre by
//
//   audioScale * bulge * wave
//
// where audioScale is the smoothed mic level times a gain of 5 floored at 1,
// bulge weights bars towards the centre, and wave is a 1s keyframe loop
// (1 -> 1.2 -> 1.5 -> 1.1 -> 1.3 -> 1, ease-in-out between keyframes) whose
// phase trails one bar's share of the loop per bar, so a crest travels across
// the row once per second and wraps seamlessly.
// Wispr Flow's row is ten bars in a 50px pill. This pill is wider, so it
// holds proportionally more of the same bars rather than stretching them:
// fifteen at Wispr Flow's thickness and spacing fill 74% of the width, the
// same fraction its ten fill of 50px.
constexpr int barCount = 15;
// The compact strip under the popup's transcript line: just enough for the
// bars at full shout (barDotHeight * audioGain * the 1.5 wave crest = 24px).
constexpr int compactStripHeight = 28;
// The sign-in dots, shared between paintDots and contentWidth.
constexpr int dotRadius = 4;
constexpr int dotGap = 8;
constexpr qreal barWidth = 2.0 * pillScale;
constexpr qreal barGap = 2.0 * pillScale;
constexpr qreal barDotHeight = 2.0 * pillScale;
constexpr qreal barRadius = 0.5 * pillScale;
constexpr float audioGain = 5.0f;
constexpr float levelSpanDb = 20.0f;
// Wispr Flow's bulge falls off with the square of a bar's distance from the
// centre for a short row, and linearly once its bulgeCoefficient reaches 2,
// which is the branch a row this long wants: the quadratic would flatten the
// outermost bars to nothing.
constexpr qreal bulgeCoefficient = 2.0;
// Wispr Flow stops the floor descending past -60 dBFS of the raw capture, so
// one freakishly quiet chunk cannot leave the display permanently
// oversensitive. Speecher's level signal is pre-gained and its gain differs
// per audio input (the microphone emits rms*8 clipped at 1, the file input a
// unity peak), so there is no single dBFS equivalent; -46dB is below room tone
// on both paths, which is what the clamp is there to protect.
constexpr float dbFloorLimit = -46.0f;
constexpr int levelAverageMs = 150;
// The tick rate the 0.85 smoothing factor assumes: Wispr Flow smooths once per
// display frame in a requestAnimationFrame loop.
constexpr int frameIntervalMs = 16;

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

void WaveformWidget::LevelModel::addChunk(float level)
{
    // A silent chunk still counts towards the window mean, so a muted
    // microphone or the end of a session brings the bars back to rest; only
    // the noise floor ignores it, because log10(0) has no floor to learn.
    float mapped = 0.0f;
    if (level > 0.0f) {
        const float db = 20.0f * std::log10(level);
        if (db < m_dbFloor) {
            m_dbFloor = std::max(dbFloorLimit, db);
        }
        mapped = std::clamp((db - m_dbFloor) / levelSpanDb, 0.0f, 1.0f);
    }
    m_windowSum += mapped;
    ++m_windowCount;
}

void WaveformWidget::LevelModel::advance(qint64 nowMs)
{
    if (nowMs - m_windowStartMs >= levelAverageMs) {
        if (m_windowCount > 0) {
            m_target = m_windowSum / float(m_windowCount);
            m_windowSum = 0.0f;
            m_windowCount = 0;
        }
        // Advance on the 150ms grid: assigning nowMs here would stretch every
        // window to the next frame boundary, averaging 160ms of audio.
        m_windowStartMs += levelAverageMs;
        if (nowMs - m_windowStartMs >= levelAverageMs) {
            m_windowStartMs = nowMs;
        }
    }
    // Per-frame exponential smoothing, quantised to 0.01 steps.
    m_smoothed = std::floor((m_smoothed * 0.85f + m_target * 0.15f) * 100.0f) / 100.0f;
}

void WaveformWidget::LevelModel::restart(qint64 nowMs)
{
    m_windowSum = 0.0f;
    m_windowCount = 0;
    m_windowStartMs = nowMs;
    m_target = 0.0f;
    m_smoothed = 0.0f;
}

float WaveformWidget::LevelModel::audioScale() const
{
    return std::max(1.0f, audioGain * m_smoothed);
}

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    applyGeometry();
    m_clock.start();
    m_timer.setInterval(frameIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (m_mode == Mode::Frozen) {
            return;
        }
        const qint64 now = m_clock.elapsed();
        // Clamped so a Frozen spell or a missed tick cannot jump the bars.
        const float dt = std::min(qreal(now - m_lastFrameMs) / 1000.0, 0.1);
        m_lastFrameMs = now;
        m_idlePhase += 14.2f * dt;
        // One cycle per second, accumulated rather than read from the clock in
        // paintWaveform: ticks stop while frozen, so the bars hold their last
        // heights however often the widget repaints, and resume without a jump.
        m_wavePhase = std::fmod(m_wavePhase + dt, 1.0f);
        if (m_mode == Mode::Waveform) {
            m_level.advance(now);
        }
        update();
    });
}

void WaveformWidget::applyGeometry()
{
    // The height follows the desktop's font where that is taller, so a large
    // font cannot clip the receipt; a long message widens the pill. Compact
    // trades the standalone pill's air for a low strip, keeping the font's
    // height only when the strip carries text (the status shimmer).
    const bool showsText = m_mode == Mode::Message || m_mode == Mode::Status;
    const int height = m_compact
        ? (showsText ? fontMetrics().height() + 6 : compactStripHeight)
        : std::max(pillHeight, fontMetrics().height() + 10);
    const int width = m_message.isEmpty()
        ? pillWidth
        : std::max(pillWidth, fontMetrics().horizontalAdvance(m_message) + 32);
    setFixedSize(width, height);
}

void WaveformWidget::setCompact(bool compact)
{
    if (m_compact == compact) {
        return;
    }
    m_compact = compact;
    applyGeometry();
    update();
}

void WaveformWidget::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    // Levels keep arriving while the popup shows an error, and the windowing
    // runs on the frame timer; without this the first tick after the next show
    // would average the whole hidden stretch into one window.
    m_level.restart(m_clock.elapsed());
    QWidget::hideEvent(event);
}

void WaveformWidget::showEvent(QShowEvent *event)
{
    m_lastFrameMs = m_clock.elapsed();
    m_level.restart(m_lastFrameMs);
    m_timer.start();
    QWidget::showEvent(event);
}

void WaveformWidget::setLevel(float level)
{
    if (m_mode != Mode::Waveform) {
        return;
    }
    m_level.addChunk(level);
}

void WaveformWidget::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    if (mode != Mode::Message && mode != Mode::Status) {
        m_message.clear();
    }
    // Frozen holds the bars where they were; every other mode change starts a
    // fresh capture.
    if (mode != Mode::Frozen) {
        m_level.restart(m_clock.elapsed());
    }
    applyGeometry();
    update();
}

void WaveformWidget::setStatusText(const QString &text)
{
    m_message = text.simplified();
    m_mode = m_message.isEmpty() ? Mode::Waveform : Mode::Status;
    m_level.restart(m_clock.elapsed());
    applyGeometry();
    update();
}

void WaveformWidget::setMessage(const QString &message)
{
    m_message = message.simplified();
    m_mode = m_message.isEmpty() ? Mode::Waveform : Mode::Message;
    m_level.restart(m_clock.elapsed());
    applyGeometry();
    update();
}

int WaveformWidget::contentWidth() const
{
    if (m_mode == Mode::Dots) {
        return dotRadius * 6 + dotGap * 2;
    }
    if (m_mode == Mode::Message || m_mode == Mode::Status) {
        return fontMetrics().horizontalAdvance(m_message);
    }
    return int(std::ceil(barCount * barWidth + (barCount - 1) * barGap));
}

void WaveformWidget::setBackgroundVisible(bool visible)
{
    m_backgroundVisible = visible;
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
    if (m_backgroundVisible) {
        painter.drawRoundedRect(pillRect, pillRect.height() / 2.0, pillRect.height() / 2.0);
    }

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
    const qreal audioScale = m_level.audioScale();
    const qreal totalWidth = barCount * barWidth + (barCount - 1) * barGap;
    const qreal startX = (width() - totalWidth) / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(bar);
    for (int i = 0; i < barCount; ++i) {
        const qreal distance = std::abs((barCount - 1) / 2.0 - i);
        const qreal bulge = std::max(0.0, 1.0 - distance * (bulgeCoefficient / 48.0));
        // Each bar trails its neighbour by one bar's share of the loop, so the
        // crest crosses the row exactly once per cycle however many bars there
        // are. At Wispr Flow's ten this is its own 0.1s delay.
        const qreal barPhase = m_wavePhase - qreal(i) / barCount;
        const qreal wave = waveMultiplier(barPhase - std::floor(barPhase));
        const qreal h = barDotHeight * audioScale * bulge * wave;
        const qreal x = startX + i * (barWidth + barGap);
        // scaleY on the reference bar stretches its corners too, which tapers
        // the tips as the bar grows; the radius scales by the same factor.
        const qreal radiusY = barRadius * h / barDotHeight;
        painter.drawRoundedRect(QRectF(x, (height() - h) / 2.0, barWidth, h),
                                barRadius, radiusY);
    }
}

void WaveformWidget::paintDots(QPainter &painter, const QColor &bar)
{
    const int radius = dotRadius;
    const int gap = dotGap;
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
