# Credits

This firmware exists because people gave their work away first.

None of it started here. A commercial miner was built on it, sold, and shipped
closed — and the reason that could be corrected at all is that the original
authors chose a licence that made the work impossible to take private. This
page names them, because [NOTICE.md](../NOTICE.md) satisfies the licence and
that is not the same thing as saying thank you.

---

## The line this firmware descends from

### Valerio Vaccaro — HAN

The Italian developer behind [**HAN**](https://github.com/valerio-vaccaro/HAN),
an ESP32 speaking Stratum to a solo pool. It is the origin of the idea that a
microcontroller costing a few dollars could mine Bitcoin on its own terms, and
the root that the portable ESP32 solo miners grow from. Vaccaro also maintains
the browser-based [DIY flasher](https://valerio-vaccaro.github.io/diyflasher/)
that put Jade, NerdMiner and HAN Solo within reach of people who had never
touched a toolchain.

Every ESP32 solo miner since is downstream of that first one.

### BitMaker — NerdMiner

[**NerdMiner_v2**](https://github.com/BitMaker-hub/NerdMiner_v2) built on HAN
and turned it into something thousands of people actually run. BitMaker's own
[ESP32_NerdMiner](https://github.com/BitMaker-hub/ESP32_NerdMiner) is
described in its own words as "a NerdSoloMiner using > Han miner" — the
lineage stated plainly by the person who wrote it, which is how it should be.

The multi-page display this firmware inherits, cycled by a single button, is
recognisably from that tradition.

### Skot (skot9000) — Bitaxe and ESP-Miner

[**Bitaxe**](https://github.com/skot/bitaxe) and
[**ESP-Miner**](https://github.com/bitaxeorg/ESP-Miner). Rather than build
another proprietary miner, Skot reverse-engineered Bitmain's ASICs and
published a complete open-source reference design that anyone could copy.

ESP-Miner is the direct upstream of this firmware. The task model, the Stratum
client, the mining pipeline, the ASIC drivers, the NVS configuration layer, the
self-test and the web UI are all his work and the work of the contributors who
followed. Measured against upstream, 41 files still match and 4,367 lines are
identical — see [PROVENANCE.md](PROVENANCE.md).

### Benjamin Wilson (@ben) — Open Source Miners United

Co-founder of **Open Source Miners United**, the community that carries the
firmware. Skot has described meeting someone on Bitcoin Talk who started the
OSMU Discord; it grew past 4,000 people and became where the work actually
happens. A project outliving its author is a community's doing, not a
repository's.

### @jhonny — core firmware

Credited alongside Skot and Ben for the foundational mining protocol work in
ESP-Miner — the layer everything else stands on and almost nobody sees.

### Open Source Miners United

The community maintaining ESP-Miner and AxeOS through the
[bitaxeorg](https://github.com/bitaxeorg) organisation. Bug reports, ASIC
bring-up, board revisions, translations, and the patience to answer the same
question again for someone new.

### shufps and the NerdQAxe+ contributors

[**ESP-Miner-NerdQAxePlus**](https://github.com/shufps/ESP-Miner-NerdQAxePlus),
origin of the InfluxDB telemetry component carried here.

### D-Central

Raised the licensing problem [publicly](https://d-central.tech/hammer-miner/)
before this repository existed, which is the reason any source was released at
all. Their [DCENT_OS](https://github.com/DCentralTech/DCENT_OS) independently
reached the same BC01 conclusions this work did — UART TX 18 / RX 17 "the
OPPOSITE of BC04", TMP75 at 0x49, HUSB238A as the PD controller — from their
own disassembly, with no contact between the two efforts. Two teams agreeing
from separate evidence is worth more than either alone.

### Chengdu Baichuan

The BC01 release contains the USB Power Delivery stage the BC04 release
omitted. Without `HUSB238A.c` the hashboard rail never comes up and a BC01
cannot mine at all. That file is imported here verbatim and credited. What the
release got wrong is set out in [FINDINGS.md](FINDINGS.md); this part it got
right, and both belong in the record.

---

## Also standing underneath this

| | |
|---|---|
| [LVGL](https://github.com/lvgl/lvgl) | LVGL Kft. — the display stack, MIT |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Espressif — RTOS, drivers, build system, Apache-2.0 |
| blockchair, blockcypher, CoinGecko | Public APIs behind the network-stats and price screens |

---

## Corrections welcome

These credits were assembled from public sources and from what people who were
there have said. Roles in a community project are rarely tidy, and anyone
misattributed, given someone else's work, or left out entirely should say so —
[open an issue](https://github.com/SerpentXSF/Hammer-Miner-Firmware/issues) and
it gets fixed. Getting this page wrong is worse than not writing it.
