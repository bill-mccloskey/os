#ifndef drivers_terminal_emulator_h
#define drivers_terminal_emulator_h

#include "types.h"

struct CellAttributes {
  int fg_color;
  int bg_color;
};

class TerminalEmulatorOutput {
public:
  void SetCell(int x, int y, wchar_t ch, const CellAttributes& attrs) = 0;
  void Bell() = 0;

  void SetMaxY(int y);
};

class TerminalEmulator {
public:
  TerminalEmulator(TerminalEmulatorOutput* output);

  void Input(int byte);

private:
  bool IsPrintable(int byte);

  TerminalEmulatorOutput* output_;

  int state_ = 0;
  int x_ = 0, y_ = 0;
  int tab_stops_ = 8;
  CellAttributes current_attrs_;
};

#endif
