# Hammer firmware: what is published, and where

A record of what Hammer Miner has actually released, gathered from primary
sources so the claims in this repository's documentation can be checked rather
than taken on trust.

**Collected:** 30 August 2026, via the GitHub REST API.
**Method:** every figure below came from `api.github.com`, not from a web page,
a screenshot, or anyone's account of it. The commands are given so the same
numbers can be produced independently. Where a document is quoted it is quoted
exactly, and the source is linked.

Nothing here is an inference about intent. Where something is an opinion or a
judgement it is marked as one.

---

## Summary

Hammer's code is spread across two GitHub organisations. One holds compiled
binaries and no source. The other holds full firmware source for three models,
was created the day that source appeared, carries no licence file, and is not
referenced from anywhere in the first.

---

## 1. The `HammerMiner` organisation

<https://github.com/HammerMiner>

```bash
gh api users/HammerMiner
```

| | |
|---|---|
| Type | Organization |
| Display name | Hammer Miner |
| Website | www.hammerminer.com |
| Location | United States of America |
| Created | 2026-04-08 |
| Public repositories | 17 |

### 1.1 What the repositories contain

```bash
gh api "orgs/HammerMiner/repos?per_page=100" --paginate
```

| Repository | Size (KB) | Last push | Description |
|---|---|---|---|
| BC01-APP | 0 | 2026-06-25 | 1-Asic BTC Miner |
| BC01-WWW | 0 | 2026-06-25 | Miner UI |
| BC04 | 4 | 2026-07-12 | 4-BM1370: BTC Miner; Thor OS |
| BC04-APP | 0 | 2026-08-27 | 4-Asic BTC Miner |
| BC04-WWW | 0 | 2026-06-25 | Miner UI |
| BC08 | 1 | 2026-08-21 | BC08 solo home miner |
| DC02-APP | 1 | 2026-06-27 | 2-Asic LTC Miner |
| DC02-V1-APP | 3 | 2026-04-15 | 2-Asic LTC Miner |
| DC02-V1-WWW | 4 | 2026-04-15 | Miner UI |
| DC02-WWW | 1 | 2026-06-27 | Miner UI |
| DC04-APP | 0 | 2026-06-29 | 4-Asic LTC Miner |
| DC04-WWW | 0 | 2026-06-29 | Miner UI |
| DC06-APP | 0 | 2026-06-27 | 6-Asic LTC Miner |
| DC06-WWW | 0 | 2026-06-27 | Miner UI |
| P2 | 2 | 2026-08-19 | |
| X1 | 9 | 2026-08-28 | test |
| hammer-claw-skills-lab | 2061 | 2026-08-04 | skills-lab |

Sixteen of the seventeen are between 0 and 9 KB.

### 1.2 The BC01 repositories specifically

```bash
gh api repos/HammerMiner/BC01-APP/contents
gh api repos/HammerMiner/BC01-APP/commits
```

| | `BC01-APP` | `BC01-WWW` |
|---|---|---|
| Files in tree | `README.md` only | `README.md` only |
| README size | 28 bytes | 20 bytes |
| Commits | 1 | 1 |
| Commit message | "Initial commit" | "Initial commit" |

`BC01-APP/README.md`, in full:

```
# BC01-APP
1-Asic BTC Miner
```

There is no firmware source, no build script and no configuration in either
repository.

### 1.3 What is actually distributed

The payloads are release assets, which do not count toward repository size.

```bash
gh api repos/HammerMiner/BC01-APP/releases
```

