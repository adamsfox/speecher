#pragma once

namespace speecher {

// Marks the keystrokes Speecher injects itself (WinPasteDelivery's Ctrl+V).
// SendInput's dwExtraInfo rides through to RAWKEYBOARD.ExtraInformation, so
// the single-key binder can drop these events even though their WM_INPUT is
// queued and only arrives after the delivery-scoped suspension has lifted.
// The e2e harness posts untagged input, which must keep firing the binder.
// "SPKR" in ASCII; ExtraInformation is 32 bits.
constexpr unsigned long injectedInputTag = 0x53504B52;

} // namespace speecher
