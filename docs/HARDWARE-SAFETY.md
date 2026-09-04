# Hardware safety

What in this firmware stands between a fault and a damaged board, how each of
those protections has actually been tested, and the ones that were found not
to work.

This exists because the question was asked directly: if somebody flashes this
and their miner dies, that is on us. What follows is the honest answer, kept
current rather than written once. Where a protection is unverified, it says
so.

Companion documents: [SECURITY.md](SECURITY.md) covers information security --
authentication, OTA integrity, shipped credentials. [KNOWN-ISSUES.md](KNOWN-ISSUES.md)
carries the full history of each defect. This file is the safety-relevant
subset in one place.

---

## 1. What protects the hardware

| Protection | Where | Trigger | Response |
|---|---|---|---|
| Hashboard over-temperature | `health_maintennance.c` | board temp > `MAX_HASHBOARD_TEMP` (71 C) | power off, fan 100%, wait 100 s, restart |
| Control-board over-temperature | `health_maintennance.c` | ESP32 temp > `MAX_ENV_TEMP` (55 C) | same |
| **Unreadable temperature** | `health_maintennance.c` | 3 consecutive failed sensor reads | same |
| Fan fault | `health_maintennance.c` | `check_fan_ok()` fails 5 times running | same |
| Core voltage range | `TPS546_set_vout()` | request outside `[VOUT_MIN, VOUT_MAX]` | refused, error returned, nothing written |
| Core over-voltage | TPS546 hardware | `VOUT_OV_FAULT_LIMIT` | regulator's own fault response |

All four software paths converge on `miner_protection_handler()`: cut the core
rail, fan to 100%, hold 100 seconds, restart.

### The voltage path is sound

`VCORE_set_voltage()` goes through `TPS546_set_vout()`, which rejects any
value outside the configured window and returns an error rather than writing
it. The regulator additionally has `VOUT_OV_FAULT_LIMIT` programmed at init,
so an over-voltage would have to defeat both a software clamp and a hardware
limit. **No configuration value, API call or corrupt NVS entry can drive an
ASIC out of its voltage range.**

`TPS546_VOUT_MAX` is 520 on a BC04 -- 5.20 V across four series domains, or
1.30 V per chip. That is the vendor's own value from
`sdkconfig.bc04-reference`, and it sits at the top of the BM1370's stated
0.65-1.30 V window rather than below it. Worth knowing; not something this
project chose.

---

## 2. Defects found, and what they meant

### 2.1 A failed temperature read looked like a cold board (fixed 2026-09-04)

**The most serious thing found so far. Present in every release up to and
including 2.0.19.**

`TMP75_read_temperature()` reports a failed I2C read as **-60 C**.
`read_hash_board_temperature()` stored that value and returned `ESP_OK`
unconditionally, so nothing upstream could tell a dead sensor from a very cold
board. Both consumers of the number then ran the wrong way:

* **Thermal protection:** `-60 > 71` is false. Never trips.
* **Fan curve:** `-60 <= MIN_FAN_TEMP (30)` puts the fan at
  `MIN_PWM_PERCENT`, **18%**.

So a sensor or bus failure made the firmware **cool the board less, while it
kept hashing at full power, with nothing watching the temperature.** A fault
should push a miner toward safety; this pushed it the other way.

**Exposure differs by board.** On a BC04, fan RPM is read over the same I2C
bus, so a total bus loss aborted the health loop and rebooted -- crude, but it
stopped the mining. The exposure there is a *partial* failure: the TMP75 at
0x48 dead while the EMC2302 at 0x2e still answers. A **BC01 reads fan RPM from
a pulse counter**, not I2C, so nothing aborts: it would sit at 18% fan,
blind, indefinitely. **BC01 is the more exposed board.**

**Origin: inherited.** The -60 sentinel, the fan curve and the overheat
comparison all come from upstream. Stock vendor firmware very likely carries
the same behaviour. That changes nothing about the obligation -- it ships in
this firmware, so it is fixed here.

**Fix.** `TMP75_get_temperature()` reports failure.
`read_hash_board_temperature()` returns an error when a sensor does not
answer. The health loop counts consecutive failures and after three -- about
six seconds -- takes the same exit as an overheat. Three rather than one,
because a single dropped I2C transaction is not a dead sensor, and shutting a
working miner down over one is its own kind of harm. The reading is still
stored so the web interface has something to show; the return value is what
callers act on.

**Verification status: not verified on hardware.** It cannot be exercised on
the BC04 here, whose I2C bus is shorted: the whole health loop is gated behind
`interface_initalized`, which that board never sets. Proving it needs a
working board with an induced sensor failure. Until then it is reasoning and a
clean build, not evidence.

### 2.2 Ethernet was unreachable exactly when it was needed (fixed 2026-09-04)

