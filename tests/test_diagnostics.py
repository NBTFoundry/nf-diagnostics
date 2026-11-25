# SPDX-License-Identifier: MPL-2.0
"""The desktop build as NBT Manager starts it: the health check without a
window, a user launch under Xvfb with the iDrive controller's socket, the
fault codes through a stand-in for NBT Manager's diagnostics and the phone
page; the signed package's declaration; the notices being current."""

import os
from pathlib import Path
import re
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP = Path(os.environ.get("NBT_DIAGNOSTICS_APP", ROOT / "build/nbt-diagnostics-host"))
MGR_DIR = Path(os.environ.get("NBT_MGR_DIR", ROOT.parent / "manager"))
SDK_DIR = Path(os.environ.get("NBT_SDK_DIR", ROOT.parent / "sdk"))
FORM = os.environ.get("NBT_APP_FORM", "native")  # the Makefile's FORM

PUSH, BACK = 1 << 0, 1 << 2  # the SDK's nbt/controller_wire.h


def controller_state(sequence, keys=0, rotation=0):
    return struct.pack("<BBBBIIi", 0x43, 1, 1, 0, sequence, keys, rotation)


def xvfb():
    r, w = os.pipe()
    proc = subprocess.Popen(
        ["Xvfb", "-displayfd", str(w), "-screen", "0", "1280x480x24", "-nolisten", "tcp"],
        pass_fds=(w,), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(w)
    number = b""
    while not number.endswith(b"\n"):
        ready, _, _ = select.select([r], [], [], 10)
        chunk = os.read(r, 1) if ready else b""
        if not chunk:
            break
        number += chunk
    os.close(r)
    return proc, ":" + number.decode().strip()


class App:
    """The application, its log (standard error) read by line."""

    def __init__(self, env, args=(), pass_fds=(), fixed=None):
        full = {k: v for k, v in os.environ.items() if not k.startswith("NBT_")}
        full.update(env)
        command = [str(APP), *args]
        if fixed:  # {descriptor number the manager gives: our descriptor}
            moves = " ".join(f"{n}<&{fd} {fd}<&-" for n, fd in fixed.items())
            command = ["bash", "-c", f'exec "$0" "$@" {moves}', *command]
            pass_fds = (*pass_fds, *fixed.values())
        self.proc = subprocess.Popen(command, stdout=subprocess.DEVNULL,
                                     stderr=subprocess.PIPE, env=full, pass_fds=pass_fds)
        self.lines, self.seen, self.buf = [], [], b""

    def wait(self, pattern, timeout=20.0):
        regex, deadline = re.compile(pattern), time.monotonic() + timeout
        while True:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                line = raw.decode(errors="replace")
                self.lines.append(line)
                self.seen.append(line)
            for i, line in enumerate(self.lines):
                if regex.search(line):
                    del self.lines[: i + 1]
                    return line
            fd = self.proc.stderr.fileno()
            left = deadline - time.monotonic()
            ready, _, _ = select.select([fd], [], [], max(0.0, left)) if left > 0 else ([], [], [])
            chunk = os.read(fd, 4096) if ready else b""
            if not chunk:
                raise AssertionError(f"no line matching {pattern!r}; saw {self.seen[-30:]!r}")
            self.buf += chunk

    def stop(self, sig=signal.SIGTERM):
        """Sends `sig` (None: only waits for the end) and returns the status."""
        if sig is not None and self.proc.poll() is None:
            self.proc.send_signal(sig)
        _, err = self.proc.communicate(timeout=20)
        self.buf += err
        return self.proc.returncode


@unittest.skipUnless(os.access(APP, os.X_OK), f"{APP} not built")
class CheckLaunchTest(unittest.TestCase):
    def test_reports_ready_without_a_window(self):
        """As NBT Manager runs it after an install or an update: no display,
        ready on --ready-fd, heartbeats, then SIGTERM ends it with status 0."""
        with tempfile.TemporaryDirectory() as data:
            lease_fd = os.open(Path(data) / "lease", os.O_CREAT | os.O_RDWR, 0o600)
            rfd, wfd = os.pipe()
            env = {"NBT_LAUNCH": "check", "NBT_DATA_DIR": data}
            app = App(env, ["--ready-fd", str(wfd), "--lease-fd", str(lease_fd)], (wfd, lease_fd))
            os.close(wfd)
            try:
                app.wait(r"ready, 1280x480")
                for _ in range(2):  # ready, then a heartbeat
                    readable, _, _ = select.select([rfd], [], [], 5.0)
                    self.assertEqual(os.read(rfd, 1) if readable else b"", b"R")
                self.assertEqual(app.stop(), 0, app.seen[-20:])
            finally:
                if app.proc.poll() is None:
                    app.proc.kill()
                os.close(rfd)
                os.close(lease_fd)


@unittest.skipUnless(os.access(APP, os.X_OK), f"{APP} not built")
@unittest.skipUnless(shutil.which("Xvfb"), "Xvfb not installed")
class UserLaunchTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="nbt-diagnostics-"))
        self.x, self.display = xvfb()
        self.controller_app, self.controller = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)

    def tearDown(self):
        self.controller_app.close()
        self.controller.close()
        self.x.terminate()
        self.x.wait(timeout=5)
        shutil.rmtree(self.tmp, ignore_errors=True)

    def start(self, **settings):
        env = {
            "NBT_LAUNCH": "user", "DISPLAY": self.display, "LIBGL_ALWAYS_SOFTWARE": "1",
            "NBT_NO_AUDIO": "1", "NBT_DATA_DIR": str(self.tmp),
            "NBT_BRIDGE_CAPS": "show,controller",
            "NBT_CONTROLLER_FD": str(self.controller_app.fileno()),
        }
        env.update({"NBT_SETTING_" + k.upper(): v for k, v in settings.items()})
        app = App(env, pass_fds=(self.controller_app.fileno(),))
        self.addCleanup(lambda: app.proc.poll() is None and app.proc.kill())
        app.wait(r"ready, 1280x480, caps show,controller")
        return app

    def test_it_runs_and_stops_when_asked(self):
        app = self.start(debug_log="on")
        time.sleep(0.5)
        self.assertIsNone(app.proc.poll())
        self.assertEqual(app.stop(), 0, app.seen[-20:])

    def test_a_long_back_hold_quits(self):
        app = self.start()
        self.controller.send(controller_state(1))  # keys count once all were seen up
        self.controller.send(controller_state(2, BACK))
        app.wait(r"BACK held: closing", timeout=3)
        self.assertEqual(app.stop(None), 0, app.seen[-20:])


