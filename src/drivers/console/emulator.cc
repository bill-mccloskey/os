// FIXME: Wrapping. Also, vertical wrapping (how does that work?).

#include "emulator.h"
#include "base/assertions.h"

static const int kBufferEnd = 0;
static const int kBEL = 7;
static const int kBS = 8;
static const int kHT = 9;
static const int kLF = 10;
static const int kVT = 11;
static const int kFF = 12;
static const int kCR = 13;
static const int kESC = 27;
static const int kDEL = 127;

TerminalEmulator::TerminalEmulator(TerminalEmulatorOutput* output)
  : output_(output),
    scroll_bottom_(output_->Height() - 1) {
  vtparse_init(&vtstate_, &TerminalEmulator::StaticParseCallback);
  vtstate_.user_data = this;
}

void TerminalEmulator::SetCharacterAttributes() {
  if (NumParams() == 0) {
    current_attrs_.Reset();
    return;
  }

  CellAttributes::Color colors[] = {
    CellAttributes::kBlack,
    CellAttributes::kRed,
    CellAttributes::kGreen,
    CellAttributes::kYellow,
    CellAttributes::kBlue,
    CellAttributes::kMagenta,
    CellAttributes::kCyan,
    CellAttributes::kWhite,
    CellAttributes::kDefault
  };

  for (int i = 0; i < NumParams(); i++) {
    int arg = GetParam(i);

    if (arg == 0) {
      current_attrs_ = CellAttributes();
    } else if (arg == 1) {
      current_attrs_.SetAttribute(CellAttributes::kBold);
      current_attrs_.ClearAttribute(CellAttributes::kFaint);
    } else if (arg == 2) {
      current_attrs_.ClearAttribute(CellAttributes::kBold);
      current_attrs_.SetAttribute(CellAttributes::kFaint);
    } else if (arg >= 3 && arg <= 9) {
      CellAttributes::Attribute attrs[] = {
        CellAttributes::kItalic, CellAttributes::kUnderline,
        CellAttributes::kBlink, static_cast<CellAttributes::Attribute>(0), CellAttributes::kInverse,
        CellAttributes::kInvisible, CellAttributes::kCrossOut
      };
      current_attrs_.SetAttribute(attrs[arg - 3]);
    } else if (arg == 21) {
      current_attrs_.SetAttribute(CellAttributes::kDoubleUnderline);
    } else if (arg == 22) {
      current_attrs_.ClearAttribute(CellAttributes::kBold);
      current_attrs_.ClearAttribute(CellAttributes::kFaint);
    } else if (arg >= 23 && arg <= 29) {
      CellAttributes::Attribute attrs[] = {
        CellAttributes::kItalic, CellAttributes::kUnderline,
        CellAttributes::kBlink, static_cast<CellAttributes::Attribute>(0), CellAttributes::kInverse,
        CellAttributes::kInvisible, CellAttributes::kCrossOut
      };
      current_attrs_.ClearAttribute(attrs[arg - 23]);
    } else if (arg >= 30 && arg <= 39) {
      current_attrs_.fg_color = colors[arg - 30];
    } else if (arg >= 40 && arg <= 49) {
      current_attrs_.bg_color = colors[arg - 40];
    }
  }
}

void TerminalEmulator::EraseCharacters() {
  int count = GetParam(0, 1);

  output_->MoveCells(x_, y_, x_ + count, y_, output_->Width(), 1);

  for (int i = 0; i < count; i++) {
    output_->SetCell(output_->Width() - i - 1, y_, ' ', current_attrs_);
  }
}

void TerminalEmulator::EraseInLine() {
  int arg = GetParam(0);

  int start = 0, end = 0;
  if (arg == 0) {
    start = x_;
    end = output_->Width();
  } else if (arg == 1) {
    start = 0;
    end = x_ + 1;
  } else if (arg == 2) {
    start = 0;
    end = output_->Width();
  }

  for (int i = start; i < end; i++) {
    output_->SetCell(i, y_, ' ', current_attrs_);
  }
}

void TerminalEmulator::EraseInDisplay() {
  int arg = GetParam(0);

  if (arg == 0) {
    // Erase below.
    ClearCells(0, y_ + 1, output_->Width(), output_->Height() - y_ - 1);
  } else if (arg == 1) {
    // Erase above.
    ClearCells(0, 0, output_->Width(), y_ + 1);
  } else if (arg == 2) {
    // Erase all.
    ClearCells(0, 0, output_->Width(), output_->Height());
  }
}

void TerminalEmulator::SetCursorPosition() {
  int y = GetParam(0, 1) - 1;
  int x = GetParam(1, 1) - 1;

  x_ = x;
  y_ = y;
  SnapCursor();
}

void TerminalEmulator::CursorMove(int delta_x, int delta_y) {
  x_ += delta_x;
  y_ += delta_y;

  SnapCursor();
}

void TerminalEmulator::SetScrollMargins() {
  if (NumParams() == 0) {
    scroll_top_ = 0;
    scroll_bottom_ = output_->Height() - 1;
  } else if (NumParams() == 1) {
    scroll_top_ = GetParam(0) - 1;
    scroll_bottom_ = output_->Height() - 1;
  } else if (NumParams() == 2) {
    scroll_top_ = GetParam(0) - 1;
    scroll_bottom_ = GetParam(1) - 1;
  }

  x_ = y_ = 0;
  SnapCursor();
}

