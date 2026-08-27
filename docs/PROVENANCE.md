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
 71.2%  components/hammer_hal/TPS546.c   (943 of 1356 lines)
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

## 4. The BC01 source was never published

Hammer maintains two repositories for the BC01, under a different
organisation from the BC04:

- <https://github.com/HammerMiner/BC01-APP> — described as "1-Asic BTC Miner"
- <https://github.com/HammerMiner/BC01-WWW> — described as "Miner UI"

**Each contains exactly one commit and one file: a two-line README.**
No source. Neither carries a LICENSE.

```
$ git clone https://github.com/HammerMiner/BC01-APP.git
$ git ls-files
README.md
$ git log --format="%H %ai %an: %s"
cc537261ae1e4e95bc485a4a73a9f19dcd67af33 2026-06-25 14:03:05 +0800 xieliyi2026: Initial commit
$ cat README.md
# BC01-APP
1-Asic BTC Miner
```

`BC01-WWW` is identical in form, at commit `cac9120b`.

That commit hash is what makes this conclusive rather than circumstantial.
`BC01-APP-2.0.3-20260625.zip`, the 392-byte archive distributed as the
BC01 source, carries `cc537261ae1e4e95bc485a4a73a9f19dcd67af33` in its
trailing comment — the same hash. The zip is therefore the complete,
unmodified source export of that repository, not a truncated download. The
repository really does contain nothing but a README.

Meanwhile the releases distribute compiled firmware:

| Repository | Release asset | Size | Downloads |
|---|---|---|---|
| BC01-APP | `bc01-miner-2.0.3-20260625-update.bin` | 2,560,049 | 24 |
| BC01-WWW | `bc01-www-1.4.0-20260625-update.bin` | 2,097,201 | 18 |

So the BC01 firmware is conveyed in object form, to users, from a
repository that presents itself as its source. Section 6 requires the
corresponding source to accompany that conveyance. A README is not it.

This is not a technicality about a product nobody runs. The shipping BC01
binary decrypts to an image built from this very tree — its embedded
`__FILE__` paths are the BC04 paths, misspelling `health_maintennance.c`
included — so the source that would satisfy the obligation demonstrably
exists. See [OTA-FORMAT.md](OTA-FORMAT.md#what-the-decrypted-image-proves).

The practical cost is documented in
[BC01-BRINGUP.md](BC01-BRINGUP.md): this firmware runs on BC01 hardware in
every respect except bringing up the core regulator, and the pin map and
power sequencing needed to close that gap are in the source that was not
released.

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