Not a damage path, but a safety-relevant availability one.

`init_all_peripherals()` talks to the hashboard over I2C, and on a board whose
bus is dead it does not return -- measured at over four minutes on the BC04
here. Everything after that call is unreachable, including the Ethernet start.
A miner in that state with no WiFi credentials has **no management interface
at all**: no API, no web interface, nothing but USB.

Fixed with a watchdog that starts Ethernet if init has not finished in 90
seconds. It waits on `interface_initalized` rather than a bare timer, because
a W5500 that is already running when the core rail steps up stops answering
for good -- so Ethernet starts either after the hashboard is up, or after the
hashboard is known not to be coming up, and never in between.

**Verified on hardware**, on the failed BC04, with and without WiFi.

### 2.3 Switching off an access point that was never on (fixed 2026-09-04)

`wifi_softap_off()` called `ESP_ERROR_CHECK(esp_wifi_set_mode(...))`. With
`wifi_on=0` there is no WiFi stack, `esp_wifi_set_mode()` answers
`ESP_ERR_WIFI_NOT_INIT`, and `ESP_ERROR_CHECK` aborted the miner.

On an Ethernet-only board that is a reboot loop with no way in: come up, take
a lease, try to tidy away an access point that does not exist, panic, repeat.
Introduced into a reachable path by the 2.2 fix and **caught the same day by
testing that configuration** rather than assuming it worked. The underlying
abort was latent before that for any `wifi_on=0` configuration.

Both toggles now treat an absent WiFi stack as a configuration, not an error.

**Verified on hardware**, Ethernet-only on the BC04.

---

## 3. What a reboot loop does and does not do

Worth stating plainly, because reboot loops look alarming and the question of
whether they damage hardware is a fair one.

`network_init()` runs at `main.c:452`. `init_all_peripherals()` runs at
`main.c:499`, and `power_on_hashboard()` is inside it. **Every restart raised
from the network stage therefore happens before the ASICs or the core
regulator are ever energised.** That covers:

* `restart_with_reason("WiFi connection timeout")` -- the 90-second give-up in
  `network_wifi_connect()`
* `restart_with_reason("Network configuration timeout")`
* the 2.3 panic above

A miner cycling on any of those is not power-cycling its hashboard, not
thermally cycling its ASICs, and not stressing its VRM. It is annoying and it
must be fixed, but it is not a hardware-damage path.

**Reboots that happen *after* `interface_initalized` do cycle the core rail** --
a panic in the health loop, an overheat trip, an ASIC detect failure. Those are
genuine power cycles. Development and bring-up produce far more of them than
normal use: the BC04 here took eight panics in ten boots from the LEDC fan
defect before that was fixed, each one after the hashboard had been powered.
Dozens of extra power cycles is not a demonstrated damage mechanism, but it is
more cycling than a deployed miner sees, and it is worth counting rather than
dismissing.

---

## 4. Verification status

Honest summary. "Verified" means observed on hardware, not merely built.

| Protection | Status |
|---|---|
| Voltage clamp refuses out-of-range | Verified by code path; never observed firing |
| Hardware `VOUT_OV_FAULT_LIMIT` | Set at init; never observed firing |
| Over-temperature trip at 71 C | **Not verified.** Never reached -- peak observed on a BC04 was 65 C chip / 73 C VR at 106 W |
| Unreadable-temperature trip (2.1) | **Not verified on hardware.** Needs a working board with an induced sensor failure |
| Fan-fault trip | **Not verified** |
| Ethernet stall watchdog (2.2) | Verified on a BC04 with a dead I2C bus, with and without WiFi |
| Absent-WiFi-stack tolerance (2.3) | Verified on a BC04, Ethernet only |

The unverified rows are not claims that the protection works. They are
protections whose trigger has not been reached on hardware here.

---

## 5. If you are running this firmware

* The thermal protections depend on the I2C temperature sensor. Up to and
  including **2.0.19**, a sensor failure disabled them silently and dropped
  the fan to 18%. If you are on 2.0.19 or earlier, that is worth knowing --
  particularly on a **BC01**, where nothing else catches it.
* Watch for a reported hashboard temperature of **-60 C**. On any release it
  means the sensor is not answering. On 2.0.19 and earlier the firmware will
  not act on it; on later builds it powers down after about six seconds.
* An observed temperature of 0 in the API means the hashboard was never
  initialised, which is different from -60 and usually means the I2C bus is
  gone entirely. `docs/BOARDS.md` covers reading an empty bus scan.

---

## 6. Reporting

Hardware-safety defects are treated as higher priority than features and are
disclosed here whether or not they originated in this project. If you find one,
open an issue. If it is exploitable rather than merely dangerous, see
[SECURITY.md](SECURITY.md) first.
