# BC01 bring-up: what works, and one open problem

Record of bringing this firmware up on a BC01 whose stock ESP32-S3 module
was replaced (see [HARDWARE-SWAP.md](HARDWARE-SWAP.md)). Most of it works.
One thing does not, and the measurements that rule out the obvious causes
are written down here so nobody repeats them.

---

## Working, verified on hardware

| | |
|---|---|
| Boot, no loop | `nvs_device: DEVICE Model: BC01`, ASIC model and board version correct |
| Display | Full LVGL stack and theme on a T-Display-S3 |
| WiFi + AP provisioning | Connects; captive portal serves when unconfigured |
| NTP | `rtc_sync: ntp sync done` |
| **API authentication** | Unauthenticated `GET /api/system/info` → **401**; login returns a 64-char token; authenticated request succeeds; `authEnabled: true` |
| TMP75 | Reads correctly once the address was corrected to 0x49 |
| HTTP API | Serves `/api/system/info` with correct `DeviceModel`, `asicCount`, `frequency` |

## Not working

The BM1370 never starts, because the core regulator is unreachable:

```
device: Initialize all the i2c dev.
EMC2103: initializing EMC2302 : 0
hammer-i2c: Device EMC2103 (0x2e)       <- NACK
vcore: TPS546 power good 0
TPS546: Initializing the core voltage regulator
hammer-i2c: Device TPS546 (0x24)        <- NACK, retries indefinitely
```

The fan does not spin and the hashboard rail never comes up.

---

## The shape of the problem

Two devices answer on the I²C bus. Two others, on the same two wires,
never do:

```
hammer-i2c: Scanning I2C bus (SDA=44 SCL=43)
  device responding at 0x42
  device responding at 0x49
  2 device(s) found
```

`0x49` is unambiguously a TMP75 — it returns the factory default limits,
so it is answering, not merely acknowledging:

```
reg0x00 = 1a b0   26.69 C
reg0x02 = 4b 00   T_LOW  75 C
reg0x03 = 50 00   T_HIGH 80 C
```

`0x42` is not a PMBus part: every PMBus register reads zero, and the low
registers return a single byte stream shifted one position per read
(`40 47` / `47 13` / `13 00` / `00 20`), so it does not honour the register
pointer. Unidentified.

**The same hardware works perfectly with its original module and stock
firmware** — 1384 GH/s, fan at 4531 RPM, `coreVoltageActual` 1195.31 mV,
shares accepted. So the carrier, hashboard, regulator, fan and PSU are all
good, and the fault is on the replacement module's side of the connector.

---

Recording the dead ends, because each cost a hardware cycle.

**Insufficient supply current.** A PC USB port genuinely cannot run this
miner, but that was not the cause: the bus is identical on a proper PSU.

**Firmware never reaching the power-on path.** It does.
`init_all_peripherals()` runs and logs its attempts. Note that it sits
behind `network_init()`, which blocks until the device has an IP, so an
unconfigured miner never powers its hashboard at all — expected behaviour,
easily mistaken for a fault.

**A second I²C bus.** The BC04 configuration this tree inherits leaves bus
1 disabled, so a device on another pin pair would be invisible. A pull-up
survey settles it — an I²C line always carries a pull-up, and reading each
pin against the chip's own pull-up and pull-down separates external from
floating:

```
GPIO2   pu=1 pd=1  EXTERNAL PULL-UP
GPIO3   pu=1 pd=0  floating
GPIO10  pu=0 pd=0  driven low externally
GPIO11  pu=1 pd=0  floating          <- vcore.c reads this as "power good"
GPIO12,13,14,16,21 floating
GPIO43  pu=1 pd=1  EXTERNAL PULL-UP  (known SCL)
GPIO44  pu=1 pd=1  EXTERNAL PULL-UP  (known SDA)
```

Only 43/44 form a pulled-up *pair*. There is no second bus. GPIO2 alone is
most likely SMBALERT — `TPS546_I2CADDR_ALERT` exists in the header.

Off the miner, every pin reads floating and no I²C device answers, which
confirms both that the survey is meaningful and that 0x42 and 0x49 are on
the carrier rather than the module.

**GPIO10 as a hashboard enable.** It is held low, and an ADC measurement
against the internal pull-up gives **473 mV**, implying an external
pull-down near 7.5 kΩ — an input defaulted off, not a driven output.
Plausible enable, so it was driven high at minimum drive strength. **No
effect on the bus.** Not the enable.

