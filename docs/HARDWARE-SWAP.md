# Running this firmware on a locked-down miner

Retail BC-series miners ship with Secure Boot enforced and both spare key
slots revoked, so they will only boot Hammer-signed images and that cannot
be changed. See [SECURE-BOOT.md](SECURE-BOOT.md).

There is a way around it that needs no exploit, because of how the hardware
is built: **the ESP32-S3 is a socketed, off-the-shelf module.** Secure Boot
lives in that chip's eFuses. Replace the module and you replace the lock.

This is a reversible, roughly $20 change. Keep the original module and you
can put the miner back to stock in a minute.

---

## Why the miner survives this

The board splits cleanly. The parts that are hard to replace stay put.

| Stays on the miner | Moves with the module |
|---|---|
| BM1370 ASIC and its hashboard | Firmware |
| TPS546 core regulator, TMP75 sensor, EMC2302 fan controller | Settings in NVS: pool, WiFi, **frequency and voltage** |
| Hashboard I²C EEPROM (`chip_bin`, `pcb_version`, binning) | MAC address, and the serial string derived from it |
| Power input and fan | |

### Tuning does not carry over — write it down

The hashboard EEPROM does hold per-unit data, and `eeprom.c` provides
`get_voltage_from_eeprom()`, `get_freq_from_eeprom()` and
`get_bin_from_eeprom()` to read it. **Nothing in the shipping firmware
calls them.** Outside the unit tests they have no callers, so that data is
present on the board and unused.

Frequency and voltage come from NVS, through `SYSTEM_update_freq_voltage()`:

```c
GLOBAL_STATE->asic_freqency = nvs_config_get_u16(NVS_CONFIG_ASIC_FREQ, CONFIG_ASIC_FREQUENCY);
uint16_t uint_voltage = nvs_config_get_u16(NVS_CONFIG_ASIC_VOLTAGE, GLOBAL_STATE->asic_vol_default);
```

