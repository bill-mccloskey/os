#include "interrupts.h"

#include "assertions.h"
#include "io.h"
#include "serial.h"
#include "thread.h"
#include "types.h"

static const int kPrimaryCommandPort = 0x20;
static const int kPrimaryDataPort = 0x21;

static const int kSecondaryCommandPort = 0xa0;
static const int kSecondaryDataPort = 0xa1;

static const int kEndOfInterruptCommand = 0x20;

extern "C" {

void kinterrupt(int64_t interrupt_number, uint64_t error_code) {
  g_serial->Printf("Interrupt %d received\n", interrupt_number);

  // Timer
  if (interrupt_number == 32) {
    outb(kPrimaryCommandPort, kEndOfInterruptCommand);
  }

  // Keyboard
  if (interrupt_number == 33) {
    unsigned char scan_code = inb(0x60);
    g_serial->Printf("Keyboard scancode = %u\n", scan_code);
    outb(kPrimaryCommandPort, kEndOfInterruptCommand);
  }

  // GPF
  if (interrupt_number == 13) {
    g_serial->Printf("GPF: error=%u\n", uint32_t(error_code));
    g_scheduler->DumpState();
    for (;;) {
      asm("hlt");
    }
  }
}

}

void InterruptController::Init() {
  unsigned char mask1 = io_->In(kPrimaryDataPort);
  unsigned char mask2 = io_->In(kSecondaryDataPort);

  g_serial->Printf("Interrupt masks = %x/%x\n", mask1, mask2);

  const int kICW1_Init = 0x10;
  const int kICW1_ICW4 = 0x01;
  const int kICW4_8086 = 0x01;

  // starts the initialization sequence (in cascade mode)
  io_->Out(kPrimaryCommandPort, kICW1_Init | kICW1_ICW4);
  io_->Out(kSecondaryCommandPort, kICW1_Init | kICW1_ICW4);

  // ICW2: Offsets.
  io_->Out(kPrimaryDataPort, offset1_);
  io_->Out(kSecondaryDataPort, offset2_);

  // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
  io_->Out(kPrimaryDataPort, 4);

  // ICW3: tell Slave PIC its cascade identity (0000 0010)
  io_->Out(kSecondaryDataPort, 2);

  io_->Out(kPrimaryDataPort, kICW4_8086);
  io_->Out(kSecondaryDataPort, kICW4_8086);

  // Restore saved masks.
  io_->Out(kPrimaryDataPort, mask1);
  io_->Out(kSecondaryDataPort, mask2);
}

void InterruptController::Acknowledge(int irq)
{
  if (irq >= 8) {
    io_->Out(kSecondaryCommandPort, kEndOfInterruptCommand);
  }

  io_->Out(kPrimaryCommandPort, kEndOfInterruptCommand);
}

void InterruptController::Mask(int irq, bool allow) {
  uint16_t port;

  if (irq < 8) {
    port = kPrimaryDataPort;
  } else {
    port = kSecondaryDataPort;
    irq -= 8;
  }
  uint8_t mask = io_->In(port);
  if (allow) {
    mask &= ~(1 << irq);
  } else {
    mask |= (1 << irq);
  }
  io_->Out(port, mask);
}

uint16_t InterruptController::GetRegister(int ocw3)
{
  io_->Out(kPrimaryCommandPort, ocw3);
  io_->Out(kSecondaryCommandPort, ocw3);
  return (io_->In(kSecondaryCommandPort) << 8) | io_->In(kPrimaryCommandPort);
}

/* Returns the combined value of the cascaded PICs irq request register */
uint16_t InterruptController::GetRaisedInterrupts()
{
  const int kReadIRR = 0xa;
  return GetRegister(kReadIRR);
}

/* Returns the combined value of the cascaded PICs in-service register */
uint16_t InterruptController::GetServicingInterrupts()
{
  const int kReadISR = 0xb;
  return GetRegister(kReadISR);
}