class FakeDiagnostics(threading.Thread):
    """NBT Manager's side of `diag` (its include/nbt/diag_wire.h): fault codes
    per control unit, no answer from the others, clears refused while
    `moving`."""

    def __init__(self, sock, codes):
        super().__init__(daemon=True)
        self.sock, self.codes, self.moving, self.asked = sock, codes, False, []
        self.route = True  # False: no gateway reached

    def run(self):
        while True:
            try:
                data = self.sock.recv(8192)
            except OSError:
                return
            if len(data) < 7 or data[0] != 0x01:
                return
            ecu, rid = data[1], struct.unpack_from("<H", data, 2)[0]
            uds = data[6:]
            self.asked.append((ecu, uds))
            status, answer = 1, b""  # no answer in time
            if not self.route:
                status = 3
            elif ecu in self.codes and uds == b"\x19\x02\xff":
                status = 0
                answer = b"\x59\x02\xff" + b"".join(
                    struct.pack(">I", dtc)[1:] + bytes([st]) for dtc, st in self.codes[ecu])
            elif ecu in self.codes and uds == b"\x14\xff\xff\xff":
                status = 4 if self.moving else 0
                if not self.moving:
                    self.codes[ecu] = []
                    answer = b"\x54"
            self.sock.send(struct.pack("<BBHBB", 0x81, ecu, rid, status, 1) + answer)


