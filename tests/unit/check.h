// 极简测试框架：TEST 自注册，CHECK 失败只记录不中断，最后汇总全部失败并以失败数为退出码。
#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace chantest
{

struct Case
{
  const char *name;
  std::function<void()> body;
};

inline std::vector<Case> &Registry()
{
  static std::vector<Case> cases;
  return cases;
}

inline int &Failures()
{
  static int failures = 0;
  return failures;
}

struct Register
{
  Register(const char *name, std::function<void()> body) { Registry().push_back({name, std::move(body)}); }
};

inline void Fail(const char *file, int line, const char *expr)
{
  std::printf("  FAIL %s:%d  %s\n", file, line, expr);
  Failures()++;
}

}  // namespace chantest

#define CHAN_CAT2(a, b) a##b
#define CHAN_CAT(a, b) CHAN_CAT2(a, b)
#define TEST(name)                                                                   \
  static void name();                                                                \
  static chantest::Register CHAN_CAT(reg_, name)(#name, name);                       \
  static void name()
#define CHECK(expr)                                              \
  do                                                             \
  {                                                              \
    if (!(expr)) chantest::Fail(__FILE__, __LINE__, #expr);      \
  } while (0)
#define REQUIRE(expr)                                            \
  do                                                             \
  {                                                              \
    if (!(expr)) { chantest::Fail(__FILE__, __LINE__, #expr); return; } \
  } while (0)
