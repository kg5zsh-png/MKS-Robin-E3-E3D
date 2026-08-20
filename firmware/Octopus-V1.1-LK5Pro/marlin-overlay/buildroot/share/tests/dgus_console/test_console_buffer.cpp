/**
 * Host-side test for DGUSConsoleBuffer.
 *
 * The printer-side code cannot be exercised without hardware, but the
 * scrollback rules are pure logic, so they are checked here.
 *
 *   g++ -std=c++11 -Wall -Wextra -o test_console_buffer test_console_buffer.cpp && ./test_console_buffer
 */

#include "../../../../Marlin/src/lcd/extui/dgus_reloaded/console/DGUSConsoleBuffer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

static int failures = 0;

static void expect_line(const char *what, const char *got, const char *want) {
  if (strcmp(got, want) != 0) {
    printf("  FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
    ++failures;
  }
}

static void expect_u8(const char *what, unsigned got, unsigned want) {
  if (got != want) {
    printf("  FAIL %s: got %u, want %u\n", what, got, want);
    ++failures;
  }
}

int main() {
  // A 4-row window matches the panel's MESSAGE_Line1..MESSAGE_Line4.
  printf("fills top-down before scrolling\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    expect_u8("empty visible", buf.visible(), 0);
    expect_line("empty row 0", buf.line(0), "");

    buf.push("one");
    buf.push("two");
    expect_u8("visible after 2", buf.visible(), 2);
    expect_line("row 0", buf.line(0), "one");
    expect_line("row 1", buf.line(1), "two");
    expect_line("row 2 blank", buf.line(2), "");
    expect_line("row 3 blank", buf.line(3), "");
  }

  printf("exactly full keeps insertion order\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    const char *in[] = { "a", "b", "c", "d" };
    for (int i = 0; i < 4; ++i) buf.push(in[i]);
    expect_u8("visible", buf.visible(), 4);
    for (int i = 0; i < 4; ++i) expect_line("row", buf.line(i), in[i]);
  }

  printf("scrolls oldest off the top, newest at the bottom\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    const char *in[] = { "l1", "l2", "l3", "l4", "l5" };
    for (int i = 0; i < 5; ++i) buf.push(in[i]);
    expect_u8("visible stays capped", buf.visible(), 4);
    expect_line("row 0", buf.line(0), "l2");
    expect_line("row 1", buf.line(1), "l3");
    expect_line("row 2", buf.line(2), "l4");
    expect_line("row 3 newest", buf.line(3), "l5");
  }

  printf("survives many wraps (ring index stays correct)\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    char tmp[16];
    for (int i = 0; i < 1000; ++i) { snprintf(tmp, sizeof(tmp), "n%d", i); buf.push(tmp); }
    for (int r = 0; r < 4; ++r) {
      snprintf(tmp, sizeof(tmp), "n%d", 996 + r);
      expect_line("wrapped row", buf.line(r), tmp);
    }
  }

  printf("truncates to COLS and always null-terminates\n");
  {
    DGUSConsoleBuffer<4, 8> buf;
    buf.push("0123456789ABCDEF");
    expect_line("truncated", buf.line(0), "01234567");
    expect_u8("length", (unsigned)strlen(buf.line(0)), 8);
  }

  printf("null and empty text still consume a row\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    buf.push("x");
    buf.push(NULL);
    buf.push("");
    expect_u8("visible", buf.visible(), 3);
    expect_line("row 0", buf.line(0), "x");
    expect_line("row 1 null", buf.line(1), "");
    expect_line("row 2 empty", buf.line(2), "");
  }

  printf("revision advances on every push\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    const uint32_t r0 = buf.rev();
    buf.push("a");
    const uint32_t r1 = buf.rev();
    if (r1 == r0) { printf("  FAIL rev did not advance\n"); ++failures; }
    buf.push("b");
    if (buf.rev() == r1) { printf("  FAIL rev did not advance again\n"); ++failures; }
  }

  printf("clear() empties the window\n");
  {
    DGUSConsoleBuffer<4, 32> buf;
    buf.push("a"); buf.push("b");
    buf.clear();
    expect_u8("visible", buf.visible(), 0);
    expect_line("row 0", buf.line(0), "");
  }

  if (failures) { printf("\n%d check(s) FAILED\n", failures); return 1; }
  printf("\nall checks passed\n");
  return 0;
}
