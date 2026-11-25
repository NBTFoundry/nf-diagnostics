// SPDX-License-Identifier: MPL-2.0
#ifndef NF_DIAGNOSTICS_UNITS_HPP
#define NF_DIAGNOSTICS_UNITS_HPP

#include <stddef.h>
#include <stdint.h>

namespace diagnostics {

/* The control units it reads: their diagnostic address, short name and
 * what they do. release/package.args lists the same addresses. */
struct Unit {
  uint8_t address;
  const char* name;
  const char* role;
};
enum { kUnitCount = 8, kCodesMax = 48 };
extern const Unit kUnits[kUnitCount];

/* One fault code: its three bytes and its UDS status byte. */
struct Code {
  uint32_t dtc;
  uint8_t status;
};

/* What is known of one unit. */
struct UnitResult {
  enum State {
    kNotRead,
    kReading,
    kRead,      // codes[0..count) of total
    kNoAnswer,  // no answer in time: often a unit this car does not have
    kRefused,   // the unit answered with a negative code (nrc)
    kNoRoute,   // no gateway reached
    kBusy,      // another tester holds the gateway
    kClearing,
    kNotParked,  // a clear while the car moves
    kFailed,     // anything else (the manager went away, a malformed answer)
  };
  State state;
  uint8_t nrc;
  bool cleared;  // the last clear went through (the read after it shows what is left)
  size_t count, total;
  Code codes[kCodesMax];
};

/* The faults among the codes Diag::codes gives (4 bytes each: the code, its
 * status) whose status says test failed, pending or confirmed; at most
 * `max` kept, `*total` counts them all. */
size_t parse_codes(const uint8_t* data, size_t size, Code* out, size_t max, size_t* total);
/* "120308": the code as BMW's tools print it. */
void format_code(uint32_t dtc, char out[7]);
/* "active" (failing now), "pending" or "stored". */
const char* code_state(uint8_t status);
/* A short summary: "3 codes", "no codes", "not read", "reading...". */
void summary(const UnitResult& result, char* out, size_t size);
/* "DME (engine): 120308 active, 2A2A00 stored", cut to `size` with "...". */
void unit_line(const Unit& unit, const UnitResult& result, char* out, size_t size);

}  // namespace diagnostics

#endif
