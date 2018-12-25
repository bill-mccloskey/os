#ifndef drivers_terminal_emulator_h
#define drivers_terminal_emulator_h

#include "types.h"
#include "vtparse.h"

struct CellAttributes {
  enum Attribute {
    kBold = 1 << 0,
    kFaint = 1 << 1,
    kItalic = 1 << 2,
    kUnderline = 1 << 3,
    kBlink = 1 << 4,
    kInverse = 1 << 5,
    kInvisible = 1 << 6,
    kCrossOut = 1 << 7,
    kDoubleUnderline = 1 << 8,
  };

  enum Color {
    kBlack,
    kRed,
    kGreen,
    kYellow,
    kBlue,
    kMagenta,
    kCyan,
    kWhite,
    kDefault
  };

  CellAttributes& SetAttribute(Attribute attr) {
    attributes |= attr;
    return *this;
  }

  CellAttributes& ClearAttribute(Attribute attr) {
    attributes &= ~attr;
    return *this;
  }

  void Reset() {
    fg_color = kDefault;
    bg_color = kDefault;
    attributes = 0;
  }

  bool operator==(const CellAttributes& other) const {
    return fg_color == other.fg_color &&
           bg_color == other.bg_color &&
           attributes == other.attributes;
  }

  Color fg_color = kDefault;
  Color bg_color = kDefault;
  int attributes = 0;
};

// I want to support a ring buffer for output. Perhaps x and y coordinates will be
// in absolute coordinates. I'll also have functions to set the min and max Y
// coordinates.
class TerminalEmulatorOutput {
public:
  virtual void SetCell(int x, int y, wchar_t ch, const CellAttributes& attrs) = 0;
  virtual void Bell() = 0;

  virtual void MoveCells(int dest_x, int dest_y, int src_x, int src_y, int width, int height) = 0;

  virtual int Width() = 0;
  virtual int Height() = 0;

  virtual void CopyToScrollback(int src_y, int y_count) = 0;
};

class TerminalEmulator {
public:
  TerminalEmulator(TerminalEmulatorOutput* output);

  void Input(int byte);

  void GetCursorPosition(int* x, int* y);

private:
  static void StaticParseCallback(vtparse_t* vtstate, vtparse_action_t action, unsigned char input);
  void ParseCallback(vtparse_action_t action, int input);

  int NumParams();
  int GetParam(int index, int deflt = 0);
  char GetIntermediateChar(int index);

  void SetCharacterAttributes();

  void EraseCharacters();
  void EraseInLine();
  void EraseInDisplay();

  void SetCursorPosition();
  void CursorMove(int delta_x, int delta_y);

  void SetScrollMargins();
  int LimitTop();
  int LimitBottom();
  void SnapCursor();
  void ScrollBy(int delta_y);

  void ClearCells(int x, int y, int width, int height);

  void DECPrivateModeSet();

  void ExecuteControl(int input);
  void CSIDispatch(int input);
  void Print(int ch);

  TerminalEmulatorOutput* output_;

  int x_ = 0, y_ = 0;
  int scroll_top_ = 0;
  int scroll_bottom_;
  int tab_stops_ = 8;
  CellAttributes current_attrs_;

  vtparse_t vtstate_;
};

#endif