@unittest.skipUnless(os.access(APP, os.X_OK), f"{APP} not built")
@unittest.skipUnless(shutil.which("Xvfb"), "Xvfb not installed")
class FaultCodesTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="nbt-diagnostics-"))
        self.x, self.display = xvfb()
        self.diag_app, diag = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        # DME: two faults and a code with no fault bit; gearbox: none; DSC: one stored.
        self.car = FakeDiagnostics(diag, {0x12: [(0x120308, 0x09), (0x2A2A00, 0x08), (0x123456, 0x50)],
                                          0x18: [], 0x29: [(0xD35A20, 0x24)]})
        self.car.start()
        self.page_fd = os.open(self.tmp / "page", os.O_RDWR | os.O_CREAT, 0o600)
        self.events = self.tmp / "events"
        self.sequence = 0
        self.lines = []

    def tearDown(self):
        self.diag_app.close()
        self.car.sock.close()
        os.close(self.page_fd)
        self.x.terminate()
        self.x.wait(timeout=5)
        shutil.rmtree(self.tmp, ignore_errors=True)

    def page(self):
        return (self.tmp / "page").read_text()

    def tap(self, widget, value=""):
        """The phone's tap, as NBT Manager's worker writes it."""
        self.sequence += 1
        self.lines = (self.lines + [f"{self.sequence}\t{widget}\t{value}\n"])[-16:]
        (self.tmp / "events.new").write_text("".join(self.lines))
        os.rename(self.tmp / "events.new", self.events)

    def wait_page(self, text, timeout=10.0):
        deadline = time.monotonic() + timeout
        while text not in self.page():
            if time.monotonic() > deadline:
                raise AssertionError(f"{text!r} not on the page:\n{self.page()}")
            time.sleep(0.05)

    def launch(self, caps, fixed):
        env = {
            "NBT_LAUNCH": "user", "DISPLAY": self.display, "LIBGL_ALWAYS_SOFTWARE": "1",
            "NBT_NO_AUDIO": "1", "NBT_DATA_DIR": str(self.tmp), "NBT_BRIDGE_CAPS": caps,
            "NBT_DIAG_FD": "10", "NBT_COMPANION_FD": "11", "NBT_COMPANION_EVENTS": str(self.events),
        }
        env.update({"NBT_CONTROLLER_FD": str(fixed.pop("controller"))} if "controller" in fixed else {})
        controller = (int(env["NBT_CONTROLLER_FD"]),) if "NBT_CONTROLLER_FD" in env else ()
        app = App(env, pass_fds=controller, fixed=fixed)
        self.addCleanup(lambda: app.proc.poll() is None and app.proc.kill())
        return app

    def test_the_controller_reads_a_unit_and_reads_all(self):
        mine, theirs = socket.socketpair(socket.AF_UNIX, socket.SOCK_DGRAM)
        self.addCleanup(mine.close)
        self.addCleanup(theirs.close)
        app = self.launch("show,controller,diag", {"controller": theirs.fileno(), 10: self.diag_app.fileno()})
        app.wait(r"diagnostics on, phone page off")
        mine.send(controller_state(1))  # keys count once all were seen up
        # The rotation counts detents since the feed started.
        mine.send(controller_state(2, rotation=1))  # to DME
        mine.send(controller_state(3, PUSH, 1))  # opens it: not read yet, so it is read
        app.wait(r"DME: read: state 2, 2 codes")
        mine.send(controller_state(4, 0, 1))
        mine.send(controller_state(5, BACK, 1))
        mine.send(controller_state(6, 0, 1))  # a tap: back to the list, on DME
        mine.send(controller_state(7, 0, 0))  # up to Read all
        mine.send(controller_state(8, PUSH, 0))
        app.wait(r"BDC: read: state 3, 0 codes")
        self.assertEqual(len(self.car.asked), 1 + 8)
        self.assertEqual(app.stop(), 0, app.seen[-20:])

    def test_no_gateway_is_said_once_read(self):
        self.car.route = False
        app = self.launch("show,diag,companion", {10: self.diag_app.fileno(), 11: self.page_fd})
        app.wait(r"diagnostics on, phone page")
        self.tap("read")
        self.wait_page("No gateway reached.")
        self.assertIn("DME (engine): no gateway\n", self.page())

    def test_codes_read_and_cleared_from_the_phone(self):
        env = {
            "NBT_LAUNCH": "user", "DISPLAY": self.display, "LIBGL_ALWAYS_SOFTWARE": "1",
            "NBT_NO_AUDIO": "1", "NBT_DATA_DIR": str(self.tmp),
            "NBT_BRIDGE_CAPS": "show,controller,diag,companion",
            "NBT_DIAG_FD": "10", "NBT_COMPANION_FD": "11", "NBT_COMPANION_EVENTS": str(self.events),
        }
        app = App(env, fixed={10: self.diag_app.fileno(), 11: self.page_fd})
        self.addCleanup(lambda: app.proc.poll() is None and app.proc.kill())
        app.wait(r"diagnostics on, phone page on")
        self.wait_page("Not read yet.")
        self.assertIn("button\tread\tRead all units\n", self.page())
        self.assertEqual(self.car.asked, [])  # nothing asked before a tap

        self.tap("read")
        self.wait_page("DME (engine): 120308 active, 2A2A00 stored\n")
        self.wait_page("DSC (stability): D35A20 pending\n")
        self.wait_page("3 fault codes in 2 units.")
        page = self.page()
        self.assertIn("EGS (gearbox): no codes\n", page)
        self.assertIn("ACSM (airbags): no answer\n", page)
        self.assertIn("button\tclear0\tClear DME codes\n", page)
        self.assertNotIn("clear2", page)  # nothing to clear on the gearbox
        allowed = {0x12, 0x13, 0x18, 0x19, 0x29, 0x01, 0x30, 0x40}
        self.assertEqual({ecu for ecu, _ in self.car.asked}, allowed)

        # A clear asks again; while the car moves the manager refuses it.
        self.tap("clear0")
        self.wait_page("Clear every fault code of DME (engine)?")
        self.assertIn("button\tyes\tClear them\n", self.page())
        self.car.moving = True
        self.tap("yes")
        self.wait_page("DME (engine): stop the car to clear\n")
        self.car.moving = False
        self.tap("clear0")
        self.tap("yes")
        self.wait_page("DME (engine): cleared, no codes\n")
        self.wait_page("1 fault code in 1 unit.")
        clears = [uds for ecu, uds in self.car.asked if uds[:1] == b"\x14"]
        self.assertEqual(len(clears), 2)
        self.assertEqual(app.stop(), 0, app.seen[-20:])


