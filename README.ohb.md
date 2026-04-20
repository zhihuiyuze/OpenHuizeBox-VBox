# OpenHuizeBox fork of Oracle VirtualBox OSE

> You landed on the **VirtualBox fork** of the OpenHuizeBox Project.
> The parent project is [`zhihuiyuze/OpenHuizeBox`](https://github.com/zhihuiyuze/OpenHuizeBox).
> The upstream Oracle `Readme.md` describing VirtualBox itself is kept
> next to this file.

## What this repository is

A fork of [Oracle VirtualBox OSE](https://github.com/virtualbox/virtualbox),
branch `VBox-7.2`, carrying the **OpenHuizeBox patch set** — the UI
integration, branding and per-VM identity-profile hooks that ship as part
of the OpenHuizeBox privacy-audit research sandbox.

This repository exists primarily to satisfy **GPL v3 §6**: when
OpenHuizeBox distributes a binary that includes modified VirtualBox
code, the corresponding source must be publicly available. That's what
this fork is.

## Branch layout

| Branch | Purpose |
|---|---|
| `ohb-7.2` | **Default branch.** OpenHuizeBox modifications on top of Oracle `VBox-7.2` at commit `e96c2fec`. The parent `zhihuiyuze/OpenHuizeBox` repo's submodule pins here. |
| `VBox-7.2` | Mirror of upstream Oracle `VBox-7.2`, kept for rebase reference. Do not force-push. |

## What's modified vs upstream

See [`PATCHES.md`](https://github.com/zhihuiyuze/OpenHuizeBox/blob/main/PATCHES.md)
in the parent project for the full enumerated list. Summary:

- 5-tab Qt Settings integration (Motherboard / Processor / Display /
  Storage / Network) — "Realistic Hardware Identity" groupbox
- OpenHuizeBox menu in the VirtualBox Manager window (About, governance,
  Create Audit VM dialog, hardware-identity viewer, apply-profile
  action, TLS root CA install, pktmon capture, tracker blocklist)
- Branding rewrite (`VBOX_RC_LEGAL_COPYRIGHT`) so Windows file
  properties credit OpenHuizeBox — GPL v3 attribution
- Device-side hooks that honour per-VM profile extradata
  (`DevACPI.cpp`, `DevFwCommon.cpp`, `DevATA.cpp`)
- Minor compile fixes for modern MSVC (19.50) and the
  `VBOX_WITH_3D_ACCELERATION=` empty-kmk-override path

The fork is deliberately **shallow in scope**: no VMM-level
counter-measures, no RDTSC / SIDT / SGDT evasion, no MSR masking.
See [`docs/DETECTOR_COVERAGE.md`](https://github.com/zhihuiyuze/OpenHuizeBox/blob/main/docs/DETECTOR_COVERAGE.md)
in the parent project for the explicit out-of-reach list.

## Upstream rebase workflow

1. Fetch Oracle upstream into the `VBox-7.2` branch of this fork.
2. Rebase `ohb-7.2` onto the new `VBox-7.2` tip.
3. Run `vbox-patches/apply_all.py --dry-run` (from the parent project)
   against a fresh Oracle checkout to catch drift in the small-diff
   patch set.
4. Run the parent project's `tests/ohb-smoke-all.ps1` against a
   freshly built tree.
5. If green, update the submodule pointer in `zhihuiyuze/OpenHuizeBox`
   to the new `ohb-7.2` SHA.

## License and attribution

- **GPL v3** — inherited from Oracle VirtualBox OSE. See
  [`COPYING`](./COPYING). All OpenHuizeBox modifications on top are
  also under GPL v3.
- **CDDL** — retained for the subset of files Oracle shipped under
  CDDL. See [`COPYING.CDDL`](./COPYING.CDDL).
- **Oracle attribution** — the upstream `Readme.md` credits Oracle as
  the original author. All Oracle copyright headers in individual
  source files are preserved unchanged.

"VirtualBox" and "Oracle" are trademarks of Oracle Corporation.
OpenHuizeBox is not affiliated with or endorsed by Oracle. See
[`TRADEMARKS.md`](https://github.com/zhihuiyuze/OpenHuizeBox/blob/main/TRADEMARKS.md)
and [`NOTICE`](https://github.com/zhihuiyuze/OpenHuizeBox/blob/main/NOTICE)
in the parent project.

## Issue tracking

This repository does **not** accept issues directly. File issues against
the parent project [`zhihuiyuze/OpenHuizeBox`](https://github.com/zhihuiyuze/OpenHuizeBox/issues).

For issues that specifically affect the VBox-level code (build failures
in `vbox-upstream/` paths, rebase conflicts against Oracle upstream),
tag with `area: vbox-upstream` when you file.

## Security

Security-sensitive reports: use the parent project's GitHub Security
Advisories — [`zhihuiyuze/OpenHuizeBox/security/advisories/new`](https://github.com/zhihuiyuze/OpenHuizeBox/security/advisories/new).
Do not file security reports on either repo's public issue tracker.
