// check.h -- the smallest test harness: CHECK(cond) counts and prints failures, DONE() returns the exit code.
#pragma once
#include <stdio.h>

static int g_checks = 0, g_fails = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { g_fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define DONE(name) (printf("%s: %d checks, %d failed\n", name, g_checks, g_fails), g_fails ? 1 : 0)
