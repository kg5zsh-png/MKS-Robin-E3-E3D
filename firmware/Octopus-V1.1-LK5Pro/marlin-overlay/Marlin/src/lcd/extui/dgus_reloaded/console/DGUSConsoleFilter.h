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
 * lcd/extui/dgus_reloaded/console/DGUSConsoleFilter.h
 *
 * Decides which G-code is too noisy to mirror onto the panel. Kept free of
 * Marlin headers so the parsing rules can be exercised by a host-side test
 * (see buildroot/share/tests/dgus_console/).
 *
 * A running print is dominated by motion commands and host temperature polls.
 * Echoing those fills the four-line window many times per second and hides
 * everything worth reading, so they are dropped by default.
 */

#include <stdint.h>

/**
 * True if `command` should be suppressed: G0/G1/G2/G3 moves, and the M105 /
 * M114 / M155 report commands. A leading line number (Nnnn) and surrounding
 * whitespace are skipped. A null or empty command is suppressed.
 */
inline bool dgusConsoleIsNoisyGcode(const char * const command) {
  if (!command) return true;

  const char *p = command;
  while (*p == ' ' || *p == '\t') ++p;

  // Skip an optional line number.
  if (*p == 'N' || *p == 'n') {
    const char *after = p + 1;
    if (*after >= '0' && *after <= '9') {
      while (*after >= '0' && *after <= '9') ++after;
      p = after;
      while (*p == ' ' || *p == '\t') ++p;
    }
  }

  if (!*p) return true;  // blank line

  const char letter = *p;
  const bool is_g = (letter == 'G' || letter == 'g');
  const bool is_m = (letter == 'M' || letter == 'm');
  if (!is_g && !is_m) return false;

  const char *d = p + 1;
  if (*d < '0' || *d > '9') return false;  // not a numbered command

  uint16_t number = 0;
  while (*d >= '0' && *d <= '9') number = (uint16_t)(number * 10 + (*d++ - '0'));

  // Reject a fractional command such as G1.5 rather than treating it as G1.
  if (*d == '.') return false;

  if (is_g) return number <= 3;               // G0/G1/G2/G3 moves

  return number == 105 || number == 114 || number == 155;
}
