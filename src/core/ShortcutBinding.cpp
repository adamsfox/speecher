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

// KeyboardEvent.code names, which tell left from right and have published
// mappings to evdev codes, Windows scancodes and macOS virtual key codes. The
// evdev column is input-event-codes.h; Fn (KEY_FN) sits past the X11 keycode
// range, which the X11 backend reports rather than watching a wrong key.
constexpr PhysicalKey physicalKeys[] = {
    {"ShiftLeft", "Left Shift", 42},
    {"ShiftRight", "Right Shift", 54},
    {"ControlLeft", "Left " SPEECHER_CONTROL_NAME, 29},
    {"ControlRight", "Right " SPEECHER_CONTROL_NAME, 97},
    {"AltLeft", "Left " SPEECHER_ALT_NAME, 56},
    {"AltRight", "Right " SPEECHER_ALT_NAME, 100},
    {"MetaLeft", "Left " SPEECHER_META_NAME, 125},
    {"MetaRight", "Right " SPEECHER_META_NAME, 126},
    {"CapsLock", "Caps Lock", 58},
    {"Fn", "Fn", 464},
    {"F1", "F1", 59}, {"F2", "F2", 60}, {"F3", "F3", 61}, {"F4", "F4", 62}, {"F5", "F5", 63}, {"F6", "F6", 64},
    {"F7", "F7", 65}, {"F8", "F8", 66}, {"F9", "F9", 67}, {"F10", "F10", 68}, {"F11", "F11", 87}, {"F12", "F12", 88},
    {"F13", "F13", 183}, {"F14", "F14", 184}, {"F15", "F15", 185}, {"F16", "F16", 186}, {"F17", "F17", 187}, {"F18", "F18", 188},
    {"F19", "F19", 189}, {"F20", "F20", 190}, {"F21", "F21", 191}, {"F22", "F22", 192}, {"F23", "F23", 193}, {"F24", "F24", 194},
    {"KeyA", "A", 30}, {"KeyB", "B", 48}, {"KeyC", "C", 46}, {"KeyD", "D", 32}, {"KeyE", "E", 18}, {"KeyF", "F", 33},
    {"KeyG", "G", 34}, {"KeyH", "H", 35}, {"KeyI", "I", 23}, {"KeyJ", "J", 36}, {"KeyK", "K", 37}, {"KeyL", "L", 38},
    {"KeyM", "M", 50}, {"KeyN", "N", 49}, {"KeyO", "O", 24}, {"KeyP", "P", 25}, {"KeyQ", "Q", 16}, {"KeyR", "R", 19},
    {"KeyS", "S", 31}, {"KeyT", "T", 20}, {"KeyU", "U", 22}, {"KeyV", "V", 47}, {"KeyW", "W", 17}, {"KeyX", "X", 45},
    {"KeyY", "Y", 21}, {"KeyZ", "Z", 44},
    {"Digit0", "0", 11}, {"Digit1", "1", 2}, {"Digit2", "2", 3}, {"Digit3", "3", 4}, {"Digit4", "4", 5},
    {"Digit5", "5", 6}, {"Digit6", "6", 7}, {"Digit7", "7", 8}, {"Digit8", "8", 9}, {"Digit9", "9", 10},
    {"Space", "Space", 57},
    {"Enter", "Enter", 28},
    {"Tab", "Tab", 15},
    {"Escape", "Escape", 1},
    {"Backspace", "Backspace", 14},
    {"Minus", "-", 12}, {"Equal", "=", 13}, {"BracketLeft", "[", 26}, {"BracketRight", "]", 27},
    {"Backslash", "\\", 43}, {"Semicolon", ";", 39}, {"Quote", "'", 40}, {"Backquote", "`", 41},
    {"Comma", ",", 51}, {"Period", ".", 52}, {"Slash", "/", 53},
    {"IntlBackslash", "International \\", 86}, {"IntlRo", "Ro", 89}, {"IntlYen", "Yen", 124},
    {"Insert", "Insert", 110}, {"Delete", "Delete", 111}, {"Home", "Home", 102}, {"End", "End", 107},
    {"PageUp", "Page Up", 104}, {"PageDown", "Page Down", 109},
    {"ArrowUp", "Up", 103}, {"ArrowDown", "Down", 108}, {"ArrowLeft", "Left", 105}, {"ArrowRight", "Right", 106},
    {"PrintScreen", "Print Screen", 99}, {"ScrollLock", "Scroll Lock", 70}, {"Pause", "Pause", 119},
    {"ContextMenu", "Menu", 127}, {"NumLock", "Num Lock", 69},
    {"Numpad0", "Numpad 0", 82}, {"Numpad1", "Numpad 1", 79}, {"Numpad2", "Numpad 2", 80},
    {"Numpad3", "Numpad 3", 81}, {"Numpad4", "Numpad 4", 75}, {"Numpad5", "Numpad 5", 76},
    {"Numpad6", "Numpad 6", 77}, {"Numpad7", "Numpad 7", 71}, {"Numpad8", "Numpad 8", 72},
    {"Numpad9", "Numpad 9", 73},
    {"NumpadAdd", "Numpad +", 78}, {"NumpadSubtract", "Numpad -", 74}, {"NumpadMultiply", "Numpad *", 55},
    {"NumpadDivide", "Numpad /", 98}, {"NumpadDecimal", "Numpad .", 83}, {"NumpadEnter", "Numpad Enter", 96},
    {"NumpadEqual", "Numpad =", 117}, {"NumpadComma", "Numpad ,", 121},
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

} // namespace speecher
