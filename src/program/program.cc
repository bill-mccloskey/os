struct S {
  int a;
  char b;
};

S global1[1000];
S global2[1000] = {{100, 10}};

void func() {
  global1[0].a = global2[0].a;
}

extern "C" {
int _start() {
  int x = 12;
  func();
  for (int i = 0; i < x; i++) {}
  return x;
}
}
