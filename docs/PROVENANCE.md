# Provenance

Evidence that this firmware derives from ESP-Miner, and what the vendor
release did and did not include.

Everything below is reproducible from the two archives named in
[README.md](../README.md) plus a clone of upstream ESP-Miner. Nothing here
rests on the vendor's own account of events.

---

## 1. The vendor's statement

The `README.md` in `github.com/baichuan-org/BC04` says, in full:

> Due to time constraints during the early development phase, the firmware
> was outsourced to Chengdu Baichuan for development. At that time, our team
> was unaware that the deliverable incorporated code derived from ESP Miner.
> Following an internal compliance audit, we have determined that the GNU
> General Public License (GPL) obligations apply. In full compliance with the
> GPL, we are now releasing the corresponding source code.

The claim of full compliance is not accurate as released. See section 5.

---

## 2. Direct textual evidence

These markers are present in the vendor's own source tree:

| Location | Content |
|---|---|
| `main/http_server/axe-os/` | The web UI directory is named after **AxeOS**, ESP-Miner's web interface. |
| `main/http_server/openapi.yaml:3` | `title: ESP-Miner API` |
| `main/http_server/openapi.yaml:228` | `Set custom voltage/frequency in AxeOS` |
| `components/asic/bm1370.c:354` | Comment citing `https://github.com/bitaxeorg/ESP-Miner/pull/167` |
| `components/connect/connect.c:222` | `char ssid_with_mac[13]; // "Bitaxe" + 4 bytes from MAC address` |
| `components/stratum/test/test_mining.c:24` | `// Values calculated from esp-miner/components/stratum/test/verifiers/merklecalc.py` |
| `components/stratum/test/verifiers/` | `bm1397.py` and `merklecalc.py`, carried over verbatim from ESP-Miner. |
| `main/http_server/axe-os/src/api/index.ts:93` | Default payout address `bc1q99n3pu…w20d.bitaxe-U1` — ESP-Miner's upstream default. |
| `config.cvs` (influx section) | Bucket and org both `nerdqaxeplus`, from the NerdQAxe+ fork. |

---

## 3. Measured similarity to upstream

Compared against ESP-Miner `dc5f6e8` (v2.15.0, 2026-08-26) by matching
filenames and diffing line sequences. Full output:
[upstream-similarity.txt](upstream-similarity.txt).

**41 files** in the vendor tree have a direct upstream counterpart.
**4,367 lines are identical.** Six files match exactly:

```
100.0%  components/asic/crc.c
100.0%  components/asic/include/crc.h
100.0%  components/asic/include/pll.h
100.0%  components/dns_server/include/dns_server.h
100.0%  main/pmbus_commands.h
100.0%  main/tasks/create_jobs_task.h
 99.0%  components/dns_server/dns_server.c
 98.4%  components/asic/pll.c
```

Larger files remain substantially upstream:

```
 71.2%  components/bc_hal/TPS546.c   (943 of 1356 lines)
 67.9%  components/asic/bm1370.c         (310 of  497 lines)
 42.4%  components/connect/connect.c     (268 of  397 lines)
```

This understates the true overlap. Upstream HEAD is far newer than the
release BC04 forked from, so much of the measured difference is upstream's
own subsequent development, not vendor modification.

The whole-program architecture — the FreeRTOS task split
(`create_jobs_task` / `asic_task` / `asic_result_task` / `stratum_task`),
the `work_queue` between them, the `GLOBAL_STATE` aggregate, the NVS
config layer, and the self-test harness — is ESP-Miner's design, carried
over with the file names intact.

---

## 4. The BC01 source is published, but not where the binaries are

**Correction.** Earlier revisions of this document stated that the BC01
source had never been published. That was wrong, and the error is recorded
here rather than quietly edited out.

The BC01 source does exist, in full:

- <https://github.com/baichuan-org/BC01> — "BC01 lab version from Chengdu
  Baichuan", 3,077 files, pushed 2026-08-21. Contains the complete tree,
  including `components/bc_hal/HUSB238A.c`, `components/asic/bm1373.c`,
  and the BC01 and BC01-Pro code paths that the BC04 release does not have.

What misled the earlier reading is that this is not where the firmware is
distributed from. The releases live in a different organisation:

| Repository | Contents | Releases |
|---|---|---|
| `HammerMiner/BC01-APP` | one commit, a two-line `README.md` | `bc01-miner-2.0.3-20260625-update.bin`, 24 downloads |
| `HammerMiner/BC01-WWW` | one commit, a two-line `README.md` | `bc01-www-1.4.0-20260625-update.bin`, 18 downloads |
| `baichuan-org/BC01` | **the full source** | none |

So a user who downloads the firmware lands on a repository that presents
itself as the source and contains a README, with nothing pointing to the
organisation that actually holds it. The 392-byte zip really is the
complete export of `HammerMiner/BC01-APP`; its commit hash
`cc537261ae1e4e95bc485a4a73a9f19dcd67af33` matches that repository's only
commit. The mistake was concluding from an empty repository that no
repository was populated.

That is a materially smaller failing than non-publication, and it should be
described accurately. What remains is real but narrower:

