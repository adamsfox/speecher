#include "core/ShortcutBinding.h"

#include <QLatin1StringView>

namespace speecher {
namespace {

#if defined(Q_OS_MACOS)
#define SPEECHER_CONTROL_NAME "Control"
#define SPEECHER_ALT_NAME "Option"
#define SPEECHER_META_NAME "Command"
#elif defined(Q_OS_WIN)
#define SPEECHER_CONTROL_NAME "Ctrl"
#define SPEECHER_ALT_NAME "Alt"
#define SPEECHER_META_NAME "Win"
#else
#define SPEECHER_CONTROL_NAME "Ctrl"
#define SPEECHER_ALT_NAME "Alt"
#define SPEECHER_META_NAME "Meta"
#endif

// No macOS virtual keycode: the key does not exist on Mac keyboards, so the
// macOS backend refuses the binding rather than watching a wrong key.
constexpr int noMac = -1;

// KeyboardEvent.code names, which tell left from right and have published
// mappings to evdev codes, Windows scancodes and macOS virtual key codes. The
// evdev column is input-event-codes.h; Fn (KEY_FN) sits past the X11 keycode
// range, which the X11 backend reports rather than watching a wrong key. The
// mac column is Carbon's kVK_* values (HIToolbox/Events.h), spelled as numbers
// because this file also builds where no Carbon header exists; both columns
// are positional, so a layout change moves neither.
constexpr PhysicalKey physicalKeys[] = {
    {"ShiftLeft", "Left Shift", 42, 56},
    {"ShiftRight", "Right Shift", 54, 60},
    {"ControlLeft", "Left " SPEECHER_CONTROL_NAME, 29, 59},
    {"ControlRight", "Right " SPEECHER_CONTROL_NAME, 97, 62},
    {"AltLeft", "Left " SPEECHER_ALT_NAME, 56, 58},
    {"AltRight", "Right " SPEECHER_ALT_NAME, 100, 61},
    {"MetaLeft", "Left " SPEECHER_META_NAME, 125, 55},
    {"MetaRight", "Right " SPEECHER_META_NAME, 126, 54},
    {"CapsLock", "Caps Lock", 58, 57},
    {"Fn", "Fn", 464, 63},
    {"F1", "F1", 59, 122}, {"F2", "F2", 60, 120}, {"F3", "F3", 61, 99}, {"F4", "F4", 62, 118}, {"F5", "F5", 63, 96}, {"F6", "F6", 64, 97},
    {"F7", "F7", 65, 98}, {"F8", "F8", 66, 100}, {"F9", "F9", 67, 101}, {"F10", "F10", 68, 109}, {"F11", "F11", 87, 103}, {"F12", "F12", 88, 111},
    {"F13", "F13", 183, 105}, {"F14", "F14", 184, 107}, {"F15", "F15", 185, 113}, {"F16", "F16", 186, 106}, {"F17", "F17", 187, 64}, {"F18", "F18", 188, 79},
    {"F19", "F19", 189, 80}, {"F20", "F20", 190, 90}, {"F21", "F21", 191, noMac}, {"F22", "F22", 192, noMac}, {"F23", "F23", 193, noMac}, {"F24", "F24", 194, noMac},
    {"KeyA", "A", 30, 0}, {"KeyB", "B", 48, 11}, {"KeyC", "C", 46, 8}, {"KeyD", "D", 32, 2}, {"KeyE", "E", 18, 14}, {"KeyF", "F", 33, 3},
    {"KeyG", "G", 34, 5}, {"KeyH", "H", 35, 4}, {"KeyI", "I", 23, 34}, {"KeyJ", "J", 36, 38}, {"KeyK", "K", 37, 40}, {"KeyL", "L", 38, 37},
    {"KeyM", "M", 50, 46}, {"KeyN", "N", 49, 45}, {"KeyO", "O", 24, 31}, {"KeyP", "P", 25, 35}, {"KeyQ", "Q", 16, 12}, {"KeyR", "R", 19, 15},
    {"KeyS", "S", 31, 1}, {"KeyT", "T", 20, 17}, {"KeyU", "U", 22, 32}, {"KeyV", "V", 47, 9}, {"KeyW", "W", 17, 13}, {"KeyX", "X", 45, 7},
    {"KeyY", "Y", 21, 16}, {"KeyZ", "Z", 44, 6},
    {"Digit0", "0", 11, 29}, {"Digit1", "1", 2, 18}, {"Digit2", "2", 3, 19}, {"Digit3", "3", 4, 20}, {"Digit4", "4", 5, 21},
    {"Digit5", "5", 6, 23}, {"Digit6", "6", 7, 22}, {"Digit7", "7", 8, 26}, {"Digit8", "8", 9, 28}, {"Digit9", "9", 10, 25},
    {"Space", "Space", 57, 49},
    {"Enter", "Enter", 28, 36},
    {"Tab", "Tab", 15, 48},
    {"Escape", "Escape", 1, 53},
    {"Backspace", "Backspace", 14, 51},
    {"Minus", "-", 12, 27}, {"Equal", "=", 13, 24}, {"BracketLeft", "[", 26, 33}, {"BracketRight", "]", 27, 30},
    {"Backslash", "\\", 43, 42}, {"Semicolon", ";", 39, 41}, {"Quote", "'", 40, 39}, {"Backquote", "`", 41, 50},
    {"Comma", ",", 51, 43}, {"Period", ".", 52, 47}, {"Slash", "/", 53, 44},
    {"IntlBackslash", "International \\", 86, 10}, {"IntlRo", "Ro", 89, 94}, {"IntlYen", "Yen", 124, 93},
    {"Insert", "Insert", 110, 114}, {"Delete", "Delete", 111, 117}, {"Home", "Home", 102, 115}, {"End", "End", 107, 119},
    {"PageUp", "Page Up", 104, 116}, {"PageDown", "Page Down", 109, 121},
    {"ArrowUp", "Up", 103, 126}, {"ArrowDown", "Down", 108, 125}, {"ArrowLeft", "Left", 105, 123}, {"ArrowRight", "Right", 106, 124},
    {"PrintScreen", "Print Screen", 99, noMac}, {"ScrollLock", "Scroll Lock", 70, noMac}, {"Pause", "Pause", 119, noMac},
    {"ContextMenu", "Menu", 127, 110}, {"NumLock", "Num Lock", 69, 71},
    {"Numpad0", "Numpad 0", 82, 82}, {"Numpad1", "Numpad 1", 79, 83}, {"Numpad2", "Numpad 2", 80, 84},
    {"Numpad3", "Numpad 3", 81, 85}, {"Numpad4", "Numpad 4", 75, 86}, {"Numpad5", "Numpad 5", 76, 87},
    {"Numpad6", "Numpad 6", 77, 88}, {"Numpad7", "Numpad 7", 71, 89}, {"Numpad8", "Numpad 8", 72, 91},
    {"Numpad9", "Numpad 9", 73, 92},
    {"NumpadAdd", "Numpad +", 78, 69}, {"NumpadSubtract", "Numpad -", 74, 78}, {"NumpadMultiply", "Numpad *", 55, 67},
    {"NumpadDivide", "Numpad /", 98, 75}, {"NumpadDecimal", "Numpad .", 83, 65}, {"NumpadEnter", "Numpad Enter", 96, 76},
    {"NumpadEqual", "Numpad =", 117, 81}, {"NumpadComma", "Numpad ,", 121, 95},
};

#undef SPEECHER_CONTROL_NAME
#undef SPEECHER_ALT_NAME
#undef SPEECHER_META_NAME

// Distinguishes a stored single key from the QKeySequence text older installs hold.
constexpr auto singleKeyPrefix = "key:";

} // namespace

const PhysicalKey *physicalKey(const QString &code)
{
    for (const PhysicalKey &key : physicalKeys) {
        if (code == QLatin1StringView(key.code)) {
            return &key;
        }
    }
    return nullptr;
}

const PhysicalKey *physicalKeyForEvdev(int evdev)
{
    for (const PhysicalKey &key : physicalKeys) {
        if (key.evdev == evdev) {
            return &key;
        }
    }
    return nullptr;
}

const PhysicalKey *physicalKeyForMac(int mac)
{
    if (mac == noMac) {
        return nullptr;
    }
    for (const PhysicalKey &key : physicalKeys) {
        if (key.mac == mac) {
            return &key;
        }
    }
    return nullptr;
}

ShortcutBinding::ShortcutBinding(const QKeySequence &combination)
    : m_combination(combination)
{
}

ShortcutBinding ShortcutBinding::singleKey(const QString &code)
{
    ShortcutBinding binding;
    if (physicalKey(code)) {
        binding.m_keyCode = code;
    }
    return binding;
}

ShortcutBinding ShortcutBinding::fromString(const QString &text)
{
    const QLatin1StringView prefix(singleKeyPrefix);
    if (text.startsWith(prefix)) {
        return singleKey(text.mid(prefix.size()));
    }
    return ShortcutBinding(QKeySequence(text));
}

bool ShortcutBinding::isEmpty() const
{
    return m_combination.isEmpty() && m_keyCode.isEmpty();
}

bool ShortcutBinding::isSingleKey() const
{
    return !m_keyCode.isEmpty();
}

QKeySequence ShortcutBinding::combination() const
{
    return m_combination;
}

QString ShortcutBinding::keyCode() const
{
    return m_keyCode;
}

QString ShortcutBinding::displayText() const
{
    if (const PhysicalKey *key = physicalKey(m_keyCode)) {
        return QString::fromLatin1(key->label);
    }
    return m_combination.toString(QKeySequence::NativeText);
}

QString ShortcutBinding::toString() const
{
    if (isSingleKey()) {
        return QLatin1StringView(singleKeyPrefix) + m_keyCode;
    }
    return m_combination.toString();
}

// Modifiers, Caps Lock, Fn and the F1-F24 block carry no text, so they bind
// without a caveat.
QString singleKeyTypingWarning(const ShortcutBinding &binding)
{
    static const char *const silentPrefixes[] = {"Shift", "Control", "Alt", "Meta",
                                                 "CapsLock", "Fn"};
    const QString code = binding.keyCode();
    for (const char *prefix : silentPrefixes) {
        if (code.startsWith(QLatin1StringView(prefix))) {
            return QString();
        }
    }
    if (code.startsWith(QLatin1Char('F')) && code.size() > 1 && code.at(1).isDigit()) {
        return QString();
    }
    return QStringLiteral(
        "Heads up: %1 still does its normal job and now also starts dictation, "
        "so pressing it types as well.")
        .arg(binding.displayText());
}

} // namespace speecher
