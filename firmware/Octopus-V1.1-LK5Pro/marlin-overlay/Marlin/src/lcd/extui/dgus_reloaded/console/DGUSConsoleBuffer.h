/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2021 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

/**
 * lcd/extui/dgus_reloaded/console/DGUSConsoleBuffer.h
 *
 * Scrollback ring for the DGUS console. Deliberately free of Marlin headers so
 * the scrolling and truncation rules can be exercised by a host-side test
 * (see buildroot/share/tests/dgus_console/) instead of only on the printer.
 *
 * Lines fill top-down until the window is full, then the window scrolls: the
 * oldest line falls off the top and the newest always occupies the bottom row.
 */

#include <stdint.h>
#include <string.h>

template <uint8_t ROWS, uint8_t COLS>
class DGUSConsoleBuffer {
public:
  DGUSConsoleBuffer() { clear(); }

  void clear() {
    for (uint8_t i = 0; i < ROWS; ++i) rows[i][0] = '\0';
    total = 0;
    revision = 0;
  }

  /**
   * Append a line. Longer text is truncated to COLS characters; a null or
   * empty pointer still consumes a row so blank separator lines are possible.
   */
  void push(const char * const text) {
    char * const dst = rows[total % ROWS];
    if (text) {
      uint8_t i = 0;
      for (; i < COLS && text[i]; ++i) dst[i] = text[i];
      dst[i] = '\0';
    }
    else
      dst[0] = '\0';

    ++total;
    ++revision;
  }

  // Number of rows currently holding text (never more than ROWS).
  uint8_t visible() const { return total < ROWS ? (uint8_t)total : ROWS; }

  /**
   * Text for display row `row` (0 = top). Rows past the filled count return an
   * empty string so callers can blank the remainder of the window.
   */
  const char * line(const uint8_t row) const {
    if (row >= visible()) return "";
    const uint32_t start = total <= ROWS ? 0 : total % ROWS;
    return rows[(start + row) % ROWS];
  }

  // Bumped on every push so a renderer can skip unchanged frames.
  uint32_t rev() const { return revision; }

private:
  char rows[ROWS][COLS + 1];
  uint32_t total;
  uint32_t revision;
};
