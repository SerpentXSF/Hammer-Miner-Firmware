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

## Ruled out, with the measurement that ruled it out

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

## What would settle it

Software probing from the module has been exhausted. What remains needs
either a meter or the vendor's source:

1. **Measure the hashboard rail** with each module fitted. That separates
   "the module fails to enable the rail" from "the module fails to connect
   to it", which no amount of firmware probing can distinguish.
2. **Check header continuity** on the replacement module. I²C on 43/44 and
   the display work, so it is seated and oriented correctly, but a single
   pin not making contact would produce exactly this.
3. **The BC01 source.** The pin map and power sequencing are in firmware
   that was never released — `BC01-APP-2.0.3-20260625.zip` is 392 bytes
   containing a two-line README. Had the vendor published the BC01 source
   as the GPL requires, this would be a five-minute fix rather than an open
   question. See [PROVENANCE.md](PROVENANCE.md#4-the-bc01-release-is-empty).

---

## Diagnostics left in the tree

Kept because they earned their place, and because anyone porting to another
BC board will need them:

| Function | Purpose |
|---|---|
| `hammer_i2c_scan()` | Log every address that acknowledges. Found the TMP75 at 0x49 where the header claimed 0x48. |
| `hammer_i2c_identify()` | Read PMBus and low registers from each device found. Confirmed the TMP75 and excluded 0x42 as a regulator. |
| `hammer_gpio_pullup_survey()` | Distinguish external pull-ups from floating pins. Inputs only; drives nothing. |
| `hammer_gpio_measure()` | ADC a pin against the internal pull-up, to tell a pull-down resistor from a driven output before considering driving it. |
