#ifndef base_testing_h
#define base_testing_h

extern bool g_testing;

class AutoEnableTesting {
public:
  AutoEnableTesting() : prev_(g_testing) {
    g_testing = true;
  }
  ~AutoEnableTesting() {
    g_testing = prev_;
  }

private:
  bool prev_;
};

#define TEST_ONLY(expr) do { \
    if (g_testing) {         \
      expr;                  \
    }                        \
  } while (0)

#endif
