# Copyright and attribution

This firmware is a derivative work of **ESP-Miner**, released under the
GNU General Public License version 3. The full license text is in
[LICENSE](LICENSE). Every copy or modified version of this firmware must
be distributed under the same terms, with complete corresponding source.

## Upstream projects

| Project | Author | License | Relationship |
|---|---|---|---|
| [ESP-Miner](https://github.com/bitaxeorg/ESP-Miner) | Bitaxe / Skot and contributors | GPL-3.0 | Primary upstream. Task model, Stratum client, mining pipeline, ASIC drivers, NVS config layer, self-test, and the web UI all derive from it. |
| [NerdQAxe+](https://github.com/shufps/ESP-Miner-NerdQAxePlus) | shufps and contributors | GPL-3.0 | Origin of the InfluxDB telemetry component. |
| [LVGL](https://github.com/lvgl/lvgl) | LVGL Kft. | MIT | Display library, fetched as an unmodified dependency. |
| [ESP-IDF](https://github.com/espressif/esp-idf) | Espressif Systems | Apache-2.0 | Build system, RTOS, and drivers. |
| BC04 firmware | Chengdu Baichuan, for Hammer | GPL-3.0 | The vendor release this repository is built from. |

## Why this file exists

The vendor's release at `github.com/baichuan-org/BC04` shipped **no license
file and no attribution of any kind**. GPL-3.0 section 4 requires that
anyone conveying the work also convey the license text and keep all
copyright notices intact. Publishing the source alone does not satisfy
that; the notices have to travel with it.

This file, [LICENSE](LICENSE), and [docs/PROVENANCE.md](docs/PROVENANCE.md)
restore what should have accompanied the original release.

## Attribution is not a formality here

ESP-Miner is the work of independent developers who chose the GPL
specifically so that hardware vendors could not take their work private.
A commercial product was built on it, sold, and shipped closed-source.
The vendor's own README describes this as an oversight discovered during
a compliance audit.

Whatever the intent, the practical effect was that the people whose work
made this product possible were not credited to its buyers. Naming them
is the point of the license.

## Scope of this repository's own changes

Changes made here — security fixes, the BC01 port, the reimplemented ASIC
abstraction layer, tooling, and documentation — are likewise GPL-3.0, and
are offered back under the same terms.
