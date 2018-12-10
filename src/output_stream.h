#ifndef printf_h
#define printf_h

class OutputStream {
public:
  virtual void OutputChar(char c) = 0;

  void OutputString(const char* s);

  void Printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
};

#endif
