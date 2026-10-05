#pragma once
// Tiny self-registering test harness (no external framework).
#include <cstdio>

struct TestCase {
    const char* name;
    void (*fn)();
    TestCase* next;
};

TestCase*& TestRegistry();
extern int g_testFailures;

struct TestRegistrar {
    TestCase test;
    TestRegistrar(const char* name, void (*fn)()) : test{name, fn, TestRegistry()} { TestRegistry() = &test; }
};

#define TEST(name)                                                   \
    static void name();                                              \
    static TestRegistrar name##_registrar(#name, &name);             \
    static void name()

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
            ++g_testFailures;                                                          \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        auto va_ = (a);                                                                             \
        auto vb_ = (b);                                                                             \
        if (!(va_ == vb_)) {                                                                        \
            std::printf("  FAILED %s:%d: %s == %s (0x%llX vs 0x%llX)\n", __FILE__, __LINE__, #a, #b, \
                        static_cast<unsigned long long>(va_), static_cast<unsigned long long>(vb_));  \
            ++g_testFailures;                                                                       \
        }                                                                                           \
    } while (0)
