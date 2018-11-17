#ifndef interrupts_h
#define interrupts_h

#include "io.h"
#include "types.h"

class InterruptController {
public:
  InterruptController(IoPorts* io) : io_(io) {}

  void Init();
  void Acknowledge(int irq);
  void Mask(int irq, bool allow);

  uint16_t GetRaisedInterrupts();
  uint16_t GetServicingInterrupts();

private:
  uint16_t GetRegister(int ocw3);

  IoPorts* io_;
  int offset1_ = 32, offset2_ = 40;
};

#endif
