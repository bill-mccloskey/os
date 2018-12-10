#include "emulator.h"

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
  : output_(output) {}

bool TerminalEmulator::IsPrintable(int byte) {
  return byte >= 0x20 && byte <= 0x7e;
}

void TerminalEmulator::Input(int byte) {
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
      y_++;
      output_->SetMaxY(y_);
      break;

    case kCR:
      x_ = 0;
      break;

    case kESC:
      break;

    case kDEL:
      break;
  }
}

