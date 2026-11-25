// SPDX-License-Identifier: MPL-2.0
#include "screen.hpp"

#include <stdio.h>

namespace diagnostics {

namespace {

using nbt::frontend::Overlay;

const uint32_t kText = 0xffffffffu, kMuted = 0x9aa4afffu, kFocus = 0x1d4f91ffu,
               kActive = 0xff6b6bffu, kStored = 0xf0b429ffu;
const int kMargin = 40, kTop = 84, kRow = 42, kScale = 3, kTitleScale = 4;

void row(Overlay& o, int width, int index, bool focused, const char* left, const char* middle,
         const char* right) {
  const int y = kTop + index * kRow;
  if (focused) o.box(kMargin / 2, y, width - kMargin, kRow - 4, kFocus);
  const int text_y = y + (kRow - 4 - Overlay::kGlyph * kScale) / 2;
  o.text(kMargin, text_y, kScale, left, kText);
  if (middle) o.text(kMargin + 220, text_y, kScale, middle, kMuted);
  if (right) {
    o.text(width - kMargin - Overlay::text_width(right, kScale), text_y, kScale, right,
           focused ? kText : kMuted);
  }
}

void title(Overlay& o, int width, const char* left, const char* right) {
  o.text(kMargin, 28, kTitleScale, left, kText);
  if (right)
    o.text(width - kMargin - Overlay::text_width(right, kScale), 36, kScale, right, kMuted);
}

}  // namespace

Screen::Screen() : page_(kList), focus_(0), unit_(0) {}

int Screen::items() const {
  return page_ == kList ? 1 + kUnitCount : 2;
}

void Screen::turn(int detents) {
  focus_ += detents;
  if (focus_ < 0) focus_ = 0;
  if (focus_ >= items()) focus_ = items() - 1;
}

Screen::Request Screen::push(const UnitResult results[kUnitCount], unsigned* unit) {
  switch (page_) {
    case kList:
      if (focus_ == 0) return kReadAll;
      unit_ = static_cast<unsigned>(focus_ - 1);
      page_ = kUnit;
      focus_ = 0;
      *unit = unit_;
      // Opening a unit not read yet reads it.
      return results[unit_].state == UnitResult::kNotRead ? kRead : kNothing;
    case kUnit:
      *unit = unit_;
      if (focus_ == 0) return kRead;
      page_ = kConfirm;
      focus_ = 0;  // "No" first
      return kNothing;
    case kConfirm:
      page_ = kUnit;
      *unit = unit_;
      if (focus_ == 1) {
        focus_ = 0;
        return kClear;
      }
      focus_ = 1;
      return kNothing;
  }
  return kNothing;
}

bool Screen::back() {
  if (page_ == kList) return false;
  if (page_ == kConfirm) {
    page_ = kUnit;
    focus_ = 1;
  } else {
    page_ = kList;
    focus_ = 1 + static_cast<int>(unit_);
  }
  return true;
}

void Screen::draw(Overlay& o, int width, int height, const UnitResult results[kUnitCount],
                  bool available, bool busy) const {
  if (!available) {
    title(o, width, "Diagnostics", NULL);
    o.text(kMargin, kTop + kRow, kScale, "NBT Manager gives this application no diagnostics.",
           kText);
    o.text(kMargin, kTop + 2 * kRow, kScale, "Update NBT Manager, then start it again.", kMuted);
    return;
  }
  switch (page_) {
    case kList:
      title(o, width, "Diagnostics", busy ? "reading..." : NULL);
      draw_list(o, width, results);
      return;
    case kUnit:
      draw_unit(o, width, height, results[unit_]);
      return;
    case kConfirm:
      draw_confirm(o, width);
      return;
  }
}

void Screen::draw_list(Overlay& o, int width, const UnitResult* results) const {
  row(o, width, 0, focus_ == 0, "Read all units", NULL, NULL);
  for (unsigned i = 0; i < kUnitCount; ++i) {
    char text[32];
    summary(results[i], text, sizeof(text));
    row(o, width, static_cast<int>(1 + i), focus_ == static_cast<int>(1 + i), kUnits[i].name,
        kUnits[i].role, text);
  }
}

void Screen::draw_unit(Overlay& o, int width, int height, const UnitResult& result) const {
  char heading[48], text[32];
  snprintf(heading, sizeof(heading), "%s - %s", kUnits[unit_].name, kUnits[unit_].role);
  summary(result, text, sizeof(text));
  title(o, width, heading, text);
  row(o, width, 0, focus_ == 0, "Read again", NULL, NULL);
  row(o, width, 1, focus_ == 1, "Clear codes", NULL, NULL);
  if (result.state != UnitResult::kRead) return;
  const int line = Overlay::kGlyph * kScale + 12, first = kTop + 2 * kRow + 12;
  const int fit = (height - first) / line;
  for (size_t i = 0; i < result.count && static_cast<int>(i) < fit; ++i) {
    const int y = first + static_cast<int>(i) * line;
    if (static_cast<int>(i) == fit - 1 && result.total > i + 1) {
      snprintf(text, sizeof(text), "and %u more", static_cast<unsigned>(result.total - i));
      o.text(kMargin, y, kScale, text, kMuted);
      break;
    }
    char code[7];
    format_code(result.codes[i].dtc, code);
    const bool active = result.codes[i].status & 0x01;
    o.text(kMargin, y, kScale, code, kText);
    o.text(kMargin + 220, y, kScale, code_state(result.codes[i].status),
           active ? kActive : kStored);
  }
}

void Screen::draw_confirm(Overlay& o, int width) const {
  char text[64];
  title(o, width, "Clear fault codes", NULL);
  snprintf(text, sizeof(text), "Clear every fault code of %s (%s)?", kUnits[unit_].name,
           kUnits[unit_].role);
  o.text(kMargin, kTop + 2 * kRow + 20, kScale, text, kText);
  o.text(kMargin, kTop + 3 * kRow + 20, kScale, "Only while the car stands still.", kMuted);
  row(o, width, 0, focus_ == 0, "No", NULL, NULL);
  row(o, width, 1, focus_ == 1, "Yes, clear them", NULL, NULL);
}

}  // namespace diagnostics
