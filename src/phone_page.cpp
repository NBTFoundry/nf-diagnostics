// SPDX-License-Identifier: MPL-2.0
#include "phone_page.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nbt/app/log.hpp"

namespace diagnostics {

PhonePage::PhonePage() : confirm_(-1) {}

bool PhonePage::init() {
  return page_.init();
}

void PhonePage::show(const UnitResult results[kUnitCount], bool busy) {
  if (!page_.available()) return;
  char line[NBT_COMPANION_TEXT_MAX + 1];
  overall(results, busy, false, line, sizeof(line));
  page_.begin();
  page_.title("Diagnostics");
  page_.text(line);
  page_.button("read", "Read all units");
  if (confirm_ >= 0) {
    const Unit& unit = kUnits[confirm_];
    snprintf(line, sizeof(line),
             "Clear every fault code of %s (%s)? Only while the car stands still.", unit.name,
             unit.role);
    page_.text(line);
    page_.button("yes", "Clear them");
    page_.button("no", "Keep them");
  }
  for (unsigned i = 0; i < kUnitCount; ++i) {
    unit_line(kUnits[i], results[i], line, sizeof(line));
    page_.text(line);
    if (confirm_ < 0 && results[i].state == UnitResult::kRead && results[i].total) {
      char id[8], label[32];
      snprintf(id, sizeof(id), "clear%u", i);
      snprintf(label, sizeof(label), "Clear %s codes", kUnits[i].name);
      page_.button(id, label);
    }
  }
  (void)page_.publish();
}

PhonePage::Action PhonePage::next(unsigned* unit) {
  nbt::app::Companion::Event event;
  while (page_.available() && page_.next(&event)) {
    nbt::app::log_info("phone: %s", event.id);
    if (!strcmp(event.id, "read")) return kReadAll;
    if (!strncmp(event.id, "clear", 5) && event.id[5] >= '0' && event.id[5] < '0' + kUnitCount &&
        !event.id[6]) {
      confirm_ = event.id[5] - '0';
    } else if (!strcmp(event.id, "yes") && confirm_ >= 0) {
      *unit = static_cast<unsigned>(confirm_);
      confirm_ = -1;
      return kClear;
    } else if (!strcmp(event.id, "no")) {
      confirm_ = -1;
    }
  }
  return kNone;
}

}  // namespace diagnostics
