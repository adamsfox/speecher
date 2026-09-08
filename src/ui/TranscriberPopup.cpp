#include "ui/TranscriberPopup.h"

#include "platform/FallbackPopupPositioner.h"
#include "ui/WaveformWidget.h"

#include <QApplication>
#include <QColor>
#include <QEasingCurve>
#include <QFrame>
#include <QEvent>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QPalette>
#include <QPaintEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <QPainter>

#include <algorithm>

#ifdef Q_OS_MACOS
#include "platform/mac/MacWindowChrome.h"
#endif

namespace speecher {
namespace {

// Lifts the error countdown bar clear of the capsule's bottom border, which
// it otherwise sits on as a square-ended strip crossing the hairline.
constexpr int kErrorBarInset = 9;

// Paints the pill instead of a stylesheet border: Qt's QSS rounded borders
// render with uneven thickness at fractional display scales, which reads as
// blur around the edge. This draws a one-device-pixel hairline aligned to the
// device-pixel grid.
class PillFrame final : public QFrame {
public:
    using QFrame::QFrame;

protected:
    void paintEvent(QPaintEvent *) override
    {
#ifdef Q_OS_MACOS
        return;
#else
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPalette p = QApplication::palette();
        QColor stroke = p.color(QPalette::Mid);
        stroke.setAlpha(150);
        const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
        const qreal penWidth = 1.0 / dpr;
        const qreal inset = penWidth / 2.0;
        painter.setPen(QPen(stroke, penWidth));
        painter.setBrush(p.color(QPalette::Base));
        const QRectF pillRect = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
        painter.drawRoundedRect(pillRect, pillRect.height() / 2.0, pillRect.height() / 2.0);
#endif
    }
};

// The popup's action chips: capsule buttons in the pill's own visual language,
// painted like PillFrame because the popup floats on a translucent window
// where a rectangular style-drawn button would not fit. Clickable chips fill
// with the Highlight role so they read as buttons at a glance; a disabled chip
// (a progress state such as "Downloading 42%") falls back to the pill's Base
// capsule and reads as status. Palette roles only.
class ChipButton final : public QPushButton {
public:
    explicit ChipButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setFlat(true);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
    }

    QSize sizeHint() const override
    {
        const QSize label = fontMetrics().size(Qt::TextSingleLine, text());
        return QSize(label.width() + 2 * kHorizontalPadding,
                     label.height() + 2 * kVerticalPadding);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPalette p = palette();

        QColor fill = isEnabled() ? p.color(QPalette::Highlight)
                                  : p.color(QPalette::Base);
        if (isEnabled() && isDown()) {
            fill = fill.darker(115);
        } else if (isEnabled() && underMouse()) {
            fill = fill.lighter(110);
        }
        QColor stroke = p.color(QPalette::Mid);
        stroke.setAlpha(150);

        const qreal dpr = devicePixelRatioF() > 0 ? devicePixelRatioF() : 1.0;
        const qreal penWidth = 1.0 / dpr;
        const qreal inset = penWidth / 2.0;
        painter.setPen(isEnabled() ? Qt::NoPen : QPen(stroke, penWidth));
        painter.setBrush(fill);
        const QRectF capsule = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
        painter.drawRoundedRect(capsule, capsule.height() / 2.0, capsule.height() / 2.0);

        painter.setPen(p.color(isEnabled() ? QPalette::HighlightedText
                                           : QPalette::PlaceholderText));
        painter.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    static constexpr int kHorizontalPadding = 14;
    static constexpr int kVerticalPadding = 6;
};

// The thin countdown under an error. Painted here rather than by the style: a
// 3px progress bar in any widget style still draws a frame, and the previous
// stylesheet that hid it hardcoded the colours it replaced.
class DismissBar final : public QProgressBar {
public:
    using QProgressBar::QProgressBar;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (maximum() <= minimum()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));
        const qreal fraction = qreal(value() - minimum()) / qreal(maximum() - minimum());
        QRectF chunk(rect());
        chunk.setWidth(chunk.width() * fraction);
        // Capsule ends, like every other shape on this popup.
        painter.drawRoundedRect(chunk, chunk.height() / 2.0, chunk.height() / 2.0);
    }
};

} // namespace

TranscriberPopup::TranscriberPopup(PopupPositioner *positioner, QWidget *parent)
    : QWidget(parent)
    , m_previewPill(new PillFrame(this))
    , m_preview(new QLabel(this))
    , m_errorDismissProgress(new DismissBar(m_previewPill))
    , m_waveform(new WaveformWidget(this))
    , m_positioner(positioner ? positioner : new FallbackPopupPositioner(this))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->setSpacing(10);
    m_layout->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);

    if (m_positioner->parent() != this) {
        m_positioner->setParent(this);
    }
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);
    m_positioner->configurePopup(m_surface);
