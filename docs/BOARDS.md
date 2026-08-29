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

The BC04 pin assignment comes from the vendor's own reference configuration.
**No BC04 has been run with this firmware.** Treat that file as unverified.

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

`/api/system/info` reports the same as `boardMismatch`.

Only boards whose pinout is actually known are checked — the BC01 because it
was measured on hardware, the BC04 from the vendor reference. An unrecognised
model passes, because a guess there would block a board that works.

Verified by flashing a BC04 image onto a BC01: the message appeared at boot,
`boardMismatch` came back true, and the hashrate stayed at zero — the original
failure, reproduced deliberately and now announced instead of hidden.

## Releases

Release artifacts carry the board in the filename
(`serpentx-bc01-<version>-full.bin`). Only the BC01 is published, because only
the BC01 has been run on real hardware. The other models can be built from
source with the command at the top of this page.