- **GPL-3.0 section 6** asks that the corresponding source accompany the
  object code, or be offered from where the object code is conveyed.
  Publishing it in an unlinked repository under a different account does
  not meet that, though the source plainly exists and is public.
- **`baichuan-org/BC01` carries no LICENSE file**, the same section 4 gap
  as the BC04 release described below.
- The `components/a/liba.a` blob is present there too, so section 5.2
  applies to the BC01 release exactly as it does to the BC04 one.

The practical cost of the discoverability gap was measurable. Bringing this
firmware up on a BC01 required decrypting the shipping image, recovering a
driver from `__func__` strings, writing an Xtensa disassembler and sweeping
256 I2C registers on live hardware — to establish a register map that was
sitting in a public repository the whole time. See
[BC01-USB-PD.md](BC01-USB-PD.md).

## 5. What is missing from the vendor release

### 5.1 No license text, no notices

The BC04 repository contains no `LICENSE`, no `COPYING`, and no copyright
header anywhere attributing ESP-Miner. The only license files in the tree
belong to the vendored LVGL dependency.

GPL-3.0 section 4 requires conveying the license text and preserving all
notices. Section 5(a)–(b) requires that modified versions carry
prominent notices of modification and be licensed as a whole under the
GPL. Publishing source with the notices stripped does not satisfy either.

### 5.2 A binary blob with no source

`components/a/liba.a` is a 259,738-byte precompiled Xtensa static library,
linked directly into the firmware by `components/a/CMakeLists.txt`. Its
only accompanying source is a header of declarations.

It is not a separable System Library under GPL-3.0 section 1 — it is
first-party product code, built from a single file
`asic_abstraction.c` that was withheld while the rest of the tree was
published. Corresponding source under section 6 means all of it.

The blob was shipped with complete DWARF-4 debug information, which
identifies the withheld file and its build:

```
F:/workspace/work_volc/temp_mini/components/a/asic_abstraction.c
GNU C17 14.2.0 -mdynconfig=xtensa_esp32s3.so -mlongcalls -gdwarf-4 -O2 ...
```

See [ASIC-ABSTRACTION.md](ASIC-ABSTRACTION.md) for the reconstruction that
replaces it, so this tree builds entirely from source.

### 5.3 No build instructions or configuration provenance

No documented toolchain version, no `sdkconfig.defaults`, and a
`dependencies.lock` that disagrees with the vendored dependency: the lock
and `idf_component.yml` ask for LVGL `^9.3.0` while
`components/lvgl__lvgl/` contains 9.2.2, which silently takes priority.

### 5.4 The published source does not build as released

The BC01 tree at `baichuan-org/BC01` cannot be built by anyone who
downloads it. `main/CMakeLists.txt` embeds a file into the application
image in three separate build configurations:

```
list(APPEND EMBED_FILES
    "../secure/flash_encryption_key.bin"
)
```

No `secure/` directory is present in the repository. CMake stops at the
generate step:

```
CMake Error in main/CMakeLists.txt:
  Cannot find source file:
    secure/flash_encryption_key.bin
```

The reference build used for this repository's comparison work only
completed after supplying a placeholder file in its place.

GPL-3.0 section 1 defines corresponding source as everything needed to
"generate, install, and ... run the object code". A tree that halts at
configuration does not meet that, whatever else it contains. The key
itself need not be published -- but a release that unconditionally
requires it, with no documented way to build without it, is not a
buildable release.

### 5.5 The signing and encryption posture locks owners out

Two things follow from the shipped `sdkconfig`, and both were confirmed
on hardware:

- The stock BC01 application **cannot run on a module whose eFuses are
  not already burned**. Flashed to a replacement ESP32-S3, it aborts
  during startup:

  ```
  E flash_encrypt: Flash encryption eFuse bit was not enabled in
  bootloader but CONFIG_SECURE_FLASH_ENC_ENABLED is on
  abort() was called
  ```

- The retail module ships with Secure Boot v2 and flash encryption in
  release mode, so it will not accept a rebuilt image either.

The practical effect is that an owner can read the source but cannot run
a modified version on the device they bought, in either direction. That
is the arrangement GPL-3.0 section 6 addresses through its Installation
Information requirement, and no such information accompanies the release.
The route this repository documents in
[HARDWARE-SWAP.md](HARDWARE-SWAP.md) -- physically replacing the
socketed module -- exists precisely because no software route remains.

### 5.6 The flash encryption key is embedded in the application

`main/http_server/http_server.c` reads the same key back out of the
image at runtime:

```c
extern const uint8_t flash_encryption_key_bin_start[]
    asm("_binary_flash_encryption_key_bin_start");
```

A key compiled into a distributed binary is recoverable by anyone
holding that binary. This is noted as an observation about the vendor's
design, not as a vulnerability this repository introduces or relies on.

---

## 6. Prior reporting

Hammer's use of ESP-Miner without licensing was raised publicly before
this release, including by D-Central:
<https://d-central.tech/hammer-miner/>.

---

## 7. Reproducing this analysis

```bash
git clone --depth 50 https://github.com/bitaxeorg/ESP-Miner.git
python tools/compare_upstream.py --upstream ESP-Miner --tree .
```

`tools/compare_upstream.py` regenerates
[upstream-similarity.txt](upstream-similarity.txt).
