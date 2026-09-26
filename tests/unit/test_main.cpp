#include "check.h"

#include <cstring>

int main(int argc, char **argv)
{
  const char *filter = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  for (const chantest::Case &c : chantest::Registry())
  {
    if (filter && !std::strstr(c.name, filter)) continue;
    int before = chantest::Failures();
    c.body();
    run++;
    if (chantest::Failures() != before) std::printf("[FAILED] %s\n", c.name);
  }
  std::printf("%d cases, %d failed checks\n", run, chantest::Failures());
  return chantest::Failures();
}
