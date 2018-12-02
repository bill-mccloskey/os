#ifndef assertions_h
#define assertions_h

#ifdef TESTING

#include <assert.h>

#define assert_eq(v1, v2) assert((v1) == (v2))
#define assert_ne(v1, v2) assert((v1) != (v2))
#define assert_lt(v1, v2) assert((v1) < (v2))
#define assert_gt(v1, v2) assert((v1) > (v2))
#define assert_le(v1, v2) assert((v1) <= (v2))
#define assert_ge(v1, v2) assert((v1) >= (v2))

#define panic(msg) assert((msg) != nullptr)

#else  // #ifdef TESTING

void AssertionFailure(const char* filename, int line, const char* msg1, const char* msg2, const char* msg3, const char* msg4);

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

#endif  // TESTING

#endif  // assertions_h