NVS lives on the module. So a fresh module comes up on **compile-time
defaults, not your board's tuning**, and you have to put the values back
yourself. Capture them before you pull the original — see
[Before you remove anything](#before-you-remove-anything). This is the step
people will skip, and it is the one that matters.

---

## The module

The host is a **LilyGO T-Display-S3**. The firmware identifies it beyond
doubt — `main/displays/lilygo_porting.h` sets a 170×320 ST7789 on an 8-bit
i80 parallel bus, with `PWR` on GPIO15 and `RD` on GPIO9, which is that
board's pinout and no other.

### Get the right variant

You need the **standard T-Display-S3**, 1.9-inch **ST7789**, **170×320**,
parallel interface, **16 MB flash / 8 MB PSRAM**.

**Do not buy the AMOLED versions.** T-Display-S3 AMOLED and AMOLED Plus use
a completely different panel and controller (536×240 over QSPI) and this
firmware will not drive them. T-Display-S3 Long is 180×640 and also wrong.

The Touch variant uses the same ST7789 panel and should work with the touch
hardware simply unused, but the plain version is the safe choice.

Get one with **headers already soldered** unless you want to solder them
yourself — the carrier board mates through two female headers.

16 MB flash is not optional: `partitions.csv` runs to `0xff0000`.

---

## Before you remove anything

Record your settings while the miner still runs:

```bash
curl -s http://<miner>/api/system/info | tee my-miner-settings.json
```

Keep that file. These have to go back in by hand:

```
DeviceModel      BC01          -> devicemodel
boardVersion     V02           -> boardversion
ASICModel        BM1370        -> asicmodel
frequency        750           -> asicfrequency, asicoverf
coreVoltage      119           -> asicvoltage, asicovervol
                               -> asicovervdef  (the fallback default)
boot_mode        1
fanspeed / autofanspeed
stratumURL / stratumUser / stratumPort
ssid
```

Voltages are in units of 10 mV, so 119 is 1.19 V. Note how much lower that
is than the multi-ASIC boards, whose core domain is a series string —
`config.cvs.example` ships 470 because it is written around a BC04. **Do
not leave a BC04 voltage on a BC01.**

If the device is already dead and you never captured this, the console log
prints the values at every boot (`asic frequency`, `asic voltage`,
`vol def`), so an older log or a photo of the screen may still have them.

`sn_str` does not need restoring. The serial is built at runtime from a
prefix plus the model and MAC, so the new module generates its own.

---

## Swapping

The module sits in two female headers. Lift it straight up, evenly, and
keep it — it is your rollback.

Note which way round it sits before removing it. Fitting it reversed will
short 5 V into a signal pin.

Fit the replacement the same way round, seated fully, with no pins missed
or bent under the body.

---

## Flashing the new module

Do this before fitting it, over the module's own USB-C.

Build and provision as in the [README](../README.md):

```bash
cp config.cvs.example config.cvs
$EDITOR config.cvs        # devicemodel, boardversion, pool, YOUR payout address
idf.py build
./merge_bin.sh -c hammer-miner-all.bin
```

Set at minimum, from what you captured:

```
devicemodel,data,string,BC01
boardversion,data,string,V02
asicmodel,data,string,BM1370
stratumuser,data,string,<your address>
apipassword,data,string,<choose one>
```

Then flash the whole image:

```bash
esptool.py --chip esp32s3 --port COM3 write_flash 0x0 hammer-miner-all.bin
```

A blank module has unburned eFuses, so this just works. Nothing needs
unlocking, and nothing is burned — leave it that way unless you have read
[SECURE-BOOT.md](SECURE-BOOT.md) and mean it.

---

## First boot

Fit the module, apply power, and watch the console on its USB-C
(`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, so logs come out that port):

```bash
idf.py -p COM3 monitor
```

What should appear:

```
nvs_device: DEVICE Model: BC01
nvs_device: ASIC Model: BM1370
nvs_device: Board Version: 2
device:     Initialize all the i2c dev.
device:     FAN PWM ...%, RPM ...
asic:       Model BC01, Detect 1 asic.
```

Then check it over HTTP:

```bash
curl -s http://<miner>/api/system/info
```

`asicCount` should be **1** on a BC01, `hashRate` should climb over the
first minutes, and shares should start being accepted.

### If the ASIC is not detected

`Detect 0 asic` means the UART to the BM1370 is not working. Check the
module is fully seated and the right way round. The firmware retries ten
times before giving up, so a persistent zero is wiring, not timing.

### If the screen shows a power or PSU fault

Check what firmware is actually running before suspecting the hardware.
Replacement modules are sometimes sold pre-flashed for a different miner --
a NerdQAxe++ image on this hardware reports `cannot find TPS53647 buck
controller` (the BC boards use a TPS546), `Failed to read temperature from
TMP1075` (they use a TMP75), and `0 chip(s) detected on the chain, expected
4`, which surfaces on screen as a power fault. The board is fine; the
firmware is looking for a different one. Flash this firmware over it.

### If the display is blank but mining works

You have an incompatible panel — most likely an AMOLED variant. Mining is
unaffected, but the display will not come up without a different porting
layer.

---

## Rolling back

Power off, swap the original module back in. It is untouched, still
Hammer-signed, and boots stock firmware with its own NVS settings intact —
your original tuning is still on that module, which is the other reason to
keep it. That is the point of doing it this way rather than attacking the
original: failure costs you nothing.

---

## One thing you lose

Your original module enforces Secure Boot. **The replacement will not**,
unless you deliberately provision it. That is a real reduction in the
device's defences, even though it is what makes the swap possible.

It matters less than it sounds, because Secure Boot never protected the
thing most worth attacking: pool configuration lives in NVS, which no
signature covers, so a stock locked-down miner is just as exposed to having
its payout address changed as this one. See
[SECURITY.md](SECURITY.md#2-unauthenticated-ota-with-obfuscation-standing-in-for-integrity).

So: **set an API password** (`apipassword` above), and keep the miner off
untrusted networks. That closes the hole that actually gets exploited. If
you want signature enforcement back on your own key,
[SECURE-BOOT.md](SECURE-BOOT.md) covers it — read the warnings first, since
burning eFuses is permanent and will lock you out of your own hardware if
you lose the key.

---

## Using a different ESP32-S3 board

Nothing above requires a T-Display-S3 specifically; it is just what the
carrier board is wired for. The miner needs remarkably little from its
host — everything else in `sdkconfig` is `255`, meaning unused:

| Signal | GPIO | Goes to |
|---|---|---|
| UART TX | 17 | BM1370 |
| UART RX | 18 | BM1370 |
| I²C SDA | 44 | TMP75, EMC2302, TPS546, hashboard EEPROM |
| I²C SCL | 43 | same bus |
| ASIC reset | 1 | hashboard |

Five signals, plus 5 V and ground.

Any ESP32-S3 board with 16 MB flash, 8 MB PSRAM and those pins available
can drive the miner, though you will need to adapt the display porting
layer in `main/displays/` or build without a display. Note that GPIO43 and
GPIO44 are the chip's default UART0 console pins and are used here for I²C
instead — the console is on USB-Serial/JTAG, which is why that works.
