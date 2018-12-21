#include "emulator.h"
#include "assertions.h"

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
}

bool TerminalEmulator::IsPrintable(int byte) {
  return byte >= 0x20 && byte <= 0x7e;
}

void TerminalEmulator::Buffer(int byte) {
  assert(buffer_size_ < kBufferSize);
  buffer_[buffer_size_] = byte;
  buffer_size_++;
}

int TerminalEmulator::GetBuffered() {
  if (buffer_ptr_ == buffer_size_) {
    return kBufferEnd;
  }
  return buffer_[buffer_ptr_++];
}

int TerminalEmulator::PeekBuffered() {
  if (buffer_ptr_ == buffer_size_) {
    return kBufferEnd;
  }
  return buffer_[buffer_ptr_];
}

void TerminalEmulator::UnGetBuffered() {
  assert_gt(buffer_ptr_, 0);
  buffer_ptr_--;
}

void TerminalEmulator::ClearBuffer() {
  buffer_ptr_ = 0;
  buffer_size_ = 0;
}

int TerminalEmulator::ParseDigits() {
  int n = 0;
  for (;;) {
    int ch = GetBuffered();
    if (ch == kBufferEnd) {
      return -1;
    }

    if (ch >= '0' && ch <= '9') {
      n = n * 10 + (ch - '0');
    } else {
      UnGetBuffered();
      return n;
    }
  }
}

void TerminalEmulator::ParseOperatingSystemCommand() {
  int arg = ParseDigits();
  if (arg == -1) return;

  int ch = GetBuffered();
  if (ch == kBufferEnd) {
    return;
  } else if (ch != ';') {
    // FIXME: Is this the right thing to do?
    ClearBuffer();
    return;
  }

  // Change icon name and window title
  if (arg == 0) {
    for (;;) {
      ch = GetBuffered();
      if (ch == kBEL) {
        ClearBuffer();
        return;
      } else if (ch == kBufferEnd) {
        return;
      }
    }
  } else {
    ClearBuffer();
    return;
  }
}

void TerminalEmulator::ParseCharacterAttributes(int* args, int nargs) {
  if (nargs == 0) {
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

  for (int i = 0; i < nargs; i++) {
    if (args[i] == 0) {
      current_attrs_ = CellAttributes();
    } else if (args[i] == 1) {
      current_attrs_.SetAttribute(CellAttributes::kBold);
      current_attrs_.ClearAttribute(CellAttributes::kFaint);
    } else if (args[i] == 2) {
      current_attrs_.ClearAttribute(CellAttributes::kBold);
      current_attrs_.SetAttribute(CellAttributes::kFaint);
    } else if (args[i] >= 3 && args[i] <= 9) {
      CellAttributes::Attribute attrs[] = {
        CellAttributes::kItalic, CellAttributes::kUnderline,
        CellAttributes::kBlink, static_cast<CellAttributes::Attribute>(0), CellAttributes::kInverse,
        CellAttributes::kInvisible, CellAttributes::kCrossOut
      };
      current_attrs_.SetAttribute(attrs[args[i] - 3]);
    } else if (args[i] == 21) {
      current_attrs_.SetAttribute(CellAttributes::kDoubleUnderline);
    } else if (args[i] == 22) {
      current_attrs_.ClearAttribute(CellAttributes::kBold);
      current_attrs_.ClearAttribute(CellAttributes::kFaint);
    } else if (args[i] >= 23 && args[i] <= 29) {
      CellAttributes::Attribute attrs[] = {
        CellAttributes::kItalic, CellAttributes::kUnderline,
        CellAttributes::kBlink, static_cast<CellAttributes::Attribute>(0), CellAttributes::kInverse,
        CellAttributes::kInvisible, CellAttributes::kCrossOut
      };
      current_attrs_.ClearAttribute(attrs[args[i] - 23]);
    } else if (args[i] >= 30 && args[i] <= 39) {
      current_attrs_.fg_color = colors[args[i] - 30];
    } else if (args[i] >= 40 && args[i] <= 49) {
      current_attrs_.bg_color = colors[args[i] - 40];
    }
  }
}

void TerminalEmulator::EraseCharacters(int count) {
  output_->MoveCells(x_, y_, x_ + count, y_, output_->Width(), 1);

  for (int i = 0; i < count; i++) {
    output_->SetCell(output_->Width() - i - 1, y_, ' ', current_attrs_);
  }
}

void TerminalEmulator::EraseInLine(int arg) {
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

void TerminalEmulator::SetScrollMargins(int* args, int nargs) {
  if (nargs == 0) {
    scroll_top_ = 0;
    scroll_bottom_ = output_->Height() - 1;
  } else if (nargs == 1) {
    scroll_top_ = args[0] - 1;
    scroll_bottom_ = output_->Height() - 1;
  } else if (nargs == 2) {
    scroll_top_ = args[0] - 1;
    scroll_bottom_ = args[1] - 1;
  }

  x_ = y_ = 0;
}

void TerminalEmulator::ParseControlSequenceIntroducer() {
  int ch = PeekBuffered();
  if (ch == kBufferEnd) {
    return;
  }

  int args[8];
  int nargs = 0;

  if (ch >= '0' && ch <= '9') {
    args[nargs++] = ParseDigits();
    if (args[nargs - 1] == -1) return;

    ch = GetBuffered();
    while (ch == ';') {
      assert_le(nargs, 8);
      args[nargs++] = ParseDigits();
      if (args[nargs - 1] == -1) return;

      ch = GetBuffered();
    }
  } else {
    ch = GetBuffered();
  }

  switch (ch) {
    case 'm':
      ParseCharacterAttributes(args, nargs);
      break;

    case 'r':
      SetScrollMargins(args, nargs);
      break;

    case 'K':
      EraseInLine(nargs == 0 ? 0 : args[0]);
      break;

    case 'P':
      EraseCharacters(nargs == 0 ? 1 : args[0]);
      break;

    case kBufferEnd:
      return;
  }

  ClearBuffer();
}

void TerminalEmulator::ParseEscaped() {
  switch (GetBuffered()) {
    case ']':  // OSC
      ParseOperatingSystemCommand();
      break;

    case '[':
      ParseControlSequenceIntroducer();
      break;

    case kBufferEnd:
      return;
    default:
      return; // FIXME
  }
}

void TerminalEmulator::ParseBuffer() {
  buffer_ptr_ = 0;

  switch (GetBuffered()) {
    case kESC:
      ParseEscaped();
      break;

    case kBufferEnd:
      return;
    default:
      return; // FIXME
  }
}

void TerminalEmulator::ClearCells(int x, int y, int width, int height) {
  for (int py = y; py < y + height; py++) {
    for (int px = x; px < x + width; px++) {
      output_->SetCell(px, py, ' ', current_attrs_);
    }
  }
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

void TerminalEmulator::Input(int byte) {
  if (buffer_size_) {
    Buffer(byte);
    ParseBuffer();
    return;
  }

  if (IsPrintable(byte)) {
    output_->SetCell(x_, y_, byte, current_attrs_);
    x_++;
    return;
  }

  switch (byte) {
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

    case kESC:
      Buffer(byte);
      break;

    case kDEL:
      break;
  }
}

