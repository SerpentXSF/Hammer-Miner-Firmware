# Building for a particular board

One source tree, one build per board, one directory each.

```bash
python tools/build_board.py bc01
python tools/build_board.py bc04
```

Output lands in `build/bc01/` and `build/bc04/`, so the two images can never
overwrite each other and neither can be flashed by accident from a stale
directory.

`tools/build_board.py <board> flash monitor` passes any further arguments
through to `idf.py`.

## Why the boards are not separate source trees

Because the code is the same and the wiring is not. Everything that differs
between models — voltage domain, ASIC difficulty, job cadence, which chip
driver runs — is already selected at runtime from the model string in NVS, in
`NVSDevice_parse_config()` and the dispatch tables in
`components/asic/asic.c`.

Forking the tree per board would mean every fix from here on gets applied
twice, and the two copies drift until one of them is quietly wrong. The thing
that actually needs separating is the *build*, and that is what the board
files do.

## What actually differs

| | BC01 / BC01 Pro | BC04 |
|---|---|---|
| ASIC UART TX | 18 | 17 |
| ASIC UART RX | 17 | 18 |
| `CONFIG_DEVICE_MODULE` | `BC01` | `BC04` |

That is the whole of `boards/bc01.defaults` and `boards/bc04.defaults`. Each is
applied as a fragment on top of the base `sdkconfig`, which carries the tuning
that is not board specific — PSRAM, partition layout, log levels — so a board
file only ever states what that board changes.