**Hashboard held in reset.** `reset_hash_board()` runs after `VCORE_init()`,
which cannot complete while the regulator is unreachable, so GPIO1 is
undriven at scan time. Releasing it before the scan had **no effect**.

**`vcore: TPS546 power good 0` is not evidence of anything.** That reads
GPIO11, which the survey shows is floating, with no pull configured. It is
reading an unconnected pin.

---

## Root cause: the BC01 is USB-PD powered, and this firmware does not negotiate

Found by reading the strings in the decrypted stock 2.0.3 image. Its
`./main/device.c` contains a subsystem the BC04 release has no trace of:

```
0x0071d4  husb238a_init()
0x0071e4  ./main/device.c
0x0071f4  PD Negotiation: Read adapter capability first, 5-15V
0x007238  attached=%d, ready=%d, volt=%dV, fault=%d, target=%dV
0x0072a8  Adapter supported voltages:
0x0072e8  Negotiation success: %dV
0x0073ac  CONTRACT CURRENT %d.
0x0073d0  VBUS output enabled
0x007430  VBUS output disabled
```

with this driver, recovered from the `__func__` strings the error macros
leave behind:

```
husb238a_init          husb238a_get_connect_status
husb238a_set_voltage   husb238a_get_negotiated_voltage
husb238a_gate_open     husb238a_gate_close
husb_soft_enable
```

and an I²C device registered under the tag `HUSB238A`.

The HUSB238A is a USB Power Delivery sink controller. **It is the
unidentified device at 0x42** — which is why it ignored the PMBus register
pointer and returned a rolling byte stream: it is not a regulator and does
not implement that access pattern.

The BC01 therefore powers its hashboard like this:

1. Read the adapter's advertised capabilities over I²C
2. Negotiate a higher supply voltage, somewhere in 5–15 V
3. Call `husb238a_gate_open()` to switch VBUS through
4. Only now do the TPS546 and EMC2302 have power and appear on the bus

This firmware performs none of that, so VBUS is never gated on and the
hashboard is dead no matter what is plugged in.

Every earlier observation follows from it:

| Observation | Explanation |
|---|---|
| TPS546 and EMC2302 never acknowledge | VBUS is off |
| TMP75 and 0x42 always answer | Both sit on the always-on rail, and 0x42 is the PD controller itself |
| A proper PSU changed nothing | Without negotiation the gate stays shut regardless of supply |
| A PC USB port changed nothing | A PC port cannot do PD negotiation at all |
| Stock reports `voltage: 14875`, `nominalVoltage: 12` | That is the negotiated PD voltage, about 15 V |
| Stock draws 24 W | ~1.6 A at 15 V, consistent |

It also explains why driving GPIO10 and releasing the chain reset did
nothing: the gate is opened over I²C, not by a GPIO.

### What is still needed

The register-level sequence. The HUSB238A has a published datasheet, but
the values this board expects — which PDO to select, what to write to open
the gate, and the order — should be read out of the shipping binary rather
than assumed, because writing the wrong register on a PD sink controller
can request a voltage the hardware is not built for.

That means locating `husb238a_init`, `husb238a_set_voltage` and
`husb238a_gate_open` in the image and reading their constants. The
`__func__` strings above give an anchor for each.

### The point worth keeping

None of this required reverse engineering to know. It is in the BC01
source that was never published. A single file describing a PD sink
controller stands between this firmware and a working miner, and
recovering it from a binary is the direct, measurable cost of the
compliance gap in [PROVENANCE.md](PROVENANCE.md#4-the-bc01-source-was-never-published).

---

## Also ruled out, with the measurement that ruled it out

## Diagnostics left in the tree

Kept because they earned their place, and because anyone porting to another
BC board will need them:

| Function | Purpose |
|---|---|
| `hammer_i2c_scan()` | Log every address that acknowledges. Found the TMP75 at 0x49 where the header claimed 0x48. |
| `hammer_i2c_identify()` | Read PMBus and low registers from each device found. Confirmed the TMP75 and excluded 0x42 as a regulator. |
| `hammer_gpio_pullup_survey()` | Distinguish external pull-ups from floating pins. Inputs only; drives nothing. |
| `hammer_gpio_measure()` | ADC a pin against the internal pull-up, to tell a pull-down resistor from a driven output before considering driving it. |
