#include "base/testing.h"

bool g_testing =
#ifdef TEST_BUILD
  true;
#else
  false;
#endif
