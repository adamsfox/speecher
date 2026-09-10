#pragma once

#include <QKeySequence>
#include <QString>

namespace speecher {

// One row of the physical-key vocabulary: the W3C KeyboardEvent.code name,
// what the UI calls that key, and the Linux evdev keycode. X11 keycodes are
// evdev + 8, so X11 needs no column of its own.
struct PhysicalKey {
    const char *code;
    const char *label;
    int evdev;
};

// The row for a KeyboardEvent.code name, or nullptr when no key has that name.
const PhysicalKey *physicalKey(const QString &code);
// The row for a Linux evdev keycode, or nullptr when the vocabulary lacks it.
const PhysicalKey *physicalKeyForEvdev(int evdev);

// The Global Shortcut: a key combination, which every desktop shortcut service
// accepts, or one physical key, which none does and a platform backend has to
// observe itself. QKeySequence cannot name a single key: Qt folds Alt_L and
// Alt_R into Qt::Key_Alt, and a sequence holding only a modifier is not a chord.
class ShortcutBinding {
public:
    ShortcutBinding() = default;
    ShortcutBinding(const QKeySequence &combination);
    // An unknown code name yields an empty binding.
    static ShortcutBinding singleKey(const QString &code);
    // Reads both toString() forms and the QKeySequence text older installs stored.
    static ShortcutBinding fromString(const QString &text);

    bool isEmpty() const;
    bool isSingleKey() const;
    // Empty for a single key.
    QKeySequence combination() const;
    // The KeyboardEvent.code name; empty for a combination.
    QString keyCode() const;
    QString displayText() const;
    QString toString() const;

    bool operator==(const ShortcutBinding &other) const = default;

private:
    QKeySequence m_combination;
    QString m_keyCode;
};

} // namespace speecher
