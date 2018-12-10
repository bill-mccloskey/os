#ifndef interrupts_h
#define interrupts_h

#include "io.h"
#include "types.h"

class Thread;

class InterruptController {
public:
  InterruptController(IoPorts* io) : io_(io) {}

  void Init();

  int InterruptNumberToIRQ(int interrupt_number);

  void Acknowledge(int irq);
  void Mask(int irq, bool allow);

  uint16_t GetRaisedInterrupts();
  uint16_t GetServicingInterrupts();

  void RegisterForInterrupt(int irq, Thread* thread);
  void UnregisterForInterrupts(Thread* thread);

  void Interrupt(int irq);

private:
  uint16_t GetRegister(int ocw3);

  IoPorts* io_;

  static const int kInterruptsPerController = 8;
  int offset1_ = 32, offset2_ = 40;

  static const int kMaxIRQs = 32;
  Thread* registrations_[kMaxIRQs] = {};
};

extern InterruptController* g_interrupts;

#endif