#ifdef Q_OS_MACOS
    mac::applyPopupChrome(this);
#endif
    setObjectName(QStringLiteral("transcriberPopup"));
    m_preview->setObjectName(QStringLiteral("rawTranscript"));
    // The pill paints a Base fill, so its text takes the Text role; the font
    // is whatever the desktop chose for the application.
    m_preview->setForegroundRole(QPalette::Text);
    applyTheme();

    m_previewPill->setObjectName(QStringLiteral("previewPill"));
    m_previewPill->setFrameShape(QFrame::NoFrame);
    m_previewPill->setAutoFillBackground(false);
    m_previewPill->setFixedHeight(48);
    m_previewPill->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // Hidden until the first words arrive: before this popup showed a tiny
    // "---" capsule under the waveform while there was nothing to preview.
    m_previewPill->hide();
    m_pillLayout = new QVBoxLayout(m_previewPill);
    m_pillLayout->setContentsMargins(24, 0, 24, 0);
    m_pillLayout->setSpacing(0);

    m_preview->setWordWrap(false);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *previewRow = new QHBoxLayout;
    previewRow->setContentsMargins(0, 0, 0, 0);
    previewRow->setSpacing(10);
    previewRow->addWidget(m_preview, 1);
    // An error's explicit way out, beside the auto-dismiss countdown, matching
    // the Dismiss buttons on the mac and Windows panels.
    m_errorDismiss = new ChipButton(m_previewPill);
    m_errorDismiss->setObjectName(QStringLiteral("errorDismiss"));
    m_errorDismiss->setText(QStringLiteral("Dismiss"));
    m_errorDismiss->hide();
    connect(m_errorDismiss, &QPushButton::clicked, this, [this] {
        m_errorDismissAnimation->stop();
        hide();
        emit errorDismissed();
    });
    previewRow->addWidget(m_errorDismiss, 0, Qt::AlignVCenter);
    m_pillLayout->addLayout(previewRow, 1);

    m_errorDismissProgress->setObjectName(QStringLiteral("errorDismissProgress"));
    m_errorDismissProgress->setRange(0, 1000);
    m_errorDismissProgress->setValue(m_errorDismissProgress->maximum());
    m_errorDismissProgress->setTextVisible(false);
    m_errorDismissProgress->setFixedHeight(3);
    m_errorDismissProgress->hide();
    m_pillLayout->addWidget(m_errorDismissProgress);

    m_errorDismissAnimation = new QPropertyAnimation(
        m_errorDismissProgress,
        QByteArrayLiteral("value"),
        this);
    m_errorDismissAnimation->setDuration(5000);
    m_errorDismissAnimation->setStartValue(m_errorDismissProgress->maximum());
    m_errorDismissAnimation->setEndValue(m_errorDismissProgress->minimum());
    m_errorDismissAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(m_errorDismissAnimation, &QPropertyAnimation::finished, this, [this] {
        hide();
        emit errorDismissed();
    });

    // Both notices are banners in the pill's own capsule: a plain message with
    // an explicitly labelled button beside it, so the action reads as a button
    // rather than asking the user to guess that colored text is clickable.
    const auto makeBanner = [this](const char *name, QLabel *&text) {
        auto *banner = new PillFrame(this);
        banner->setObjectName(QLatin1String(name));
        banner->setFrameShape(QFrame::NoFrame);
        banner->setAutoFillBackground(false);
        auto *layout = new QHBoxLayout(banner);
        layout->setContentsMargins(16, 5, 5, 5);
        layout->setSpacing(10);
        text = new QLabel(banner);
        text->setForegroundRole(QPalette::Text);
        layout->addWidget(text);
        banner->hide();
        m_layout->addWidget(banner, 0, Qt::AlignHCenter);
        return banner;
    };

    m_whatsNewRow = makeBanner("whatsNewRow", m_whatsNewText);
    m_whatsNewText->setObjectName(QStringLiteral("whatsNewText"));
    m_whatsNewAction = new ChipButton(m_whatsNewRow);
    m_whatsNewAction->setObjectName(QStringLiteral("whatsNewAction"));
    m_whatsNewAction->setText(QStringLiteral("See what's new"));
    m_whatsNewDismiss = new ChipButton(m_whatsNewRow);
    m_whatsNewDismiss->setObjectName(QStringLiteral("whatsNewDismiss"));
    m_whatsNewDismiss->setText(QStringLiteral("✕"));
    m_whatsNewDismiss->setToolTip(QStringLiteral("Dismiss"));
    m_whatsNewDismiss->setAccessibleName(QStringLiteral("Dismiss what's new"));
    m_whatsNewRow->layout()->addWidget(m_whatsNewAction);
    m_whatsNewRow->layout()->addWidget(m_whatsNewDismiss);
    connect(m_whatsNewAction, &QPushButton::clicked, this, &TranscriberPopup::whatsNewRequested);
    connect(m_whatsNewDismiss, &QPushButton::clicked, this, [this] {
        setWhatsNewBanner({}, false);
        emit whatsNewDismissed();
    });
    m_whatsNewAutoHide = new QTimer(this);
    m_whatsNewAutoHide->setObjectName(QStringLiteral("whatsNewAutoHide"));
    m_whatsNewAutoHide->setSingleShot(true);
    m_whatsNewAutoHide->setInterval(6000);
    // Auto-hide only tidies this popup; the offer stays pending and returns
    // with the next popup, unlike the dismiss button.
    connect(m_whatsNewAutoHide, &QTimer::timeout, this, [this] {
        setWhatsNewBanner({}, false);
    });

    m_updateBanner = makeBanner("updateBanner", m_updateBannerText);
    m_updateBannerText->setObjectName(QStringLiteral("updateBannerText"));
    m_updateBannerAction = new ChipButton(m_updateBanner);
    m_updateBannerAction->setObjectName(QStringLiteral("updateBannerAction"));
    m_updateBanner->layout()->addWidget(m_updateBannerAction);
    connect(m_updateBannerAction, &QPushButton::clicked,
            this, &TranscriberPopup::updateRequested);
    // No settings prompts here: the overlay cannot take focus and shows while
    // the user is speaking. Desktop accessibility is offered on the Dictation
    // page and in the setup assistant.
    m_layout->addWidget(m_waveform, 0, Qt::AlignHCenter);
    m_layout->addWidget(m_previewPill, 0, Qt::AlignHCenter);
}

