struct S {
  int a;
  char b;
};

S global1[1000];
S global2[1000] = {{100, 10}};

void func() {
  global1[0].a = global2[0].a;
}

using uint64_t = unsigned long;

__asm__(
  ".globl syscall\n"
  "syscall:\n"
  "int $0x80\n"
  "ret\n");

extern "C" {
uint64_t syscall(uint64_t number, uint64_t arg1 = 0, uint64_t arg2 = 0, uint64_t arg3 = 0,
                 uint64_t arg4 = 0, uint64_t arg5 = 0);

int _start() {
  asm("xchg %bx, %bx");
  syscall(3);
  int x = 12;
  func();
  for (int i = 0; i < x; i++) {}
  return x;
}
}
