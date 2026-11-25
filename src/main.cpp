// SPDX-License-Identifier: MPL-2.0
/* Diagnostics: the fault codes of the car's main control units, read and
 * cleared through NBT Manager's diagnostics (capability `diag`), on the iDrive
 * screen and on the phone (capability `companion`). */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <GLES2/gl2.h>

#include "nbt/app/back_gesture.hpp"
#include "nbt/app/clock.hpp"
#include "nbt/app/lifecycle.hpp"
#include "nbt/app/log.hpp"
#include "nbt/app/video.hpp"
#include "nbt/controller/input.hpp"
#include "nbt/frontend/overlay.hpp"
#include "phone_page.hpp"
#include "scanner.hpp"
#include "screen.hpp"

namespace {

using diagnostics::Screen;
using diagnostics::UnitResult;

const uint32_t kFrameMs = 16;
/* The panel's size, for a health check: it has no window. */
const int kPanelWidth = 1280, kPanelHeight = 480;

void run(diagnostics::Scanner& scanner, Screen::Request request, unsigned unit) {
  if (request == Screen::kReadAll) scanner.read_all();
  if (request == Screen::kRead) scanner.read(unit);
  if (request == Screen::kClear) scanner.clear(unit);
}

/* Input acts at once, in the order it comes. */
struct Input {
  nbt::app::BackGesture back;
  uint64_t now;
  Screen* screen;
  diagnostics::Scanner* scanner;
  const UnitResult* results;

  void push() {
    unsigned unit = 0;
    if (scanner->available()) run(*scanner, screen->push(results, &unit), unit);
  }
};

void on_controller(void* context, const nbt_controller_wire_state& state,
                   const nbt::controller::Event& event) {
  Input* in = static_cast<Input*>(context);
  if (event.rotation) in->screen->turn(event.rotation);
  if (event.pressed & NBT_CONTROLLER_TILT_UP) in->screen->turn(-1);
  if (event.pressed & NBT_CONTROLLER_TILT_DOWN) in->screen->turn(1);
  if (event.pressed & NBT_CONTROLLER_PUSH) in->push();
  if (in->back.update((state.keys & NBT_CONTROLLER_BACK) != 0, in->now, false) ==
      nbt::app::BackGesture::kTap) {
    (void)in->screen->back();
  }
}

void apply_settings(const nbt::app::Lifecycle& lifecycle) {
  nbt::app::log_set_debug(!strcmp(lifecycle.settings().get("debug_log", "off"), "on"));
}

}  // namespace

int main(int argc, char** argv) {
  nbt::app::log_init("diagnostics");
  nbt::app::Lifecycle lifecycle;
  const int started = lifecycle.init(argc, argv);
  if (started != 0) return started;
  nbt::app::log_install_crash_handlers();
  apply_settings(lifecycle);

  // A health check (after an install or an update) has no screen, no input
  // and asks the car nothing.
  nbt::app::VideoEgl video;
  nbt::frontend::Overlay overlay;
  const bool user = lifecycle.is_user_mode();
  if (user) {
    const nbt::app::VideoConfig window = {"Diagnostics", kPanelWidth, kPanelHeight};
    if (!video.init(lifecycle.surface_id(), &window) || !overlay.init()) {
      nbt::app::log_info("no window: the car will not show it");
      return 1;
    }
  }
  const int width = user ? video.window_width() : kPanelWidth;
  const int height = user ? video.window_height() : kPanelHeight;
  diagnostics::Scanner scanner;
  diagnostics::PhonePage phone;
  if (user) {
    nbt::app::log_info("diagnostics %s, phone page %s", scanner.start() ? "on" : "not granted",
                       phone.init() ? "on" : "off");
  }
  nbt::app::log_info("ready, %dx%d, caps %s", width, height, lifecycle.bridge_caps());
  if (!lifecycle.send_ready()) return 1;

  nbt::controller::Input controller;
  Screen screen;
  UnitResult results[diagnostics::kUnitCount];
  Input input;
  input.now = 0;
  input.screen = &screen;
  input.scanner = &scanner;
  input.results = results;
  while (!lifecycle.stopping()) {
    const uint64_t now = nbt::app::monotonic_ms();
    input.now = now;
    (void)scanner.snapshot(results);
    if (lifecycle.controller_fd() >= 0) {
      controller.poll(lifecycle.controller_fd(), &on_controller, &input);
    }
    if (input.back.poll(now) == nbt::app::BackGesture::kLongHold) {
      nbt::app::log_info("BACK held: closing");
      lifecycle.request_stop();
    }
    nbt::app::HostEvent event;
    while (user && video.poll_event(&event)) {  // the desktop build's window
      if (event.type == nbt::app::HostEvent::kClose) lifecycle.request_stop();
      if (event.type != nbt::app::HostEvent::kKeyDown) continue;
      if (event.key == nbt::app::kHostKeyEscape) lifecycle.request_stop();
      if (event.key == nbt::app::kHostKeyUp) screen.turn(-1);
      if (event.key == nbt::app::kHostKeyDown) screen.turn(1);
      if (event.key == nbt::app::kHostKeyReturn) input.push();
      if (event.key == nbt::app::kHostKeyBackspace) (void)screen.back();
    }

    (void)scanner.snapshot(results);
    const bool busy = scanner.busy();
    unsigned unit = 0;
    switch (phone.next(&unit)) {
      case diagnostics::PhonePage::kReadAll:
        scanner.read_all();
        break;
      case diagnostics::PhonePage::kClear:
        scanner.clear(unit);
        break;
      case diagnostics::PhonePage::kNone:
        break;
    }
    phone.show(results, busy);  // written only when it changed

    char status[25];
    diagnostics::overall(results, busy, true, status, sizeof(status));  // at most 24
    bool changed = false;
    lifecycle.tick(now, status, &changed);
    if (changed) apply_settings(lifecycle);
    (void)lifecycle.consume_show_requested();

    if (user) {
      glViewport(0, 0, width, height);
      glClearColor(0.05f, 0.07f, 0.09f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      overlay.begin(width, height);
      screen.draw(overlay, width, height, results, scanner.available(), busy);
      overlay.end();
      video.swap();
    }
    const uint64_t spent = nbt::app::monotonic_ms() - now;
    usleep(static_cast<useconds_t>((spent < kFrameMs ? kFrameMs - spent : 1) * 1000));
  }

  scanner.stop();
  overlay.shutdown();
  video.shutdown();
  lifecycle.close_fds();
  return 0;
}
