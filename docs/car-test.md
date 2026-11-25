<!-- SPDX-License-Identifier: MPL-2.0 -->
# Car test: Diagnostics

Packages: `diagnostics-0.0.1.nbt` (install), `diagnostics-0.0.2.nbt`
(update), `diagnostics-0.0.1-SHA256SUMS.txt`; the NBT Manager test kit
dev142 or later (bridge 1.9: fault codes, clearing, the phone page; an
older manager disables it). Parked, ignition on, about 20 minutes, phone
only, nothing in the OBD port.

Main questions: which units answer through NBT Manager's diagnostics, and
do the codes match what the car has?

## A. Install

| # | Do | Look for |
| --- | --- | --- |
| A0 | Phone: Library › open `diagnostics-0.0.1.nbt` | **Project: NBTFoundry nf-diagnostics**, **Licence: MPL-2.0**; **Uses: engine data, phone screen**; **Can: clear fault codes (when parked)** |
| A1 | Phone: Car › install Diagnostics; approve on the car | The review shows the same Uses and Can lines; health check passes, "Installed" |
| A2 | Car: hold the push on **diagnostics** › settings | One setting: Debug log |

## B. Read on the car

| # | Do | Look for |
| --- | --- | --- |
| B1 | Start **diagnostics** | "Read all units", then 8 rows (DME, DME2, EGS, VTG, DSC, ACSM, EPS, BDC), each "not read" |
| B2 | Push **Read all units**; wait up to a minute. **Take a photo** | Each row: "reading...", then "N codes", "no codes" or "no answer". Expected "no answer": DME2 (except V8/V12), VTG (except xDrive). A row saying "no gateway" or "gateway busy": note which |
| B3 | Turn to a unit with codes, push | Its codes: six hex digits, then active (red), stored or pending (amber). **Take a photo**. If you know the car's codes (a workshop report, another reader), do they match? |
| B4 | **Read again** | The same list |
| B5 | BACK | Back on the list, on that unit |
| B6 | App Manager (MENU) | The row's status: "N fault codes" (or "No fault codes") |

## C. On the phone

| # | Do | Look for |
| --- | --- | --- |
| C1 | Phone: Car › diagnostics › **Phone page** | "N fault codes in M units", one line per unit with its codes, a **Clear X codes** button under each unit with codes |
| C2 | **Read all units** on the phone | Within a minute the page and the car's list both show the results again |

## D. Clearing (optional: it erases what a workshop may want to see)

Pick a unit whose codes you do not need, e.g. an old stored code.

| # | Do | Look for |
| --- | --- | --- |
| D1 | Car: open the unit › **Clear codes** | "Clear every fault code of …? Only while the car stands still." **No** is selected |
| D2 | Push on **No** | Back on the unit, nothing cleared |
| D3 | **Clear codes** › **Yes, clear them** | "clearing...", then "cleared, no codes", or fewer codes (an active fault comes back at once) |
| D4 | Phone: **Clear X codes** for another unit with codes, then **Keep them** | Nothing cleared |
| D5 | Same, then **Clear them** | Its line says "cleared, no codes" (or fewer codes); the car's list too |

## E. Update and close

| # | Do | Look for |
| --- | --- | --- |
| E1 | Phone: update to `diagnostics-0.0.2.nbt`; approve | Health check passes; the list starts "not read" again |
| E2 | Car: hold BACK about 1.5 s | Diagnostics closes |

To go back: phone › diagnostics › Uninstall, confirmed on the car.

## What to send back

- The results, one line per step (OK, or what you saw).
- Photos: the list (B2), a unit's codes (B3).
- Before switching off: phone › **diagnostics** › **Show log** › Share (the
  lines `DME: read: state 2, N codes`; state 3 is no answer, 4 a negative
  answer, 5 no gateway, 6 gateway busy). If a row said "no gateway": Car ›
  Manage › **iDrive log** too.