int TerminalEmulator::LimitTop() {
  return 0;
}

int TerminalEmulator::LimitBottom() {
  return output_->Height() - 1;
}

void TerminalEmulator::SnapCursor() {
  if (x_ < 0) x_ = 0;
  if (x_ >= output_->Width()) x_ = output_->Width() - 1;

  if (y_ < LimitTop()) y_ = LimitTop();
  if (y_ > LimitBottom()) y_ = LimitBottom();
}

// A delta_y that is positive means we're scrolling up.
void TerminalEmulator::ScrollBy(int delta_y) {
  if (delta_y == 0) {
    return;
  }

  int window_size = scroll_bottom_ - scroll_top_ + 1;

  int scroll_size = delta_y;
  if (scroll_size < 0) {
    scroll_size = -scroll_size;
  }
  if (scroll_size > window_size) {
    scroll_size = window_size;
  }

  int move_size = window_size - scroll_size;

  if (delta_y > 0) {
    if (scroll_top_ == 0) {
      output_->CopyToScrollback(scroll_top_, scroll_size);
    }
    output_->MoveCells(0, scroll_top_, 0, scroll_top_ + scroll_size, output_->Width(), move_size);
    ClearCells(0, scroll_bottom_ + 1 - scroll_size, output_->Width(), scroll_size);
  } else {
    output_->MoveCells(0, scroll_top_ + scroll_size, 0, scroll_top_, output_->Width(), move_size);
    ClearCells(0, scroll_top_, output_->Width(), scroll_size);
  }
}

void TerminalEmulator::ClearCells(int x, int y, int width, int height) {
  for (int py = y; py < y + height; py++) {
    for (int px = x; px < x + width; px++) {
      output_->SetCell(px, py, ' ', current_attrs_);
    }
  }
}

void TerminalEmulator::DECPrivateModeSet() {
  int arg = GetParam(0);
  switch (arg) {
    case 1049:
      //SaveCursor();
      //AlternateScreenBuffer();
      break;
  }
}

void TerminalEmulator::ExecuteControl(int input) {
  switch (input) {
    case kBEL:
      output_->Bell();
      break;

    case kBS:
      if (x_ > 0) x_--;
      break;

    case kHT:
      x_ = (x_ + tab_stops_) % tab_stops_;
      break;

    case kLF:
    case kVT:
    case kFF:
      if (y_ == scroll_bottom_) {
        ScrollBy(1);
      } else {
        y_++;
      }
      break;

    case kCR:
      x_ = 0;
      break;
  }
}

void TerminalEmulator::CSIDispatch(int input) {
  switch (input) {
    case 'h':
      if (GetIntermediateChar(0) == '?') {
        DECPrivateModeSet();
      }
      break;

    case 'm':
      SetCharacterAttributes();
      break;

    case 'r':
      SetScrollMargins();
      break;

    case 'A':
      CursorMove(0, -GetParam(0, 1));
      break;

    case 'B':
      CursorMove(0, GetParam(0, 1));
      break;

    case 'C':
      CursorMove(GetParam(0, 1), 0);
      break;

    case 'D':
      CursorMove(-GetParam(0, 1), 0);
      break;

    case 'H':
      SetCursorPosition();
      break;

    case 'J':
      EraseInDisplay();
      break;

    case 'K':
      EraseInLine();
      break;

    case 'P':
      EraseCharacters();
      break;

    case 'S':
      ScrollBy(GetParam(0, 1));
      break;

    case 'T':
      ScrollBy(-GetParam(0, 1));
      break;

    default:
      assert(false);
      break;
  }
}

void TerminalEmulator::Print(int ch) {
  output_->SetCell(x_, y_, ch, current_attrs_);
  if (x_ < output_->Width() - 1) {
    x_++;
  }
}

void TerminalEmulator::StaticParseCallback(vtparse_t* vtstate, vtparse_action_t action, unsigned char input) {
  TerminalEmulator* em = static_cast<TerminalEmulator*>(vtstate->user_data);
  em->ParseCallback(action, input);
}

void TerminalEmulator::ParseCallback(vtparse_action_t action, int input) {
  switch (action) {
    case VTPARSE_ACTION_EXECUTE:
      ExecuteControl(input);
      break;

    case VTPARSE_ACTION_CSI_DISPATCH:
      CSIDispatch(input);
      break;

    case VTPARSE_ACTION_PRINT:
      Print(input);
      break;

    default:
      break;
  }
}

int TerminalEmulator::NumParams() {
  return vtstate_.num_params;
}

int TerminalEmulator::GetParam(int index, int deflt) {
  if (index < vtstate_.num_params) {
    if (vtstate_.params[index] == 0) {
      return deflt;
    } else {
      return vtstate_.params[index];
    }
  } else {
    return deflt;
  }
}

char TerminalEmulator::GetIntermediateChar(int index) {
  if (index >= vtstate_.num_intermediate_chars) {
    return 0;
  }
  return vtstate_.intermediate_chars[index];
}

void TerminalEmulator::Input(int byte) {
  unsigned char ch = byte;
  vtparse(&vtstate_, &ch, 1);
}

void TerminalEmulator::GetCursorPosition(int* x, int* y) {
  *x = x_;
  *y = y_;
}
