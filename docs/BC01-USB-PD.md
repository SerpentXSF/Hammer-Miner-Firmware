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
| Driver tag | `"HUSB238A"`, passed to `bc_i2c_add_device()` |

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

## Measured on hardware

The driver above was run on a BC01 and the predictions held. The gate
register read `0x27` with the disable bit set, clearing it gave `0x07`, and
the new value survived a reset, so the part keeps its own retained state.

It was not sufficient. The same read shows why:

```
0x63 connect status     = 0x00      nothing attached
0x66..0x6d PDOs         = 0x00      no capabilities read
```

There is no contract for the gate to pass through. The adapter is the same
PD supply the miner mines on with its stock module, so this is a missing
negotiation, not a supply problem.

### Full register state, gate open, nothing negotiated

```
0x00=40 0x01=47 0x02=13 0x04=20 0x0a=90 0x0c=a8 0x0d=40 0x0e=07
0x1f=6c 0x38=e0 0x39=20 0x3a=c0 0x3d=ea 0x40=f8 0x41=7e
0x46=78 0x47=b8 0x48=40 0x49=80 0x4a=e0 0x4b=91 0x4c=af 0x4d=99
0x4e=2e 0x4f=90 0x50=23
0x51..0x5f = a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae
0x60=a1 0x61=e2 0x64=80
0x90=01 0x97=23
everything else reads 0x00
```

**0x88 reads 0x00 and is written by a helper in the stock driver**
(`movi a8, -120` then `s8i a8, a1, 0` at `0x4205d1a0`), which is the shape
of a self-clearing command register. Triggering negotiation most likely
means writing a command byte there.

This also explains an earlier misreading recorded elsewhere in this
repository: the "rolling byte stream" at 0x42 was register auto-increment
on multi-byte reads. `0x00=0x40, 0x01=0x47, 0x02=0x13` reproduces the
observed `40 47 / 47 13 / 13 00` exactly. It is an ordinary register file.

## What is still missing

**The command that starts negotiation.** Without it the controller never
attaches, never reads the adapter's PDOs, and never forms a contract, so
the gate has nothing to switch through.

Two candidates, neither confirmed:

- The command byte written to **0x88**.
- The register `husb_soft_enable` ORs 0x08 into. It is held in `a5` and
  never written inside that function, so it arrives as a parameter on a
  path a linear disassembler does not reach. A brute-force scan for
  branches into that block found none, which means the function boundary
  taken from the `ENTRY` prologue is wrong somewhere.

Also unresolved: `husb238a_set_voltage` branches over 0x08, 0x1C, 0x30 and
0x60 to pick a PDO, and a `code * 20 + 500` calculation sits near the
`"CONTRACT CURRENT %d."` log. Neither mapping was established.

### The cheapest way to finish this

The HUSB238A has a published datasheet. With the register map measured
above -- 0x88 as a command register, 0x63 status, 0x0E gate, 0x67
negotiated voltage, 0x66-0x6D capability PDOs -- anyone holding that
datasheet can complete this quickly and correctly.

That is a far better route than more disassembly. The remaining code is
branch-heavy and defeated the small decoder in `tools/xtensa_dis.py`, which
follows a linear stream and does not chase control flow.

Best of all would be the vendor releasing the BC01 source, where this is
one file.

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
