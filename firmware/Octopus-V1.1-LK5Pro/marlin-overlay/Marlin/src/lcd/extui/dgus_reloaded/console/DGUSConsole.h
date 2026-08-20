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
 * lcd/extui/dgus_reloaded/console/DGUSConsole.h
 *
 * Mirrors debug output, printer state changes and executed G-code onto the
 * DGUS panel's four message lines (MESSAGE_Line1..MESSAGE_Line4).
 *
 * Writes are coalesced rather than sent per event: a print can dispatch
 * hundreds of G-code commands per second, and each panel line costs ~40 bytes
 * of framing on a 115200 baud link, so pushing every command straight to the
 * display would saturate the UART and stall command processing. Events land in
 * a scrollback ring, and loop() repaints at most once per
 * DGUS_CONSOLE_FLUSH_MS, sending only rows whose text actually changed.
 */

#include "../../../../inc/MarlinConfigPre.h"

#if ENABLED(DGUS_CONSOLE)

#include "DGUSConsoleBuffer.h"
#include "DGUSConsoleFilter.h"
#include "../config/DGUS_Addr.h"

// Rows mirrored on the panel. The stock DGUS-Reloaded project defines four.
#ifndef DGUS_CONSOLE_ROWS
  #define DGUS_CONSOLE_ROWS 4
#endif

// Minimum interval between repaints, in ms.
#ifndef DGUS_CONSOLE_FLUSH_MS
  #define DGUS_CONSOLE_FLUSH_MS 120
#endif

class DGUSConsole {
public:
  // Reset the scrollback and blank the panel rows.
  static void init();

  // Append a line of text. Truncated to DGUS_LINE_LEN characters.
  static void log(const char * const text);
  static void log(FSTR_P const ftext);

  // Append a "key: value" line, e.g. state("Homing", "XYZ").
  static void logPair(FSTR_P const key, const char * const value);

  // Printer state / status transitions (also shown on the status line).
  static void logState(FSTR_P const state);

  // A G-code command that is about to be dispatched. Honors the motion filter.
  static void logGcode(const char * const command);

  // Repaint changed rows, rate-limited. Call from the ExtUI idle hook.
  static void loop() { flush(false); }

  // Repaint changed rows. `force` bypasses the rate limiter, for paths such as
  // kill() where no further idle cycle will run.
  static void flush(const bool force);

  // Runtime toggle, driven by M118-style control or the G-code hook.
  static bool enabled;

private:
  // Skip high-frequency motion commands so the window stays readable.
  static bool isNoisy(const char * const command);

  static DGUSConsoleBuffer<DGUS_CONSOLE_ROWS, DGUS_LINE_LEN> buffer;
  static char painted[DGUS_CONSOLE_ROWS][DGUS_LINE_LEN + 1];
  static uint32_t painted_rev;
  static uint32_t next_flush_ms;
};

extern DGUSConsole console;

#endif // DGUS_CONSOLE
