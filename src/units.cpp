// SPDX-License-Identifier: MPL-2.0
#include "units.hpp"

#include <stdio.h>
#include <string.h>

namespace diagnostics {

/* Their usual addresses on F-series cars, behind the gateway; a car without
 * one gives no answer there. */
const Unit kUnits[kUnitCount] = {
    {0x12, "DME", "engine"},        {0x13, "DME2", "second engine"}, {0x18, "EGS", "gearbox"},
    {0x19, "VTG", "transfer case"}, {0x29, "DSC", "stability"},      {0x01, "ACSM", "airbags"},
    {0x30, "EPS", "steering"},      {0x40, "BDC", "body"},
};

namespace {

// UDS status bits (ISO 14229-1): test failed, pending, confirmed.
const uint8_t kTestFailed = 0x01, kPending = 0x04, kConfirmed = 0x08;

void append(char* out, size_t size, const char* text) {
  const size_t used = strlen(out);
  if (used + 1 < size) snprintf(out + used, size - used, "%s", text);
}

}  // namespace

size_t parse_codes(const uint8_t* data, size_t size, Code* out, size_t max, size_t* total) {
  size_t kept = 0, all = 0;
  // 4 bytes per code (Diag::codes leaves out 59 02 and the availability mask).
  for (size_t at = 0; data && at + 4 <= size; at += 4) {
    const uint8_t status = data[at + 3];
    if (!(status & (kTestFailed | kPending | kConfirmed))) continue;
    ++all;
    if (out && kept < max) {
      out[kept].dtc = static_cast<uint32_t>(data[at]) << 16 |
                      static_cast<uint32_t>(data[at + 1]) << 8 | data[at + 2];
      out[kept].status = status;
      ++kept;
    }
  }
  if (total) *total = all;
  return kept;
}

void format_code(uint32_t dtc, char out[7]) {
  snprintf(out, 7, "%06X", static_cast<unsigned>(dtc & 0xFFFFFFu));
}

const char* code_state(uint8_t status) {
  return status & kTestFailed ? "active" : status & kConfirmed ? "stored" : "pending";
}

void summary(const UnitResult& result, char* out, size_t size) {
  switch (result.state) {
    case UnitResult::kNotRead:
      snprintf(out, size, "not read");
      return;
    case UnitResult::kReading:
      snprintf(out, size, "reading...");
      return;
    case UnitResult::kClearing:
      snprintf(out, size, "clearing...");
      return;
    case UnitResult::kNoAnswer:
      snprintf(out, size, "no answer");
      return;
    case UnitResult::kRefused:
      snprintf(out, size, "refused (%02X)", result.nrc);
      return;
    case UnitResult::kNoRoute:
      snprintf(out, size, "no gateway");
      return;
    case UnitResult::kBusy:
      snprintf(out, size, "gateway busy");
      return;
    case UnitResult::kNotParked:
      snprintf(out, size, "stop the car to clear");
      return;
    case UnitResult::kFailed:
      snprintf(out, size, "failed");
      return;
    case UnitResult::kRead:
      break;
  }
  if (!result.total) {
    snprintf(out, size, result.cleared ? "cleared, no codes" : "no codes");
  } else {
    snprintf(out, size, "%u code%s", static_cast<unsigned>(result.total),
             result.total == 1 ? "" : "s");
  }
}

void overall(const UnitResult* results, bool busy, bool brief, char* out, size_t size) {
  unsigned codes = 0, units = 0, read = 0, failed = 0, unrouted = 0;
  for (unsigned i = 0; i < kUnitCount; ++i) {
    const UnitResult::State state = results[i].state;
    if (state == UnitResult::kRead) {
      ++read;
      codes += static_cast<unsigned>(results[i].total);
      units += results[i].total ? 1u : 0u;
    } else if (state != UnitResult::kNotRead && state != UnitResult::kReading &&
               state != UnitResult::kClearing) {
      ++failed;
      unrouted += state == UnitResult::kNoRoute ? 1u : 0u;
    }
  }
  if (busy) {
    snprintf(out, size, brief ? "Reading..." : "Reading the car...");
  } else if (codes) {
    if (brief) {
      snprintf(out, size, "%u fault code%s", codes, codes == 1 ? "" : "s");
    } else {
      snprintf(out, size, "%u fault code%s in %u unit%s.", codes, codes == 1 ? "" : "s", units,
               units == 1 ? "" : "s");
    }
  } else if (read) {
    snprintf(out, size, brief ? "No fault codes" : "No fault codes.");
  } else if (failed && unrouted == failed) {
    snprintf(out, size, brief ? "No gateway" : "No gateway reached.");
  } else if (failed) {
    snprintf(out, size, brief ? "No answer" : "No unit answered.");
  } else {
    snprintf(out, size, brief ? "Not read" : "Not read yet.");
  }
}

void unit_line(const Unit& unit, const UnitResult& result, char* out, size_t size) {
  char text[32];
  snprintf(out, size, "%s (%s): ", unit.name, unit.role);
  if (result.state != UnitResult::kRead || !result.total) {
    summary(result, text, sizeof(text));
    append(out, size, text);
    return;
  }
  for (size_t i = 0; i < result.count; ++i) {
    char code[7], item[24];
    format_code(result.codes[i].dtc, code);
    snprintf(item, sizeof(item), "%s%s %s", i ? ", " : "", code,
             code_state(result.codes[i].status));
    // Room for this one and a "..." after it.
    if (strlen(out) + strlen(item) + 4 >= size) {
      append(out, size, "...");
      return;
    }
    append(out, size, item);
  }
  if (result.total > result.count) append(out, size, "...");
}

}  // namespace diagnostics
