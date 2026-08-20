/**
 * Host-side test for dgusConsoleIsNoisyGcode().
 *
 *   g++ -std=c++11 -Wall -Wextra -o test_console_filter test_console_filter.cpp && ./test_console_filter
 */

#include "../../../../Marlin/src/lcd/extui/dgus_reloaded/console/DGUSConsoleFilter.h"

#include <cstdio>

static int failures = 0;

static void expect(const char *cmd, bool want) {
  const bool got = dgusConsoleIsNoisyGcode(cmd);
  if (got != want) {
    printf("  FAIL \"%s\": got %s, want %s\n",
           cmd ? cmd : "(null)", got ? "noisy" : "shown", want ? "noisy" : "shown");
    ++failures;
  }
}

static const bool NOISY = true, SHOWN = false;

int main() {
  printf("motion commands are suppressed\n");
  expect("G0 X10 Y10", NOISY);
  expect("G1 X10 Y10 E5 F1500", NOISY);
  expect("G2 I10 J10", NOISY);
  expect("G3 I10 J10", NOISY);

  printf("host polling commands are suppressed\n");
  expect("M105", NOISY);
  expect("M114", NOISY);
  expect("M155 S1", NOISY);

  printf("interesting commands are shown\n");
  expect("G28", SHOWN);          // home
  expect("G29", SHOWN);          // bed leveling
  expect("G4 P1000", SHOWN);     // dwell
  expect("M104 S200", SHOWN);    // set hotend temp
  expect("M109 S200", SHOWN);
  expect("M140 S60", SHOWN);
  expect("M600", SHOWN);         // filament change
  expect("M117 Hello", SHOWN);
  expect("M115", SHOWN);         // near M114, must not be caught

  printf("line numbers are skipped before classifying\n");
  expect("N12 G1 X10", NOISY);
  expect("N12 M105", NOISY);
  expect("N12 G28", SHOWN);
  expect("N999999 G1 X1", NOISY);

  printf("leading whitespace is tolerated\n");
  expect("   G1 X10", NOISY);
  expect("\tG28", SHOWN);
  expect("  N5  G1 X1", NOISY);

  printf("lowercase is handled\n");
  expect("g1 x10", NOISY);
  expect("m105", NOISY);
  expect("g28", SHOWN);

  printf("blank and null input is suppressed, not echoed\n");
  expect(NULL, NOISY);
  expect("", NOISY);
  expect("   ", NOISY);

  printf("non-numbered and non-G/M lines are shown\n");
  expect("T0", SHOWN);           // tool change
  expect(";just a comment", SHOWN);
  expect("GX", SHOWN);           // malformed, do not silently swallow
  expect("M", SHOWN);
  expect("G", SHOWN);

  printf("fractional commands are not mistaken for their integer form\n");
  expect("G1.5 X10", SHOWN);
  expect("G0.1", SHOWN);

  printf("a bare N with no digits is not treated as a line number\n");
  expect("NG1", SHOWN);

  if (failures) { printf("\n%d check(s) FAILED\n", failures); return 1; }
  printf("\nall checks passed\n");
  return 0;
}
