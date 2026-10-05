#include "test.h"

int g_testFailures = 0;

TestCase*& TestRegistry()
{
    static TestCase* head = nullptr;
    return head;
}

int main()
{
    int count = 0;
    for (TestCase* t = TestRegistry(); t; t = t->next) {
        int before = g_testFailures;
        t->fn();
        std::printf("%s %s\n", g_testFailures == before ? "ok  " : "FAIL", t->name);
        ++count;
    }
    std::printf("%d tests, %d failed checks\n", count, g_testFailures);
    return g_testFailures == 0 ? 0 : 1;
}