QSize TranscriberPopup::sizeHint() const
{
    // configurePopup asks for the hint from the constructor, before the
    // banners exist.
    const int spacing = m_layout->spacing();
    const auto bannerHeight = [spacing](const QFrame *banner) {
        return !banner || banner->isHidden() ? 0
                                             : banner->sizeHint().height() + spacing;
    };
    return QSize(620, 110 + bannerHeight(m_updateBanner) + bannerHeight(m_whatsNewRow));
}

void TranscriberPopup::setStatus(const QString &status)
{
    // "Stopping" is the one state whose label drives this popup: the mic is
    // closed but the provider is still finalising, so the waveform gives way
    // to a shimmering "Transcribing…" and the stale speech preview goes away.
    if (status == QStringLiteral("Stopping")) {
        m_phase = Phase::Transcribing;
        restoreStandardLayout();
        setRefreshLayout(false);
        hidePreview();
        m_waveform->setStatusText(QStringLiteral("Transcribing…"));
    }
    adjustSize();
    updateWindowMask();
}

void TranscriberPopup::setPreview(const QString &preview)
{
    if (m_phase != Phase::Live) {
        return;
    }
    restoreStandardLayout();
    setRefreshLayout(false);
    m_waveform->setMode(WaveformWidget::Mode::Waveform);
    applyPreviewText(preview);
}

void TranscriberPopup::setRefinementPreview(const QString &preview)
{
    if (m_phase != Phase::Refining) {
        return;
    }
    applyPreviewText(preview);
}

void TranscriberPopup::applyPreviewText(const QString &preview)
{
    QString visible = preview.simplified();
    if (visible.isEmpty()) {
        // Nothing to preview means no pill, not a placeholder capsule.
        hidePreview();
        return;
    }
    const QFontMetrics metrics(m_preview->font());
    constexpr int maxTextWidth = 520;
    if (metrics.horizontalAdvance(visible) > maxTextWidth) {
        // A live transcript overflows from the front: the words just spoken
        // stay visible, and the ellipsis says something came before them,
        // as on the mac and Windows panels.
        const QString ellipsis = QStringLiteral("… ");
        const int room = maxTextWidth - metrics.horizontalAdvance(ellipsis);
        while (metrics.horizontalAdvance(visible) > room) {
            const int firstSpace = visible.indexOf(QLatin1Char(' '));
            if (firstSpace < 0) {
                break;
            }
            visible = visible.mid(firstSpace + 1).trimmed();
        }
        visible = metrics.horizontalAdvance(visible) > room
            ? metrics.elidedText(visible, Qt::ElideLeft, maxTextWidth)
            : ellipsis + visible;
    }
    m_preview->setText(visible);
    m_preview->setVisible(true);
    m_previewPill->setVisible(true);
    m_preview->setMaximumWidth(520);
    m_previewPill->resize(m_previewPill->sizeHint().width(), 48);
    adjustSize();
    updateWindowMask();
}

