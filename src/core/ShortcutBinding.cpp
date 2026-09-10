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
// mappings to evdev codes, Windows scancodes and macOS virtual key codes.
constexpr PhysicalKey physicalKeys[] = {
    {"ShiftLeft", "Left Shift"},
    {"ShiftRight", "Right Shift"},
    {"ControlLeft", "Left " SPEECHER_CONTROL_NAME},
    {"ControlRight", "Right " SPEECHER_CONTROL_NAME},
    {"AltLeft", "Left " SPEECHER_ALT_NAME},
    {"AltRight", "Right " SPEECHER_ALT_NAME},
    {"MetaLeft", "Left " SPEECHER_META_NAME},
    {"MetaRight", "Right " SPEECHER_META_NAME},
    {"CapsLock", "Caps Lock"},
    {"Fn", "Fn"},
    {"F1", "F1"}, {"F2", "F2"}, {"F3", "F3"}, {"F4", "F4"}, {"F5", "F5"}, {"F6", "F6"},
    {"F7", "F7"}, {"F8", "F8"}, {"F9", "F9"}, {"F10", "F10"}, {"F11", "F11"}, {"F12", "F12"},
    {"F13", "F13"}, {"F14", "F14"}, {"F15", "F15"}, {"F16", "F16"}, {"F17", "F17"}, {"F18", "F18"},
    {"F19", "F19"}, {"F20", "F20"}, {"F21", "F21"}, {"F22", "F22"}, {"F23", "F23"}, {"F24", "F24"},
    {"KeyA", "A"}, {"KeyB", "B"}, {"KeyC", "C"}, {"KeyD", "D"}, {"KeyE", "E"}, {"KeyF", "F"},
    {"KeyG", "G"}, {"KeyH", "H"}, {"KeyI", "I"}, {"KeyJ", "J"}, {"KeyK", "K"}, {"KeyL", "L"},
    {"KeyM", "M"}, {"KeyN", "N"}, {"KeyO", "O"}, {"KeyP", "P"}, {"KeyQ", "Q"}, {"KeyR", "R"},
    {"KeyS", "S"}, {"KeyT", "T"}, {"KeyU", "U"}, {"KeyV", "V"}, {"KeyW", "W"}, {"KeyX", "X"},
    {"KeyY", "Y"}, {"KeyZ", "Z"},
    {"Digit0", "0"}, {"Digit1", "1"}, {"Digit2", "2"}, {"Digit3", "3"}, {"Digit4", "4"},
    {"Digit5", "5"}, {"Digit6", "6"}, {"Digit7", "7"}, {"Digit8", "8"}, {"Digit9", "9"},
    {"Space", "Space"},
    {"Enter", "Enter"},
    {"Tab", "Tab"},
    {"Escape", "Escape"},
    {"Backspace", "Backspace"},
    {"Minus", "-"}, {"Equal", "="}, {"BracketLeft", "["}, {"BracketRight", "]"},
    {"Backslash", "\\"}, {"Semicolon", ";"}, {"Quote", "'"}, {"Backquote", "`"},
    {"Comma", ","}, {"Period", "."}, {"Slash", "/"},
    {"IntlBackslash", "International \\"}, {"IntlRo", "Ro"}, {"IntlYen", "Yen"},
    {"Insert", "Insert"}, {"Delete", "Delete"}, {"Home", "Home"}, {"End", "End"},
    {"PageUp", "Page Up"}, {"PageDown", "Page Down"},
    {"ArrowUp", "Up"}, {"ArrowDown", "Down"}, {"ArrowLeft", "Left"}, {"ArrowRight", "Right"},
    {"PrintScreen", "Print Screen"}, {"ScrollLock", "Scroll Lock"}, {"Pause", "Pause"},
    {"ContextMenu", "Menu"}, {"NumLock", "Num Lock"},
    {"Numpad0", "Numpad 0"}, {"Numpad1", "Numpad 1"}, {"Numpad2", "Numpad 2"},
    {"Numpad3", "Numpad 3"}, {"Numpad4", "Numpad 4"}, {"Numpad5", "Numpad 5"},
    {"Numpad6", "Numpad 6"}, {"Numpad7", "Numpad 7"}, {"Numpad8", "Numpad 8"},
    {"Numpad9", "Numpad 9"},
    {"NumpadAdd", "Numpad +"}, {"NumpadSubtract", "Numpad -"}, {"NumpadMultiply", "Numpad *"},
    {"NumpadDivide", "Numpad /"}, {"NumpadDecimal", "Numpad ."}, {"NumpadEnter", "Numpad Enter"},
    {"NumpadEqual", "Numpad ="}, {"NumpadComma", "Numpad ,"},
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
