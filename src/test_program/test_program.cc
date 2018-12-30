#include "keyboard.h"
#include "output_stream.h"
#include "system.h"

class DebugOutputStream : public OutputStream {
public:
  void OutputChar(char c) override {
    SysWriteByte(c);
  }
};

extern "C" {
void _start() {
  DebugOutputStream stream;
  stream.Printf("test_program: Hello from the test program!\n");

  for (;;) {
    // Request a key
    SysSend(2, 0, 0);

    int sender, type;
    uint64_t payload;
    SysReceive(&sender, &type, &payload);

    KeyEvent key_event(payload);

    if (key_event.type == kKeyDownEvent) {
      switch (key_event.key.type) {
        case kTypeNumber:
        case kTypeLetter:
        case kTypeSymbol:
        case kTypeKeypad:
          SysSend(1, 0, key_event.key.data);
          break;

        case kTypeNamedKey:
          switch (key_event.key.data) {
            case kKeyBackspace:
              stream.Printf("BACKSPACE\n");
              SysSend(1, 0, '\b');
              break;

            case kKeyTab:
              SysSend(1, 0, '\t');
              break;

            case kKeyEnter:
              SysSend(1, 0, '\r');
              SysSend(1, 0, '\n');
              break;

            case kKeySpace:
              SysSend(1, 0, ' ');
              break;

            case kKeyLeft:
              SysSend(1, 0, '\e');
              SysSend(1, 0, '[');
              SysSend(1, 0, 'D');
              break;

            case kKeyRight:
              SysSend(1, 0, '\e');
              SysSend(1, 0, '[');
              SysSend(1, 0, 'C');
              break;

            case kKeyUp:
              SysSend(1, 0, '\e');
              SysSend(1, 0, '[');
              SysSend(1, 0, 'A');
              break;

            case kKeyDown:
              SysSend(1, 0, '\e');
              SysSend(1, 0, '[');
              SysSend(1, 0, 'B');
              break;

            default:
              break;
          }
          break;

        default:
          break;
      }
    }
  }

  stream.Printf("test_program: Finished send\n");

  SysExitThread();
}

void __cxa_pure_virtual() {
  for (;;) {}
}
}
