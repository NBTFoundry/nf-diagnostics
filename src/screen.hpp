// SPDX-License-Identifier: MPL-2.0
#ifndef NF_DIAGNOSTICS_SCREEN_HPP
#define NF_DIAGNOSTICS_SCREEN_HPP

#include "nbt/frontend/overlay.hpp"
#include "units.hpp"

namespace diagnostics {

/* The iDrive screen: the list (Read all, then each unit), a unit's codes,
 * and the question before a clear. The knob moves, a push chooses, BACK goes
 * up a page. */
class Screen {
 public:
  enum Request { kNothing, kReadAll, kRead, kClear };
  Screen();
  void turn(int detents);
  /* What a push asks for; `*unit` for kRead and kClear. */
  Request push(const UnitResult results[kUnitCount], unsigned* unit);
  /* False on the list, where BACK has nothing to close. */
  bool back();
  /* `available`: diagnostics granted; `busy`: a job runs. */
  void draw(nbt::frontend::Overlay& overlay, int width, int height,
            const UnitResult results[kUnitCount], bool available, bool busy) const;

 private:
  enum Page { kList, kUnit, kConfirm };
  int items() const;
  void draw_list(nbt::frontend::Overlay& o, int width, const UnitResult* results) const;
  void draw_unit(nbt::frontend::Overlay& o, int width, int height, const UnitResult& result) const;
  void draw_confirm(nbt::frontend::Overlay& o, int width) const;

  Page page_;
  int focus_;
  unsigned unit_;
};

}  // namespace diagnostics

#endif