| Repository | Releases | Latest tag | Asset |
|---|---|---|---|
| BC01-APP | 2 | V2.0.3-20260625 | `bc01-miner-2.0.3-20260625-update.bin` |
| BC01-WWW | 2 | V1.4.0-20260625 | `bc01-www-1.4.0-20260625-update.bin` |
| BC04-APP | 10 | V3.0.4-20260826 | `bc04-3.0.4-20260826-update.bin` |
| BC04 | 1 | v1.0.1 | `Thor-BC04.bin` |
| DC02-APP | 3 | V2.0.2-20260627 | `dc02-miner-2.0.2-20260627-update.bin` |
| DC02-WWW | 3 | V1.3.8-20260627 | `dc02-www-1.3.8-20260627-update.bin` |
| DC04-APP | 1 | V2.0.2-20260629 | `dc04-miner-2.0.2-20260629-update.bin` |
| DC06-APP | 2 | V2.0.2-20260625 | `dc06-miner-2.0.2-20260625-update.bin` |
| BC08 | 2 | v1.0.2-20260820 | `bc08-1.0.2-20260820-update.bin` |
| X1 | 3 | v1.0.4-20260827 | `thorx1-1.0.4-20260827-update.bin` |
| P2 | 2 | v1.0.1-20260818 | `thorp2-1.0.1-20260818-update.bin` |

Every artifact is a compiled `.bin`. Binary releases were still being published
in late August 2026 — `BC04-APP` most recently on 2026-08-26.

### 1.4 The damage warning

`HammerMiner/DC02-APP/README.md`, quoted exactly:

