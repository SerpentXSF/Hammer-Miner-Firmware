# The BC01 USB-PD controller, recovered from firmware

The BC01 powers its hashboard through a **HUSB238A USB Power Delivery sink
controller** on I²C. Nothing in the released BC04 source mentions it, so
this firmware never negotiated a supply and never switched VBUS on — which
is why the BM1370 could not be brought up at all. Background in
[BC01-BRINGUP.md](BC01-BRINGUP.md).

The BC01 source that would describe this was never published
([PROVENANCE.md](PROVENANCE.md#4-the-bc01-source-was-never-published)), so
what follows was read out of the shipping `bc01-miner-2.0.3` image with
`tools/dwarf_dump.py`'s sibling tooling and a small Xtensa disassembler.

**Everything below is stated with its evidence.** Where something is
inferred rather than read directly, it says so. Do not treat the inferred
parts as settled: writing a wrong register on a PD sink controller can
request a voltage the board is not built for, and this one feeds a 5 nm
ASIC.

---

## The device

| | |
|---|---|
| Part | HUSB238A, USB-PD sink controller |
| I²C address | **0x42** — observed answering on the bus, and the only device there besides the TMP75 |
| I²C bus | SDA GPIO44, SCL GPIO43 — the same bus as everything else |
| Driver tag | `"HUSB238A"`, passed to `hammer_i2c_add_device()` |

It is the device this repository previously logged as an unidentified 0x42.
It ignores the PMBus register pointer and returns a rolling byte stream
because it is not a regulator.

## Driver surface

Recovered from the `__func__` strings the error macros leave in the image:

```
husb238a_init                    husb238a_get_connect_status
husb238a_set_voltage             husb238a_get_negotiated_voltage
husb238a_gate_open               husb238a_gate_close
husb_soft_enable
```

## Registers

| Register | Use | How it was established |
|---|---|---|
| **0x0E** | VBUS gate control | Read-modify-written by `gate_open` / `gate_close` |
| **0x63** | Connect status | `movi a8, 99` then `s8i a8, a1, 16` in `get_connect_status` |
| **0x67** | Negotiated voltage | `movi a8, 103` then `s8i a8, a1, 16` in `get_negotiated_voltage` |
| 0x66, 0x68, 0x6A, 0x6B, 0x6C, 0x6D | Adapter capability PDOs | Read in sequence by the capability scan, matching the log line `"Adapter supported voltages:"` |
| 0x08 (bit) | Soft enable | `husb_soft_enable` ORs 0x08 into a register it reads first |

Values such as `movi a8, 201` and `movi a8, 192` that sit immediately
before a `__func__` load are `__LINE__` arguments to the logging macro, not
register data. They are easy to mistake for constants and are not.

## The gate

This is the part that matters, and it is read directly rather than inferred.
`husb238a_gate_open` at `0x4205d30c`:

```
movi   a7, 14          ; register 0x0E
addi   a11, a1, 16     ; tx buffer
addi   a13, a1, 18     ; rx buffer
s8i    a7, a1, 16      ; tx[0] = 0x0E
callx8 -> i2c_master_transmit_receive     ; read one byte, 100 ms timeout

l8ui   a8, a1, 18      ; the byte just read
movi   a9, 223         ; 0xDF, i.e. ~0x20
and    a8, a8, a9      ; CLEAR bit 0x20
s8i    a7, a1, 16      ; tx[0] = 0x0E
s8i    a8, a1, 17      ; tx[1] = modified value
callx8 -> i2c_master_transmit             ; write two bytes
```

`husb238a_gate_close` is byte-for-byte the same shape at the equivalent
offset, differing in one field of one instruction:

```
gate_open   82 01 12 | 92 a0 df | 90 88 10      op2 = 1  -> AND
gate_close  82 01 12 |    ...   | 90 88 20      op2 = 2  -> OR
```

So, on register 0x0E:

- **clear bit 0x20 → VBUS on**
- **set bit 0x20 → VBUS off**

The bit is active-low: it is a disable, not an enable.

## Transaction shape

Every access follows the same pattern, which is worth copying exactly:

- **Read**: `i2c_master_transmit_receive(dev, &reg, 1, &value, 1, 100)`
- **Write**: `i2c_master_transmit(dev, buf, 2, 100)` with `buf = {reg, value}`
- Timeout is 100 ms throughout (`movi a15, 100` / `movi a13, 100`)

## Bring-up order

Inferred from the log strings and call structure, not read as a single
sequence:

1. `husb238a_init` — register the I²C device
2. `husb_soft_enable` — read a register, OR 0x08, write back
3. Read the adapter's capability PDOs — `"PD Negotiation: Read adapter capability first, 5-15V"`
4. `husb238a_set_voltage` — request one of the advertised voltages
5. Poll `husb238a_get_negotiated_voltage` / `get_connect_status` until settled, or time out — `"%dV negotiation timeout"`, `"Global negotiation timeout"`
6. `husb238a_gate_open` — clear bit 0x20 in 0x0E, `"VBUS output enabled"`
7. Only now can the TPS546 and EMC2302 be reached

A running BC01 reports `voltage: 14875` against `nominalVoltage: 12`, so
this board negotiates roughly 15 V and draws about 24 W, near 1.6 A.

## What is not yet established

- **The register `husb_soft_enable` touches.** The bit (0x08) and the
  read-modify-write are read directly; the register number is held in `a5`
  from earlier in the function and was not traced.
- **`husb238a_set_voltage`'s encoding.** The function branches over
  constants 0x08, 0x1C, 0x30, 0x60 to select a PDO, but the mapping from
  those to volts was not resolved.
- **The capability decode.** A `code * 20 + 500` calculation appears near
  the `"CONTRACT CURRENT %d."` log, which looks like a current decode, but
  the units were not confirmed.

None of these block opening the gate. They matter for choosing a voltage
deliberately rather than accepting whatever the adapter defaults to, so
they should be resolved before `set_voltage` is implemented.

## Implementing this safely

Sequence the work so that a mistake cannot request the wrong voltage:

1. **Read-only first.** Add the device at 0x42 and log registers 0x63,
   0x67 and 0x0E on a running miner. Compare against what stock firmware
   reports through `/api/system/info`. If the negotiated voltage read here
   does not match, stop.
2. **Then the gate alone.** Clear bit 0x20 in 0x0E and re-run the I²C
   scan. Success is the TPS546 at 0x24 and the EMC2302 at 0x2e appearing.
   This changes no voltage — it switches through whatever the adapter has
   already agreed to.
3. **Negotiation last**, once the rest is proven, and only after the
   capability decode above is resolved.

Step 2 is the one that unblocks mining. Steps 1 and 3 are what keep it
honest.
