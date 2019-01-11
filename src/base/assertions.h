#ifndef assertions_h
#define assertions_h

void AssertionFailure(const char* filename, int line,
                      const char* msg1, const char* msg2, const char* msg3, const char* msg4) __attribute__((noreturn));

#define assert(cond) do { \
    if (!(cond)) { \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #cond, nullptr, nullptr); \
    }                                                                   \
  } while (false)

#define assert_eq(v1, v2) do {                  \
    if (!((v1) == (v2))) {                                              \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " == ", #v2); \
    } \
  } while (false)

#define assert_ne(v1, v2) do {                  \
    if (!((v1) != (v2))) {                                              \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " != ", #v2); \
    } \
  } while (false)

#define assert_lt(v1, v2) do {                  \
    if (!((v1) < (v2))) {                                               \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " < ", #v2); \
    } \
  } while (false)

#define assert_gt(v1, v2) do {                  \
    if (!((v1) > (v2))) {                                               \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " > ", #v2); \
    } \
  } while (false)

#define assert_le(v1, v2) do {                  \
    if (!((v1) <= (v2))) {                                               \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " <= ", #v2); \
    } \
  } while (false)

#define assert_ge(v1, v2) do {                  \
    if (!((v1) >= (v2))) {                                               \
      AssertionFailure(__FILE__, __LINE__, "Assertion failure: ", #v1, " >= ", #v2); \
    } \
  } while (false)

#define panic(msg) AssertionFailure(__FILE__, __LINE__, (msg), nullptr, nullptr, nullptr)

#endif  // assertions_h