void TranscriberPopup::hidePreview()
{
    setRefreshLayout(false);
    m_previewPill->hide();
    m_preview->hide();
    adjustSize();
    updateWindowMask();
}

void TranscriberPopup::setLevel(float level)
{
    m_waveform->setLevel(level);
}

void TranscriberPopup::setRefining(bool refining)
{
    setRefreshLayout(false);
    if (refining) {
        m_phase = Phase::Refining;
        restoreStandardLayout();
        hidePreview();
        m_waveform->setStatusText(QStringLiteral("Refining…"));
        return;
    }
    m_phase = Phase::Live;
    m_waveform->setMode(WaveformWidget::Mode::Waveform);
}

void TranscriberPopup::setFrozen(bool frozen)
{
    setRefreshLayout(false);
    if (frozen) {
        // Between "Transcribing…" and "Refining…" the session freezes the
        // popup; keep the shimmer rather than flashing a stilled waveform.
        if (m_phase == Phase::Live) {
            m_waveform->setMode(WaveformWidget::Mode::Frozen);
        }
        return;
    }
    m_phase = Phase::Live;
    m_waveform->setMode(WaveformWidget::Mode::Waveform);
}

void TranscriberPopup::showOAuthRefreshIndicator()
{
    m_phase = Phase::Live;
    restoreStandardLayout();
    setRefreshLayout(true);
    m_preview->setText(QStringLiteral("Renewing sign-in…"));
    m_preview->setVisible(true);
    m_previewPill->setVisible(true);
    m_preview->setMaximumWidth(520);
    m_previewPill->resize(m_previewPill->sizeHint().width(), 48);
    m_waveform->setMode(WaveformWidget::Mode::Dots);
    adjustSize();
    updateWindowMask();
}

void TranscriberPopup::showListeningIndicator()
{
    m_phase = Phase::Live;
    restoreStandardLayout();
    setRefreshLayout(false);
    m_waveform->setMode(WaveformWidget::Mode::Waveform);
    adjustSize();
    updateWindowMask();
}

void TranscriberPopup::showMessage(const QString &message)
{
    m_phase = Phase::Live;
    // The outcome is the whole popup: without this the transcript pill stays
    // under the receipt with the last preview words in it.
    hidePreview();
    m_waveform->setMessage(message);
    updateWindowMask();
}

void TranscriberPopup::showErrorMessage(const QString &message)
{
    m_phase = Phase::Live;
    setRefreshLayout(false);
    m_errorDismissAnimation->stop();
    m_waveform->hide();
    const QString text = message.simplified();
    const QFontMetrics metrics(m_preview->font());
    constexpr int maxTextWidth = 520;
    // The capsule hugs a short error instead of stretching to the full wrap
    // width around one small centred line.
    const int textWidth = qBound(1, metrics.horizontalAdvance(text), maxTextWidth);
    m_preview->setText(text);
    m_preview->setWordWrap(true);
    m_preview->setFixedWidth(textWidth);
    m_preview->setVisible(true);
    m_errorDismiss->setVisible(true);
    m_previewPill->setVisible(true);
    // previewRow is centred in what is left after the bar and its air, so the
    // same amount above it puts the text on the capsule's optical centre.
    m_pillLayout->setContentsMargins(24, kErrorBarInset + 3, 24, kErrorBarInset);
    m_errorDismissProgress->setValue(m_errorDismissProgress->maximum());
    m_errorDismissProgress->show();

    const int textHeight = metrics.boundingRect(
                                      QRect(0, 0, textWidth, 1000),
                                      Qt::AlignCenter | Qt::TextWordWrap,
                                      text)
                               .height();
    // 24 keeps the label's 12px above and below the text; 3 is the countdown
    // bar; the inset is the air between the bar and the border.
    m_previewPill->setFixedHeight(
        qMax(48, textHeight + 24 + 2 * (3 + kErrorBarInset)));
    m_previewPill->resize(m_previewPill->sizeHint());
    adjustSize();
    updateWindowMask();
    m_errorDismissAnimation->start();
}

