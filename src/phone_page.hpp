// SPDX-License-Identifier: MPL-2.0
#ifndef NF_DIAGNOSTICS_PHONE_PAGE_HPP
#define NF_DIAGNOSTICS_PHONE_PAGE_HPP

#include "nbt/app/companion.hpp"
#include "units.hpp"

namespace diagnostics {

/* The page in the phone app (capability `companion`): each unit's codes, Read
 * all, and Clear, which asks again before it clears. */
class PhonePage {
 public:
  enum Action { kNone, kReadAll, kClear };
  PhonePage();
  bool init();
  bool available() const { return page_.available(); }
  void show(const UnitResult results[kUnitCount], bool busy);
  /* The phone's next request; `*unit` for kClear. */
  Action next(unsigned* unit);

 private:
  nbt::app::Companion page_;
  int confirm_;  // the unit whose clear waits for its second tap, or -1
};

}  // namespace diagnostics

#endif
