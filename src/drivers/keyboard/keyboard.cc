#include "io.h"
#include "keyboard.h"
#include "output_stream.h"
#include "system.h"
#include "types.h"

class DebugOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysWriteByte(c);
  }
};

class ConsoleOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysSend(1, 0, c);
  }
};

static const int kMsgReadReply = 7387;

struct ScanCodeInfo {
  KeyInfo unshifted;
  KeyInfo shifted;
  KeyInfo escaped;
};

#define SINGLE_KEY(type, data) { type, data }, { type, data }
#define SHIFTED_KEY(type, data_unshifted, data_shifted) { type, data_unshifted }, { type, data_shifted }
#define KEYPAD_KEY(unshifted, shifted) { kTypeKeypad, unshifted }, { kTypeNamedKey, shifted }

ScanCodeInfo g_scancodes[] = {
  { SINGLE_KEY(kTypeError, 0) }, /* 0 */
  { SINGLE_KEY(kTypeNamedKey, kKeyEscape) }, /* 1 */
  { SHIFTED_KEY(kTypeNumber, '1', '!') }, /* 2 */
  { SHIFTED_KEY(kTypeNumber, '2', '@') }, /* 3 */
  { SHIFTED_KEY(kTypeNumber, '3', '#') }, /* 4 */
  { SHIFTED_KEY(kTypeNumber, '4', '$') }, /* 5 */
  { SHIFTED_KEY(kTypeNumber, '5', '%') }, /* 6 */
  { SHIFTED_KEY(kTypeNumber, '6', '^') }, /* 7 */
  { SHIFTED_KEY(kTypeNumber, '7', '&') }, /* 8 */
  { SHIFTED_KEY(kTypeNumber, '8', '*') }, /* 9 */
  { SHIFTED_KEY(kTypeNumber, '9', '(') }, /* a */
  { SHIFTED_KEY(kTypeNumber, '0', ')') }, /* b */
  { SHIFTED_KEY(kTypeSymbol, '-', '_') }, /* c */
  { SHIFTED_KEY(kTypeSymbol, '=', '+') }, /* d */
  { SINGLE_KEY(kTypeNamedKey, kKeyBackspace) }, /* e */
  { SINGLE_KEY(kTypeNamedKey, kKeyTab) }, /* f */
  { SHIFTED_KEY(kTypeLetter, 'q', 'Q') }, /* 10 */
  { SHIFTED_KEY(kTypeLetter, 'w', 'W') }, /* 11 */
  { SHIFTED_KEY(kTypeLetter, 'e', 'E') }, /* 12 */
  { SHIFTED_KEY(kTypeLetter, 'r', 'R') }, /* 13 */
  { SHIFTED_KEY(kTypeLetter, 't', 'T') }, /* 14 */
  { SHIFTED_KEY(kTypeLetter, 'y', 'Y') }, /* 15 */
  { SHIFTED_KEY(kTypeLetter, 'u', 'U') }, /* 16 */
  { SHIFTED_KEY(kTypeLetter, 'i', 'I') }, /* 17 */
  { SHIFTED_KEY(kTypeLetter, 'o', 'O') }, /* 18 */
  { SHIFTED_KEY(kTypeLetter, 'p', 'P') }, /* 19 */
  { SHIFTED_KEY(kTypeSymbol, '[', '{') }, /* 1a */
  { SHIFTED_KEY(kTypeSymbol, ']', '}') }, /* 1b */
  { SINGLE_KEY(kTypeNamedKey, kKeyEnter), { kTypeNamedKey, kKeyEnter } }, /* 1c */
  { { kTypeNamedKey, kKeyControl, kLeft }, { kTypeNamedKey, kKeyControl, kLeft }, { kTypeNamedKey, kKeyControl, kRight } }, /* 1d */
  { SHIFTED_KEY(kTypeLetter, 'a', 'A') }, /* 1e */
  { SHIFTED_KEY(kTypeLetter, 's', 'S') }, /* 1f */
  { SHIFTED_KEY(kTypeLetter, 'd', 'D') }, /* 20 */
  { SHIFTED_KEY(kTypeLetter, 'f', 'F') }, /* 21 */
  { SHIFTED_KEY(kTypeLetter, 'g', 'G') }, /* 22 */
  { SHIFTED_KEY(kTypeLetter, 'h', 'H') }, /* 23 */
  { SHIFTED_KEY(kTypeLetter, 'j', 'J') }, /* 24 */
  { SHIFTED_KEY(kTypeLetter, 'k', 'K') }, /* 25 */
  { SHIFTED_KEY(kTypeLetter, 'l', 'L') }, /* 26 */
  { SHIFTED_KEY(kTypeSymbol, ';', ':') }, /* 27 */
  { SHIFTED_KEY(kTypeSymbol, '\'', '"') }, /* 28 */
  { SHIFTED_KEY(kTypeSymbol, '`', '~') }, /* 29 */
  { { kTypeNamedKey, kKeyShift, kLeft }, { kTypeNamedKey, kKeyShift, kLeft }, { kTypeNamedKey, kKeyFakeShift, kLeft } }, /* 2a */
  { SHIFTED_KEY(kTypeSymbol, '\\', '|') }, /* 2b */
  { SHIFTED_KEY(kTypeLetter, 'z', 'Z') }, /* 2c */
  { SHIFTED_KEY(kTypeLetter, 'x', 'X') }, /* 2d */
  { SHIFTED_KEY(kTypeLetter, 'c', 'C') }, /* 2e */
  { SHIFTED_KEY(kTypeLetter, 'v', 'V') }, /* 2f */
  { SHIFTED_KEY(kTypeLetter, 'b', 'B') }, /* 30 */
  { SHIFTED_KEY(kTypeLetter, 'n', 'N') }, /* 31 */
  { SHIFTED_KEY(kTypeLetter, 'm', 'M') }, /* 32 */
  { SHIFTED_KEY(kTypeSymbol, ',', '<') }, /* 33 */
  { SHIFTED_KEY(kTypeSymbol, '.', '>') }, /* 34 */
  { SHIFTED_KEY(kTypeSymbol, '/', '?'), { kTypeKeypad, '/' } }, /* 35 */
  { { kTypeNamedKey, kKeyShift, kRight }, { kTypeNamedKey, kKeyShift, kRight }, { kTypeNamedKey, kKeyFakeShift, kRight } }, /* 36 */
  { SINGLE_KEY(kTypeKeypad, '*'), { kTypeNamedKey, kKeyPrintScreen } }, /* 37 */
  { { kTypeNamedKey, kKeyAlt, kLeft }, { kTypeNamedKey, kKeyAlt, kLeft }, { kTypeNamedKey, kKeyAlt, kRight } }, /* 38 */
  { SINGLE_KEY(kTypeNamedKey, kKeySpace) }, /* 39 */
  { SINGLE_KEY(kTypeNamedKey, kKeyCapsLock) }, /* 3a */
  { SINGLE_KEY(kTypeFunction, 1) }, /* 3b */
  { SINGLE_KEY(kTypeFunction, 2) }, /* 3c */
  { SINGLE_KEY(kTypeFunction, 3) }, /* 3d */
  { SINGLE_KEY(kTypeFunction, 4) }, /* 3e */
  { SINGLE_KEY(kTypeFunction, 5) }, /* 3f */
  { SINGLE_KEY(kTypeFunction, 6) }, /* 40 */
  { SINGLE_KEY(kTypeFunction, 7) }, /* 41 */
  { SINGLE_KEY(kTypeFunction, 8) }, /* 42 */
  { SINGLE_KEY(kTypeFunction, 9) }, /* 43 */
  { SINGLE_KEY(kTypeFunction, 10) }, /* 44 */
  { SINGLE_KEY(kTypeNamedKey, kKeyNumLock) }, /* 45 */
  { SINGLE_KEY(kTypeNamedKey, kKeyScrollLock), { kTypeNamedKey, kKeyBreak } }, /* 46 */
  { KEYPAD_KEY('7', kKeyHome), { kTypeNamedKey, kKeyHome } }, /* 47 */
  { KEYPAD_KEY('8', kKeyUp), { kTypeNamedKey, kKeyUp } }, /* 48 */
  { KEYPAD_KEY('9', kKeyPageUp), { kTypeNamedKey, kKeyPageUp } }, /* 49 */
  { SINGLE_KEY(kTypeKeypad, '-') }, /* 4a */
  { KEYPAD_KEY('4', kKeyLeft), { kTypeNamedKey, kKeyLeft } }, /* 4b */
  { SINGLE_KEY(kTypeKeypad, '5') }, /* 4c */
  { KEYPAD_KEY('6', kKeyRight), { kTypeNamedKey, kKeyRight } }, /* 4d */
  { SINGLE_KEY(kTypeKeypad, '+') }, /* 4e */
  { KEYPAD_KEY('1', kKeyEnd), { kTypeNamedKey, kKeyEnd } }, /* 4f */
  { KEYPAD_KEY('2', kKeyDown), { kTypeNamedKey, kKeyDown } }, /* 50 */
  { KEYPAD_KEY('3', kKeyPageDown), { kTypeNamedKey, kKeyPageDown } }, /* 51 */
  { KEYPAD_KEY('0', kKeyInsert), { kTypeNamedKey, kKeyInsert } }, /* 52 */
  { KEYPAD_KEY('.', kKeyDelete), { kTypeNamedKey, kKeyDelete } }, /* 53 */
  {}, {}, /* 54 to 55 */
  { SINGLE_KEY(kTypeNamedKey, kKeyMeta) }, /* 56 */
  { SINGLE_KEY(kTypeFunction, 11) }, /* 57 */
  { SINGLE_KEY(kTypeFunction, 12) }, /* 58 */
};