class PackageTest(unittest.TestCase):
    def test_the_package_declares_its_bridge_and_settings(self):
        """The native form, signed with a throw-away key: bridge 1.9, the
        diagnostics policy and the phone page."""
        verify = MGR_DIR / "build" / "nbt-verify"
        if not (MGR_DIR / "tools/package.py").exists() or not verify.exists():
            self.skipTest("the manager's tools/package.py or build/nbt-verify is missing")
        if not shutil.which("openssl"):
            self.skipTest("openssl is missing")
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            key, public = tmp / "app.key", tmp / "app.pub"
            subprocess.run(["openssl", "genrsa", "-out", key, "3072"], check=True, capture_output=True)
            subprocess.run(["openssl", "rsa", "-in", key, "-pubout", "-out", public], check=True,
                           capture_output=True)
            # A stand-in for the payload: an ARM ELF header; with the runner it
            # carries the SDK's NBT ABI note, as the packer wants with `runner`.
            elf = bytearray(52)
            elf[:7] = b"\x7fELF\x01\x01\x01"
            if FORM == "native":
                elf[16:20] = b"\x02\x00\x28\x00"
                binary, payload = tmp / "stand-in", bytes(elf)
            else:
                note = struct.pack("<III", 4, 12, 1) + b"NBT\0" + struct.pack("<III", 1, 1, 1)
                struct.pack_into("<HHI", elf, 16, 3, 40, 1)
                struct.pack_into("<I", elf, 28, 52)
                struct.pack_into("<HHH", elf, 40, 52, 32, 1)
                phdr = struct.pack("<IIIIIIII", 4, 84, 0, 0, len(note), len(note), 4, 4)
                binary, payload = tmp / "stand-in.so", bytes(elf) + phdr + note
            binary.write_bytes(payload)
            subprocess.run(["make", "-s", "-C", str(ROOT), "package", f"FORM={FORM}", f"BUILD_DIR={tmp}",
                            f"PACKAGE_INPUT={binary}", f"APP_KEY={key}", f"NBT_MGR_DIR={MGR_DIR}",
                            f"NBT_SDK_DIR={SDK_DIR}", "APP_VERSION=0.1.0", "APP_SEQ=1000"],
                           check=True, capture_output=True)
            out = subprocess.run([str(verify), str(tmp / "diagnostics-0.1.0.nbt"), str(public), "diagnostics",
                                  "nbt_6.5_armv7"], check=True, capture_output=True, text=True).stdout
            self.assertIn("app=diagnostics\n", out)
            self.assertIn("bridge=1.9\n", out)
            self.assertIn("bridge-uses=show,controller\n", out)
            self.assertIn("bridge-optional=status,live-settings,diag,companion\n", out)
            self.assertIn("bridge-diag=01:codes+clear,12:codes+clear,13:codes+clear,18:codes+clear,"
                          "19:codes+clear,29:codes+clear,30:codes+clear,40:codes+clear\n", out)


class NoticeTest(unittest.TestCase):
    def test_the_notices_are_current(self):
        if not (SDK_DIR / "tools/third_party_notices.py").exists():
            self.skipTest("no SDK checkout for its notices")
        check = subprocess.run([sys.executable, str(ROOT / "tools/make_notice.py"), "--check"],
                               env=dict(os.environ, NBT_SDK_DIR=str(SDK_DIR)),
                               capture_output=True, text=True)
        if check.returncode and "no picolibc notices" in check.stderr:
            self.skipTest("no NBT toolchain for its notices")
        self.assertEqual(check.returncode, 0, check.stderr)


if __name__ == "__main__":
    unittest.main()