The BC04 pin assignment comes from the vendor's own reference configuration
and has since been **confirmed on hardware** — a BC04 running this build
detects all four ASICs and mines. See [Bringing up a BC04](#bringing-up-a-bc04)
for what it does and what it does not do.

## The trap this exists to close

The two lines above are swapped between the boards, and getting them wrong
does not look like getting them wrong. The miner boots, serves its web
interface, reports the correct chip, warms up — and never returns a single
share. It reads as dead hardware, and diagnosing it from that end took a long
time.

Two things made this easy to hit:

- **The pins are fixed when the image is built; the board model is read from
  NVS at runtime.** Nothing connected the two, so a BC01 image whose settings
  said BC04 would take every BC04 code path over BC01 wiring.
- **The Kconfig defaults in `components/bc_hal` are 17/18 — the BC04
  arrangement.** The BC01 values lived only in the generated `sdkconfig`
  checked into the repository. Deleting or regenerating that file silently
  produced a BC04 pinout for a BC01 build.

`boards/*.defaults` fixes the second. The check in `main.c` fixes the first: at
boot the firmware compares the model in NVS against the pins it was compiled
with and, where it knows that board's pinout, says so plainly rather than
letting it look like a hardware fault.

```
E serpentx: ================ BOARD MISMATCH ================
E serpentx: Settings say this is a BC01.
E serpentx: This image drives the ASIC on TX 17 / RX 18;
E serpentx: a BC01 needs TX 18 / RX 17.
E serpentx: The chip will be detected and will never return a
E serpentx: share. Flash the BC01 build instead.
E serpentx: ================================================
```

`/api/system/info` reports the same as `boardMismatch`, and the display shows

```
Wrong firmware
for this board
```

The screen matters more than it looks. A mismatched board produces exactly the
symptom the firmware already had a message for -- the chip is found, clocked,
and silent -- so it used to show **No ASIC response** and then restart, having
waited three minutes. Rebooting cannot change which pins an image was compiled
for, so it failed again three minutes later, forever, taking the web interface
needed to flash the right build away every time. A known mismatch now says what
it is, immediately, and does not restart.

Only boards whose pinout is actually known are checked — the BC01 because it
was measured on hardware, the BC04 from the vendor reference. An unrecognised
model passes, because a guess there would block a board that works.

Verified by flashing a BC04 image onto a BC01: the message appeared at boot,
`boardMismatch` came back true, and the hashrate stayed at zero — the original
failure, reproduced deliberately and now announced instead of hidden.

## Bringing up a BC04

**A BC04 has now been run with this firmware.** It passes its self test,
detects four BM1370s, and mines — measured 3 September 2026 at 640 MHz /
460 cV: 26 accepted shares, none rejected, no hardware errors in 204 nonces,
85 W, chip 53 °C against a 57 °C regulator. `boards/bc04.defaults` is
confirmed correct as written.

Three things had to be fixed before it would run, and one is still open. All
of them are below.

The board file began as three settings — the model and the two UART pins —
and that was not enough. Diffing the two reference configurations turned up
eight more, and two of those would each have produced the same failure the
BC01 spent a session on: a miner that boots, serves its interface, detects the
chip, and never returns a share. Every one of those eight proved correct on
hardware; none of them was the thing that actually broke.

| | BC01 | BC04 |
|---|---|---|
| ASIC UART TX / RX | 18 / 17 | **17 / 18** |
| ASIC reset | GPIO 3 | **GPIO 1** |
| `TPS546_VOUT_MAX` | 130 | **520** |
| `ASIC_VOLTAGE` | 120 | 125 |
| Second I2C bus | SDA 11 / SCL 12 | none |
| Fan | LEDC PWM + tach | EMC2302 |
| Power | USB-PD, HUSB238A, VBUS gate | barrel jack |

The voltage row is the one that is not obvious. A BC04 runs **four ASICs in
series**, so its core voltage is roughly four times a single domain — about 500
where a BC01 wants 120. The base configuration caps at 130 because it is a BC01,
and a BC04 built without overriding that clamps to a quarter of the voltage it
needs. Nothing reports a fault; it simply does not work.

Check before anything else:

1. **eFuses.** `esptool.py get_security_info`. If Secure Boot is enforced, stop
   and read [SECURE-BOOT.md](SECURE-BOOT.md).
2. **`DEVICE_BC04` in all four dispatch functions** in
   `components/asic/asic.c` — `ASIC_send_work`, `ASIC_set_version_mask`,
   `ASIC_set_frequency`, `ASIC_read_registers`. A missing `send_work` case is
   why the BC01 never hashed.
3. **Confirm hashing by accepted shares, not by wattage.** The BC01 drew the
   same ~24 W whether it was hashing or not.

Two things still need BC04 values: `config.cvs.example` is BC01-specific
(voltages near 120, `flipscreen 0`), and the
[benchmark](https://github.com/SerpentXSF/StayOpen-Hashrate-Benchmark) has
profiles for `bitaxe` and `bc01` only.

### What a BC04 actually wants

Taken from the vendor's own `config.cvs` for this board, not guessed:

| | |
|---|---|
| `devicemodel` / `boardversion` | `BC04` / `V05` |
| Normal profile | 640 MHz @ **460** |
| Over-frequency profile | 750 MHz @ **470**, ceiling 480 |
| `flipscreen` | **1** — the opposite of a BC01 |
| Core voltage reported | ~4.79 V; four ASIC domains in series |
| Draw | **~85 W**, roughly 16–18 A on the core rail |

`selftest` is worth leaving at `0` on a first flash. `0` means *run it* —
it is set to `1` once it passes — and on this board it is a genuinely useful
check: it exercises the fan through all four PWM steps and confirms all four
ASICs before the miner ever tries to mine.

### The BC04 has Ethernet, and it does not survive hashing

This page used to warn that the BC04 might have no W5500 and would fail the
self test's Ethernet check. **The opposite is true on both counts.** The board
has a W5500, it passes the check, and it takes a DHCP lease, resolves DNS and
syncs NTP without complaint.

Then the hashboard is powered, and **70 ms later** every socket command starts
timing out and never recovers:

```
I (10658) vcore: Set ASIC voltage = 4.80V
E (10728) w5500.mac: w5500_send_command(210): send command timeout
E        esp_eth: eth_on_state_changed(151): ethernet mac set link failed
```

The interface keeps its address and silently stops passing packets, so the
miner looks networked and cannot reach a pool. Ruled out by measurement, not
by argument:

- **SPI clock.** 16 MHz is the W5500's rated ceiling; 8 MHz behaves identically.
- **Task starvation.** `volc_delay()` is a plain `vTaskDelay`.
- **Supply sag.** 12.30 V idle to 12.125 V under 85 W — 1.4 %, and the failure
  begins before the ASICs are clocked at all.
- **Pin or bus conflict.** Nothing else uses SPI2, and `power_on_hashboard()`
  only writes a voltage over I2C. No GPIO is touched between the two events.
- **A wedged socket.** `esp_eth_stop()` cannot even reset the PHY afterwards.

Seventy milliseconds is the switching transient itself, and the driver reports
a *link state* change rather than only failed commands, so the PHY side is
affected too. The likeliest reading is that bringing up the 16 A core
regulator resets or browns out the W5500 — a hardware interaction on this
board, which is why no software change touched it.

`network_eth_recover()` in `main/network.c` exists for this and **does not fix
it.** `esp_eth_start()` only reopens socket 0; it never re-runs
`emac_w5500_init()`, which is what writes the MAC and sets MACRAW mode, so a
controller that came back blank stays blank. A real fix would be a full
`esp_eth_driver_uninstall()` and re-init once the rail settles.

Do not be tempted to "improve" that function by starting anyway when the stop
fails. A failed stop means the driver never emitted `ETH_EVENT_STOP`, the
netif is still attached, and starting again trips
`assert failed: netif_add (netif already added)` — a reboot loop for as long
as Ethernet is enabled. That was tried, and it cost nine boots to learn.

**Run a BC04 on WiFi.** `eth_on` and `wifi_on` are settable through
`PATCH /api/system` and reported by `/api/system/info`, so Ethernet can be
turned off without a factory restore — which matters, because a factory
restore also discards the WiFi credentials that are the only way back in.

## Releases

```bash
python tools/build_board.py bc01
python tools/make_release.py bc01
python tools/publish_flasher.py bc01 --push
```

Artifacts carry the board in the filename (`stay-open-bc01-<version>-full.bin`),
and `make_release.py` reads that board's build directory and refuses to publish
a build whose own sdkconfig names a different model than the filename claims.
The filename is the only thing telling someone which board an image is for, and
the cost of getting it wrong is a miner that looks broken.

Only the BC01 is published. A BC04 has now been run on real hardware and
mines, but its Ethernet does not survive the hashboard powering up, so a
published image would hand BC04 owners a miner that needs WiFi without saying
so. That gets published once the Ethernet question is closed. Either model can
be built from source with the command at the top of this page.

### Why the flasher image lives on gh-pages

esp-web-tools fetches the firmware with XHR, and GitHub release assets are
served without CORS headers, so the image has to come from the same origin as
the page. It therefore has to be somewhere Pages can serve.

Keeping it on `main` meant every release added another 12 MB blob to the
project's permanent history, where nothing could remove it afterwards.
`publish_flasher.py` writes `gh-pages` as a single **orphan** commit instead, so
the previous image stops being referenced rather than accumulating, and the
branch can be regenerated wholesale at any time. It is generated output, not
source, which is why the push is a force.

The branch is assembled with git plumbing, so publishing never touches the
working tree, the index, or the current branch. `.gitignore` keeps flasher
binaries off `main` so this cannot quietly regress.

Note that history already written is not affected: the blobs from releases up
to 2.0.8 remain in `main`. Only the growth stops.
