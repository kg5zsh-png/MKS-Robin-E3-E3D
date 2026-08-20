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

/**
 * lcd/extui/dgus_reloaded/console/DGUSConsole.cpp
 */

#include "../../../../inc/MarlinConfigPre.h"

#if ENABLED(DGUS_CONSOLE)

#include "DGUSConsole.h"
#include "../DGUSDisplay.h"
#include "../DGUSScreenHandler.h"
#include "../../ui_api.h"

DGUSConsole console;

bool DGUSConsole::enabled = true;

DGUSConsoleBuffer<DGUS_CONSOLE_ROWS, DGUS_LINE_LEN> DGUSConsole::buffer;
char DGUSConsole::painted[DGUS_CONSOLE_ROWS][DGUS_LINE_LEN + 1];
uint32_t DGUSConsole::painted_rev = 0;
uint32_t DGUSConsole::next_flush_ms = 0;

// Panel message rows are contiguous, 0x20 words apart.
static constexpr uint16_t message_line_addr[4] = {
  (uint16_t)DGUS_Addr::MESSAGE_Line1,
  (uint16_t)DGUS_Addr::MESSAGE_Line2,
  (uint16_t)DGUS_Addr::MESSAGE_Line3,
  (uint16_t)DGUS_Addr::MESSAGE_Line4
};

void DGUSConsole::init() {
  buffer.clear();
  for (uint8_t r = 0; r < DGUS_CONSOLE_ROWS; ++r) painted[r][0] = '\0';
  painted_rev = 0;
  next_flush_ms = 0;
}

void DGUSConsole::log(const char * const text) {
  if (!enabled) return;
  buffer.push(text);
}

void DGUSConsole::log(FSTR_P const ftext) {
  if (!enabled) return;
  char tmp[DGUS_LINE_LEN + 1];
  strncpy_P(tmp, FTOP(ftext), DGUS_LINE_LEN);
  tmp[DGUS_LINE_LEN] = '\0';
  buffer.push(tmp);
}

void DGUSConsole::logPair(FSTR_P const key, const char * const value) {
  if (!enabled) return;

  char tmp[DGUS_LINE_LEN + 1];
  strncpy_P(tmp, FTOP(key), DGUS_LINE_LEN);
  tmp[DGUS_LINE_LEN] = '\0';

  uint8_t len = strlen(tmp);
  if (len < DGUS_LINE_LEN) { tmp[len++] = ':'; tmp[len] = '\0'; }
  if (len < DGUS_LINE_LEN) { tmp[len++] = ' '; tmp[len] = '\0'; }

  if (value)
    for (uint8_t i = 0; len < DGUS_LINE_LEN && value[i]; ++i, ++len) tmp[len] = value[i];
  tmp[len] = '\0';

  buffer.push(tmp);
}

void DGUSConsole::logState(FSTR_P const state) {
  if (!enabled) return;
  log(state);
  // Also surface it on the dedicated status field so it survives scrollback.
  screen.setStatusMessage(state);
}

/**
 * Motion and temperature-poll commands dominate a running print and would push
 * everything else off the window within a fraction of a second. Filter the
 * worst offenders unless the user asked for a full trace.
 */
bool DGUSConsole::isNoisy(const char * const command) {
  #if ENABLED(DGUS_CONSOLE_GCODE_ALL)
    return !command;
  #else
    return dgusConsoleIsNoisyGcode(command);
  #endif
}

void DGUSConsole::logGcode(const char * const command) {
  if (!enabled || !command) return;
  if (isNoisy(command)) return;
  buffer.push(command);
}

void DGUSConsole::flush(const bool force) {
  if (!enabled) return;

  // Nothing new since the last repaint.
  if (buffer.rev() == painted_rev) return;

  const uint32_t now = ExtUI::safe_millis();
  if (!force && next_flush_ms && PENDING(now, next_flush_ms)) return;
  next_flush_ms = now + (DGUS_CONSOLE_FLUSH_MS);

  for (uint8_t r = 0; r < DGUS_CONSOLE_ROWS && r < COUNT(message_line_addr); ++r) {
    const char * const text = buffer.line(r);
    if (strncmp(text, painted[r], DGUS_LINE_LEN) == 0) continue;  // unchanged row

    strncpy(painted[r], text, DGUS_LINE_LEN);
    painted[r][DGUS_LINE_LEN] = '\0';
    dgus.writeString(message_line_addr[r], text, DGUS_LINE_LEN, true, true);
  }

  painted_rev = buffer.rev();
}

#endif // DGUS_CONSOLE