void TranscriberPopup::showPopup(quint64 generation)
{
    // A previous dictation's error belongs to the attempt that failed: its
    // text, its Dismiss chip, its taller pill and its draining countdown all
    // go before this dictation is shown. Without this the countdown could
    // hide a live dictation's popup and report a dismissal against it.
    restoreStandardLayout();
    hidePreview();
    m_pendingPresentationGeneration = generation;
    m_positioner->positionBottomCenter(m_surface);
    updateWindowMask();
    show();
    raise();
    update();
    if (!m_whatsNewRow->isHidden()) {
        m_whatsNewAutoHide->start();
    }
}

void TranscriberPopup::setWhatsNewBanner(const QString &message, bool visible)
{
    const bool visibilityChanged = m_whatsNewRow->isHidden() == visible;
    m_whatsNewText->setText(message);
    m_whatsNewRow->setVisible(visible);
    if (visible && isVisible()) {
        m_whatsNewAutoHide->start();
    } else if (!visible) {
        m_whatsNewAutoHide->stop();
    }
    if (!visibilityChanged) {
        return;
    }
    adjustSize();
    if (isVisible()) {
        m_positioner->positionBottomCenter(m_surface);
    }
}

void TranscriberPopup::setUpdateBanner(const QString &message,
                                       const QString &action,
                                       bool actionEnabled)
{
    const bool visible = !message.isEmpty();
    const bool visibilityChanged = m_updateBanner->isHidden() == visible;
    const bool textChanged = m_updateBannerText->text() != message
        || m_updateBannerAction->text() != action;
    m_updateBannerText->setText(message);
    m_updateBannerAction->setText(action);
    m_updateBannerAction->setVisible(!action.isEmpty());
    m_updateBannerAction->setEnabled(actionEnabled);
    m_updateBanner->setVisible(visible);
    if (!visibilityChanged && !textChanged) {
        return;
    }
    adjustSize();
    if (isVisible()) {
        m_positioner->positionBottomCenter(m_surface);
    }
}

void TranscriberPopup::changeEvent(QEvent *event)
{
    if (!m_applyingTheme && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)) {
        applyTheme();
    }
    QWidget::changeEvent(event);
}

void TranscriberPopup::hideEvent(QHideEvent *event)
{
    // Whatever hid the popup, a countdown left running would hide the next
    // dictation's popup when it finished.
    m_errorDismissAnimation->stop();
    QWidget::hideEvent(event);
}

void TranscriberPopup::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (m_pendingPresentationGeneration == 0) {
        return;
    }
    const quint64 generation = m_pendingPresentationGeneration;
    m_pendingPresentationGeneration = 0;
    QTimer::singleShot(0, this, [this, generation] {
        emit popupPresented(generation);
    });
}

void TranscriberPopup::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateWindowMask();
}

void TranscriberPopup::applyTheme()
{
    if (m_applyingTheme) {
        return;
    }
    m_applyingTheme = true;
    // Colours come from palette roles set once in the constructor; a palette
    // change only needs the painted pill and bar redrawn.
    if (m_previewPill) {
        m_previewPill->update();
    }
    if (m_errorDismissProgress) {
        m_errorDismissProgress->update();
    }
    m_applyingTheme = false;
}

void TranscriberPopup::restoreStandardLayout()
{
    m_errorDismissAnimation->stop();
    m_errorDismissProgress->hide();
    m_errorDismiss->hide();
    m_pillLayout->setContentsMargins(24, 0, 24, 0);
    m_waveform->show();
    m_preview->setWordWrap(false);
    m_preview->setMinimumWidth(0);
    m_preview->setMaximumWidth(520);
    m_previewPill->setFixedHeight(48);
}

void TranscriberPopup::setRefreshLayout(bool refreshLayout)
{
    if (!m_layout) {
        return;
    }

    const int previewIndex = m_layout->indexOf(m_previewPill);
    const int waveformIndex = m_layout->indexOf(m_waveform);
    const bool isRefreshLayout = previewIndex >= 0 && waveformIndex >= 0 && previewIndex < waveformIndex;
    if (isRefreshLayout == refreshLayout) {
        return;
    }

    m_layout->removeWidget(m_previewPill);
    m_layout->removeWidget(m_waveform);
    if (refreshLayout) {
        m_layout->addWidget(m_previewPill, 0, Qt::AlignHCenter);
        m_layout->addWidget(m_waveform, 0, Qt::AlignHCenter);
    } else {
        m_layout->addWidget(m_waveform, 0, Qt::AlignHCenter);
        m_layout->addWidget(m_previewPill, 0, Qt::AlignHCenter);
    }
}

void TranscriberPopup::updateWindowMask()
{
    clearMask();
}

} // namespace speecher