> Firmware Upgrade Compatibility Notice
> -2.x.x devices: Flash this firmware (DC02-APP).
> 1.x.x devices: Please download the corresponding firmware from [GitHub Releases](https://github.com/HammerMiner/DC02-V1-APP).
> (Warning: Upgrading 1.x.x firmware with this firmware will cause device damage)

No recovery procedure accompanies it.

---

## 2. The `baichuan-org` organisation

<https://github.com/baichuan-org>

```bash
gh api users/baichuan-org
```

| | |
|---|---|
| Type | Organization |
| Display name | *(none)* |
| Website | *(none)* |
| Description / bio | *(none)* |
| Location | *(none)* |
| Created | **2026-08-21T03:29:59Z** |
| Public repositories | 3 |

### 2.1 What is there

| Repository | Size (KB) | Last push | Description |
|---|---|---|---|
| [BC01](https://github.com/baichuan-org/BC01) | 68,126 | 2026-08-21 | BC01 lab version from Chengdu Baichuan |
| [BC04](https://github.com/baichuan-org/BC04) | 67,618 | 2026-08-21 | |
| [DC02](https://github.com/baichuan-org/DC02) | 69,872 | 2026-08-21 | DC02 lab version |

`baichuan-org/BC01` top level:

```
CMakeLists.txt   README.md      components/    config.cvs
dependencies.lock  idf_component.yml  main/    merge_bin.sh
package-lock.json  package.json   partitions.csv  sdkconfig
setup.py
```

`main/` holds 27 entries including `main.c`, `device.c`, `global_state.h`,
`http_server/` and `displays/`.

**This is genuine, complete, buildable firmware source** — a standard ESP-IDF
project. Any claim that Hammer released no source is wrong.

### 2.2 No licence file

```bash
gh api repos/baichuan-org/BC01/license   # 404
```

None of the three repositories contains a `LICENSE` or `COPYING` file, and
GitHub detects no licence for any of them. The release states its purpose is
satisfying GPL obligations; GPL-3.0 §4 requires the licence text accompany the
source.

For contrast, this repository carries `LICENSE` (GPL-3.0, detected by GitHub)
along with [PROVENANCE.md](PROVENANCE.md) and [CREDITS.md](CREDITS.md).

### 2.3 The README, quoted in full

<https://github.com/baichuan-org/BC01/blob/main/README.md>

> This repository contains the firmware source code for the pre-production
> product that was released to the market in limited quantities.
>
> Why we are open-sourcing this code: Due to time constraints during the early
> development phase, the firmware was outsourced to Chengdu Baichuan for
> development. At that time, our team was unaware that the deliverable
> incorporated code derived from ESP Miner. Following an internal compliance
> audit, we have determined that the GNU General Public License (GPL)
> obligations apply. In full compliance with the GPL, we are now releasing the
> corresponding source code.
>
> Important distinction: The Hammer R&D team has since embarked on a clean-room
> effort to develop its own firmware operating system from the ground up, with
> strict independence from ESP Miner from day one. The code in this repository
> represents the laboratory version of the pre-production product only. It is
> entirely separate from and unrelated to our subsequent self-developed systems,
> including but not limited to THOR OS, NORN OS, and GLOD OS.
>
> Maintenance status: The Hammer team will not be able to provide ongoing
> maintenance, support, or continued development for the code released in this
> repository. This release is provided as-is to satisfy our GPL compliance
> obligations.

Three things this establishes on the record:

1. **An ESP-Miner derivation is acknowledged**, and GPL obligations accepted.
2. **It is scoped** to the outsourced pre-production build. THOR OS, NORN OS and
   GLOD OS are asserted to be clean-room and unrelated. This document takes no
   position on that assertion, which is not independently verifiable from
   published material.
3. **The released code is unmaintained by their own statement.**

The voice is Hammer's, not Baichuan's: it is Hammer who *"outsourced to Chengdu
Baichuan"* and Hammer whose *"R&D team has since embarked"*. Baichuan is the
subject described, not the author.

---

## 3. The two organisations are operated by the same people

Commit metadata is public. Identities below are GitHub account names; the
underlying addresses are visible through the API and are not reproduced here.

```bash
gh api "repos/baichuan-org/BC01/commits"
gh api "repos/HammerMiner/hammer-claw-skills-lab/commits?per_page=100"
```

| Account | In `baichuan-org` | In `HammerMiner` |
|---|---|---|
| `EdwinSong` | **both** commits to `BC01`, including "Update README with firmware and compliance details" (2026-08-21) | **8 of 52** commits to `hammer-claw-skills-lab` (2026-08-04) |
| `xieliyi2026` | — | `BC01-APP`, `BC01-WWW`, `BC04-APP`, `DC02-APP` |
| `Hammer-Miner` | — | `hammer-claw-skills-lab` |
| `liuhq123321` | — | `BC08`, `X1`, `hammer-claw-skills-lab` |

Commits under `HammerMiner` are authored from the corporate domain
`gullpower.com`.

The account that wrote Hammer's compliance statement holds commit access inside
Hammer's own organisation. The source release is Hammer's own work, not a third
party's.

---

## 4. Nothing links them publicly

Every README in the `HammerMiner` organisation was checked for any mention of
`baichuan`, `GPL`, `source`, or `licence`:

```
BC01-APP     no mention        BC08         no mention
BC01-WWW     no mention        X1           no mention
BC04-APP     no mention        P2           no mention
BC04         no mention        DC02-V1-APP  no mention
DC02-APP     no mention
```

Neither organisation publishes its members. `baichuan-org` has no display name,
website or description.

**The practical effect:** an owner who goes to Hammer's GitHub looking for the
source to their miner finds compiled binaries and a 28-byte README, with nothing
indicating that source exists or where to find it. Whether that is deliberate is
not something this document can establish. That it is the outcome is a matter of
record.

---

## 5. What is not claimed here

Kept separate so it is clear where the evidence stops.

- **No position on whether THOR/NORN/GLOD OS derive from ESP-Miner.** Those are
  not published, so nobody outside Hammer can say. Their statement that these
  are clean-room is recorded above and is neither corroborated nor contradicted
  by anything available.
- **No claim that Hammer released nothing.** They released full source for three
  models. The findings concern licensing and discoverability, not existence.
- **No claim about intent.** Only about what is published, and where.
- **No claim about the shipping firmware's provenance**, beyond noting the
  acknowledged derivation applies to the pre-production build.

---

## 6. Reproducing this

```bash
gh api users/HammerMiner
gh api "orgs/HammerMiner/repos?per_page=100" --paginate
gh api repos/HammerMiner/BC01-APP/contents
gh api repos/HammerMiner/BC01-APP/commits
gh api repos/HammerMiner/BC01-APP/releases
gh api repos/HammerMiner/DC02-APP/readme

gh api users/baichuan-org
gh api "users/baichuan-org/repos?per_page=100"
gh api repos/baichuan-org/BC01/contents
gh api repos/baichuan-org/BC01/readme
gh api repos/baichuan-org/BC01/license      # 404
gh api repos/baichuan-org/BC01/commits
```

Figures were accurate on 30 August 2026. Repositories change; re-run before
relying on any number here.

## 5a. The vendor's own site does not lead to the source

<https://www.hammerminer.com/#/service#download>, read 30 August 2026.

The firmware download page offers "APP FIRMWARE" and "WEB FIRMWARE" for BC01,
BC04, DC02 and DC06. Every download control links to a GitHub releases page in
the `HammerMiner` organisation:

```
BC01 APP  -> github.com/HammerMiner/BC01-APP/releases
BC01 WEB  -> github.com/HammerMiner/BC01-WWW/releases
BC04 APP  -> github.com/HammerMiner/BC04-APP/releases
DC02 APP  -> github.com/HammerMiner/DC02-APP/releases
DC02 WEB  -> github.com/HammerMiner/DC02-WWW/releases
DC06 APP  -> github.com/HammerMiner/DC06-APP/releases
DC06 WEB  -> github.com/HammerMiner/DC06-WWW/releases
```

Those are the repositories shown in section 1 to contain no source — a README
and compiled `.bin` release assets.

A further link, "Browse All Firmware on GitHub", points to
`github.com/HammerMiner/` beneath this description:

> Find firmware releases, source code, and documentation for all Hammer Miner
> devices.

No source code is published in that organisation. The source is in
`baichuan-org`, which is not linked or named anywhere on the page.

**The consequence is concrete.** An owner following the vendor's own
instructions to find the source for their device is directed to compiled
binaries and told that is where the source code is. The corresponding source
exists, but nothing in the supply chain the owner is given leads to it.

Two other statements from the same page are worth recording, since they bear on
claims made elsewhere about which devices are current:

> THOR OS [...] ships natively on every new Hammer Miner — THOR X1, THOR P2 and
> BC08

> Existing Hammer Miner models BC01 and BC04 will receive dedicated THOR OS
> upgrade packages, keeping already-deployed hardware on the same platform.

The BC04 entry lists "V3.0.2 | 2026-08-02 — Upgrade to new THOR OS Firmware".
BC01 is therefore described by the vendor as scheduled to receive THOR OS, not
as discontinued.

Contact address published on the same page: `info@hammerminer.com`, described as
the "Official Support Channel".

## 7. Preservation

Published repositories can be withdrawn. What is recorded here does not depend
on them staying up.

Each source repository was cloned in full on 30 August 2026. A git commit id is
a cryptographic commitment to the entire tree, so a copy can be proven identical
to what was published by checking its HEAD against these:

| repository | HEAD commit |
|---|---|
| `baichuan-org/BC01` | `8dab8f4b1e7b81eea0f3063b09619677a1252dca` |
| `baichuan-org/BC04` | `059321423fc2608d1acb229e64f466233e24dbff` |
| `baichuan-org/DC02` | `55d10fd4ca07f16efbe195cd6805a69f790da46e` |

The BC01 id is the same commit this firmware's `main/device.c` already cites for
the imported USB-PD sequence, so that attribution is checkable against the
archive independently.

Alongside the clones, the raw API responses and the READMEs quoted above were
captured verbatim, each recorded with its SHA-256 in a manifest. Those files are
held outside this repository — they are evidence, not part of the firmware, and
none of it is anything the licence prevents anyone from redistributing.

Screenshots were deliberately not relied on. An image cannot be verified against
anything; a commit id and a digest can.

## Related

- [PROVENANCE.md](PROVENANCE.md) — how this firmware descends from its upstreams
- [FINDINGS.md](FINDINGS.md) — defects found in the shipped firmware
- [SECURITY.md](SECURITY.md) — security findings and mitigations
- [CREDITS.md](CREDITS.md) — the open source work this builds on
