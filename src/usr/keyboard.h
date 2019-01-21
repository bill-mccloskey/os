#ifndef keyboard_h
#define keyboard_h

#include "base/types.h"

enum KeyType {
  kTypeNone,
  kTypeError,
  kTypeNumber,
  kTypeLetter,
  kTypeSymbol,
  kTypeKeypad,
  kTypeFunction,
  kTypeNamedKey,
};

enum KeyName {
  kKeyNone,

  kKeyEscape,
  kKeyBackspace,
  kKeyTab,
  kKeyEnter,
  kKeyControl,
  kKeyShift,
  kKeyFakeShift,
  kKeyAlt,
  kKeySpace,
  kKeyCapsLock,
  kKeyNumLock,
  kKeyScrollLock,
  kKeyMeta,

  kKeyPrintScreen,
  kKeyBreak,

  kKeyHome,
  kKeyUp,
  kKeyPageUp,
  kKeyLeft,
  kKeyRight,
  kKeyEnd,
  kKeyDown,
  kKeyPageDown,
  kKeyInsert,
  kKeyDelete,
};

enum KeySide {
  kNoSide, kLeft, kRight,
};

struct KeyInfo {
  KeyType type;
  int data;
  KeySide side;
};

enum Modifiers {
  kModShift = 1 << 0,
  kModControl = 1 << 1,
  kModAlt = 1 << 2,
  kModMeta = 1 << 3,
};

enum EventType {
  kKeyDownEvent,
  kKeyUpEvent,
};

struct KeyEvent {
  EventType type;
  int modifiers;
  KeyInfo key;

  KeyEvent(EventType type, int modifiers, KeyInfo key)
    : type(type), modifiers(modifiers), key(key) {}

  explicit KeyEvent(uint64_t formatted) {
    type = static_cast<EventType>((formatted >> 0) & 0xf);
    modifiers = (formatted >> 4) & 0xf;
    key.type = static_cast<KeyType>((formatted >> 8) & 0xf);
    key.side = static_cast<KeySide>((formatted >> 12) & 0xf);
    modifiers = (formatted >> 16) & 0xff;
    key.data = formatted >> 24;
  }

  uint64_t Format() const {
    return (type << 0)
         | (modifiers << 4)
         | (key.type << 8)
         | (key.side << 12)
         | (modifiers << 16)
         | (uint64_t(key.data) << 24);
  }
};

#endif
