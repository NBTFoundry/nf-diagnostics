<!-- SPDX-License-Identifier: MPL-2.0 -->
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
    <img src="docs/assets/logo-light.svg" alt="NBTFoundry" height="44">
  </picture>
</p>

<h1 align="center">Diagnostics for NBT Manager</h1>

> **Warning: installing or modifying this software can brick your head unit. Recovery may require professional service or replacement, at your expense. Provided AS IS; use at your own risk, subject to applicable law.** [Read the risk and liability disclaimer](DISCLAIMER.md).

The fault codes of the car's main control units, read and cleared on the
iDrive screen or in the NBT Manager phone app. Runs on the
[NBT App SDK](https://github.com/NBTFoundry/sdk), installed through
[NBT Manager](https://github.com/NBTFoundry/manager), which sends every
request to the car and refuses anything else.

**Supported head units:** BMW NBT Evo (iDrive 6).

Not affiliated with or endorsed by BMW AG. BMW, iDrive and NBT are trademarks
of BMW AG.

## What it reads

| Unit | Address | Role |
| --- | --- | --- |
| DME | 0x12 | engine |
| DME2 | 0x13 | second engine computer (V8, V12) |
| EGS | 0x18 | gearbox |
| VTG | 0x19 | transfer case (xDrive) |
| DSC | 0x29 | stability control |
| ACSM | 0x01 | airbags |
| EPS | 0x30 | steering |
| BDC | 0x40 | body (FEM or BDC) |

- **Codes:** six hex digits, as BMW's tools print them, with their state
  (active, stored, pending). No descriptions: search a code with the car's
  model.
- **No answer:** usually a unit this car does not have.
- **Clearing:** every code of one unit, after a second confirmation; NBT
  Manager allows it only while the car stands still.
- **Permissions shown before install:** engine data, phone screen; can clear
  fault codes (when parked).

## Controls

| Action | iDrive controller | Desktop |
| --- | --- | --- |
| Move | Turn, or tilt up/down | Up, Down |
| Choose | Push | Return |
| Back | BACK | Backspace |
| Quit | BACK, long hold | Escape |

**Phone:** NBT Manager app › Car › Diagnostics › Phone page: every unit's
codes, Read all units, Clear (asked twice).

## Build

Coding agents: [AGENTS.md](AGENTS.md).

Needs the NBT App SDK next to this repository (`../sdk`, or `NBT_SDK_DIR`),
gcc, Python 3, and the X11, EGL and GLESv2 development files; Xvfb for the
tests.

```sh
make -j"$(nproc)" host test test-asan check-static-init   # host binary and tests
./build/nbt-diagnostics-host                                    # on the desktop (X11); Escape closes it
```

The tests start the desktop build as NBT Manager does: a health check without
a window, then a launch under Xvfb. The package test needs NBT Manager's
checkout (`../manager`, or `NBT_MGR_DIR`) with `nbt-verify` built
(`make -C ../manager host`); it is skipped without.

Head unit, with the NBT toolchain (`make -C ../sdk toolchain` once):

```sh
make -j"$(nproc)" native native-check                 # build/nbt-diagnostics-native
make vm-test                                          # its health check in the SDK's VM
make APP_KEY=/path/to/apps-signing-key.pem package    # build/diagnostics-<version>.nbt
```

`release/package.args` describes the package: the id, the capabilities and
the settings. `release/sdk.ref` is the SDK commit CI and releases build
against. `make notice` writes `legal/NOTICE.txt`; `make test` fails when it
lags.

## Your own key

Without NBTFoundry's apps key, sign with a key of your own and add it to your
car once (an NBT Manager with developer keys):

```sh
openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3072 -out ~/.nbt/diagnostics-key.pem  # keep it private
openssl pkey -in ~/.nbt/diagnostics-key.pem -pubout -out release/developer-public.pem
make APP_KEY=~/.nbt/diagnostics-key.pem package
```

- **On the car:** copy `release/developer-public.pem` to the phone; in the
  NBT Manager app, Car › Manage › Developer keys › Pick a key file, then
  confirm the same fingerprint on the car. Packages signed with that key then
  install from the phone.
- **Releases from CI:** the private key, base64, as `APPS_SIGNING_KEY` in the
  `release` environment, and `public-key: release/developer-public.pem` under
  `with:` in `.github/workflows/release.yml`.

## The form

Native only: a program NBT Manager starts itself, not through `nbt-runner`.
Diagnostics need it (bridge 1.9, `diag` and `companion`; NBT Manager's
`docs/app-bridge.md`).

## Releases

`.github/workflows/ci.yml` runs the host tests, the same under ASan/UBSan and
the head-unit builds on every push and pull request. A `vX.Y.Z` tag runs
`.github/workflows/release.yml`: NBT Manager's application release workflow
builds the payload, signs it with the apps key after a maintainer approves
the run (the `release` environment, secret `APPS_SIGNING_KEY`) and publishes
`diagnostics-X.Y.Z.nbt`, sequence `X*1000000 + Y*1000 + Z`.

## License

Copyright (c) 2026 Fabien Poussin. Original code, tests, build scripts and
documentation are licensed under **MPL-2.0**; see [LICENSE](LICENSE).

Original NBTFoundry logos and artwork are MPL-2.0; copyright permission for
artwork does not grant trademark rights or imply endorsement by BMW AG.

See [legal/README.md](legal/README.md) for third-party license notices.
