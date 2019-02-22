#ifndef base_output_stream_h
#define base_output_stream_h

class OutputStream {
public:
  virtual void OutputChar(char c) = 0;

  void OutputString(const char* s);
  void Printf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

  OutputStream& operator<<(const char* s);
  OutputStream& operator<<(int x);
  OutputStream& operator<<(unsigned int x);
  OutputStream& operator<<(long x);
  OutputStream& operator<<(unsigned long x);
  OutputStream& operator<<(long long x);
  OutputStream& operator<<(unsigned long long x);
  OutputStream& operator<<(void* x);
  OutputStream& operator<<(bool x);
};

class TerminatingOutputStream : public OutputStream {
public:
  TerminatingOutputStream(OutputStream& stream) : stream_(stream) {}
  ~TerminatingOutputStream() {
    stream_.OutputChar('\n');
  }

  void OutputChar(char c) override {
    stream_.OutputChar(c);
  }

private:
  OutputStream& stream_;
};

enum LogLevel {
  DEBUG,
  INFO,
  WARNING,
  ERROR,
  FATAL,
};

TerminatingOutputStream Log(const char* filename, int line, LogLevel level);

#define LOG(level) Log(__FILE__, __LINE__, level)

#endif