static const int kNumScanCodes = sizeof(g_scancodes) / sizeof(g_scancodes[0]);

#if 0
static void DescribeKey(const KeyInfo& info, OutputStream* stream) {
  switch (info.type) {
    case kTypeError:
      stream->Printf("error");
      break;

    case kTypeNamedKey:
      switch (info.data) {
        case kKeyEscape:
          stream->Printf("esc");
          break;

        case kKeyBackspace:
          stream->Printf("backspace");
          break;

        case kKeyTab:
          stream->Printf("tab");
          break;

        case kKeyEnter:
          stream->Printf("enter");
          break;

        case kKeyControl:
          stream->Printf("ctrl");
          break;

        case kKeyShift:
          stream->Printf("shift");
          break;

        case kKeyFakeShift:
          stream->Printf("fakeshift");
          break;

        case kKeyAlt:
          stream->Printf("alt");
          break;

        case kKeySpace:
          stream->Printf("space");
          break;

        case kKeyCapsLock:
          stream->Printf("capslock");
          break;

        case kKeyNumLock:
          stream->Printf("numlock");
          break;

        case kKeyScrollLock:
          stream->Printf("scrolllock");
          break;

        case kKeyMeta:
          stream->Printf("meta");
          break;

        case kKeyPrintScreen:
          stream->Printf("printscreen");
          break;

        case kKeyBreak:
          stream->Printf("break");
          break;

        case kKeyHome:
          stream->Printf("home");
          break;

        case kKeyUp:
          stream->Printf("up");
          break;

        case kKeyPageUp:
          stream->Printf("pgup");
          break;

        case kKeyLeft:
          stream->Printf("left");
          break;

        case kKeyRight:
          stream->Printf("right");
          break;

        case kKeyEnd:
          stream->Printf("end");
          break;

        case kKeyDown:
          stream->Printf("down");
          break;

        case kKeyPageDown:
          stream->Printf("pgdown");
          break;

        case kKeyInsert:
          stream->Printf("insert");
          break;

        case kKeyDelete:
          stream->Printf("delete");
          break;

        default:
          stream->Printf("???");
          break;
      }
      break;

    case kTypeNumber:
      stream->Printf("num(%c)", info.data);
      break;

    case kTypeLetter:
      stream->Printf("letter(%c)", info.data);
      break;

    case kTypeSymbol:
      stream->Printf("symbol(%c)", info.data);
      break;

    case kTypeKeypad:
      stream->Printf("keypad(%c)", info.data);
      break;

    case kTypeFunction:
      stream->Printf("fn(%d)", info.data);
      break;

    default:
      stream->Printf("unknown");
      break;
  }

  if (info.side == kLeft) {
    stream->Printf("(left)");
  } else if (info.side == kRight) {
    stream->Printf("(right)");
  }
}
#endif

void kbd_ack(void) {
  while (inb(0x60) != 0xfa) {}
}

void set_keyboard_led_status(char ledstatus) {
  outb(0x60, 0xed);
  kbd_ack();
  outb(0x60, ledstatus);
  kbd_ack();
}

extern "C" {
void _start() {
  // Turn on capslock for debugging!
  set_keyboard_led_status(4);

  outb(0x60, 0xf4);
  kbd_ack();

  DebugOutputStream stream;
  ConsoleOutputStream console;

  stream.Printf("keyboard: Starting up!\n");

  SysRequestInterrupt(1);

  bool escaped = false;
  bool shift = false;
  int modifiers = 0;

  static const int kBufferSize = 1024;
  uint64_t buffer[kBufferSize];
  int buffer_start = 0, buffer_end = 0;

  static const int kMaxRequests = 8;
  int requestors[kMaxRequests];
  int num_requests = 0;

  for (;;) {
    int sender, type;
    uint64_t payload;
    SysReceive(&sender, &type, &payload);

    if (sender) {
      stream.Printf("keyboard: Got key request\n");
      if (buffer_start == buffer_end) {
        requestors[num_requests++] = sender;
      } else {
        SysSend(sender, kMsgReadReply, buffer[buffer_start]);
        buffer_start = (buffer_start + 1) % kBufferSize;
      }

      continue;
    }

    //console.Printf("Got key interrupt\r\n");

    unsigned char scan_code = inb(0x60);
    if (scan_code == 0xe0) {
      escaped = true;
    } else if ((scan_code & 0x7f) < kNumScanCodes) {
      bool keyup = scan_code & 0x80;
      scan_code = scan_code & 0x7f;

      ScanCodeInfo info = g_scancodes[scan_code];
      KeyInfo key;
      if (escaped) {
        key = info.escaped;
        escaped = false;
      } else if (shift) {
        key = info.shifted;
      } else {
        key = info.unshifted;
      }

      if (key.type == kTypeNamedKey) {
        if (key.data == kKeyShift || key.data == kKeyFakeShift) {
          shift = !keyup;
        }

        int mod = 0;
        if (key.data == kKeyShift) {
          mod = kModShift;
        } else if (key.data == kKeyAlt) {
          mod = kModAlt;
        } else if (key.data == kKeyControl) {
          mod = kModControl;
        } else if (key.data == kKeyMeta) {
          mod = kModMeta;
        }

        if (keyup) {
          modifiers &= ~mod;
        } else {
          modifiers |= mod;
        }
      }

      KeyEvent event(keyup ? kKeyUpEvent : kKeyDownEvent, modifiers, key);
      uint64_t formatted = event.Format();

      if (num_requests) {
        num_requests--;
        int recipient = requestors[num_requests];
        SysSend(recipient, kMsgReadReply, formatted);
      } else {
        buffer[buffer_end] = formatted;
        buffer_end = (buffer_end + 1) % kBufferSize;
        if (buffer_start == buffer_end) {
          buffer_start = (buffer_start + 1) % kBufferSize;
        }
      }

#if 0
      if (keyup) {
        stream.Printf("keyboard: keyup ");
      } else {
        stream.Printf("keyboard: keydown ");
      }

      DescribeKey(key, &stream);

      if (modifiers & kModShift) {
        stream.Printf("[shift]");
      }
      if (modifiers & kModAlt) {
        stream.Printf("[alt]");
      }
      if (modifiers & kModControl) {
        stream.Printf("[control]");
      }
      if (modifiers & kModMeta) {
        stream.Printf("[meta]");
      }

      stream.Printf("\n");
#endif
    }

    SysAckInterrupt(1);
  }

  SysExitThread();
}

void __cxa_pure_virtual() {
  for (;;) {}
}
}
