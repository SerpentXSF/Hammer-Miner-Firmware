# Known issues

Things that are understood, worked around, and worth doing properly.

## A www update needed a restart the caller had to know about (fixed)

**Status: fixed.** The handler restarts itself once the SHA256 verifies, the
way the application update always did, and `Settings.vue` no longer sends a
restart of its own.

`POST /api/system/OTAWWW` used to answer `WWW update complete` as soon as it
had written the partition, while the running server kept serving the old
mount. `WWWVersion` read empty and every page was the built-in **WWW
Recovery** screen, which looks exactly like a failed update.

Correcting what this entry said before: the web interface was never affected.
`Settings.vue` always sent its own restart after a www upload, so a user
updating through the page would not have seen this. The exposure was every
other caller -- scripts, `curl`, anything posting to the endpoint directly --
which is how it was found and how it was tested.

An *interrupted* upload is still worth knowing about: it leaves the partition
damaged and the miner on the recovery page until another upload succeeds. The
API and mining are untouched throughout -- the board kept hashing at 6.2 TH/s
with no web interface at all -- so a second upload fixes it.

Verified by posting a container straight to the API with no client restart:
the answer is now `WWW update complete, rebooting now!`, and the miner returns
on its own at 87 seconds uptime serving the real interface.

**Separately, and not explained:** a large upload to either OTA endpoint
sometimes resets mid-transfer on a BC04. Retrying works.

This was first written up as an Ethernet-specific fault, on the strength of
two resets over Ethernet and two successes over WiFi. That was too small a
sample: an application upload over *WiFi* then reset the same way, and
succeeded on the retry. It is intermittent and not tied to an interface.

Nothing is damaged either way -- a partial write fails its checksum and is
rejected, and mining carries on -- but a www upload interrupted after the
erase leaves the miner on the recovery page until a second upload succeeds.
So: **retry, and check the version afterwards.**


## BC04 Ethernet had to be started after the hashboard (fixed)

**Status: fixed** in `main/network.c` and `main/main.c` -- Ethernet is started
after `init_all_peripherals()` rather than before it. Kept here because the
wrong diagnosis stood for a while and the reasoning is worth not repeating.

A BC04 brings its W5500 up cleanly: link, DHCP lease, DNS, NTP. Then the core
regulator switches on and **70 ms later** every socket command times out, for
good -- *if the controller was already running at that moment*. That last
clause is the whole thing, and it took far too long to test.

```
I (10658) vcore: Set ASIC voltage = 4.80V
E (10728) w5500.mac: w5500_send_command(210): send command timeout
E        esp_eth: eth_on_state_changed(151): ethernet mac set link failed
```

Nothing runs in software between those two lines -- the failure lands inside a
plain `vTaskDelay`, before the ASICs are reset or clocked. The interface keeps
its address and stops passing packets, so the miner looks networked and cannot
reach a pool.

Eliminated by test, not by argument:

| Theory | Result |
|---|---|
| SPI clock too fast for the bus | 16 MHz and 8 MHz behave identically |
| Driver task starved during power-up | `volc_delay()` is a plain `vTaskDelay` |
| Supply browning out | 12.30 V to 12.125 V under 85 W -- 1.4 % |
| GPIO or SPI bus conflict | Nothing else uses SPI2; power-on only writes I2C |
| Socket wedged, restart clears it | `esp_eth_stop()` cannot even reset the PHY |

Seventy milliseconds is the switching transient itself, and the driver reports
a link *state* change rather than only failed commands. That was read as the
16 A core regulator resetting or browning out the W5500 -- a hardware fault.

**It is not.** Every experiment above disturbed a controller that was already
running; none initialised one after the transient. Doing that works and keeps
working. Five eliminated hypotheses were mistaken for a complete set, and
"therefore hardware" followed from an argument rather than a measurement.

**What a real fix would look like.** `esp_eth_start()` only reopens socket 0.
It never re-runs `emac_w5500_init()`, which is what resets the chip, writes the
MAC into `SHAR` and puts socket 0 into MACRAW -- so a controller that came back
blank stays blank no matter how many times it is restarted. Recovery has to be
a full `esp_eth_driver_uninstall()` and re-init after the rail settles, which
also means retaining the mac, phy and netif-glue handles that
`example_eth_init()` currently drops on the floor.

**Do not start after a failed stop.** A failed `esp_eth_stop()` means the
driver never emitted `ETH_EVENT_STOP`, so the netif is still attached;
`esp_eth_start()` then re-enters `esp_netif_action_start` and trips
`assert failed: netif_add (netif already added)`, panicking the miner into a
reboot loop for as long as Ethernet is enabled. This was tried on hardware and
cost nine boots.

**Verified** on hardware at 750 MHz, both with WiFi alongside and with WiFi
switched off entirely -- the latter being what a freshly flashed miner is.
Ethernet-only: 6218 GH/s, 50 accepted shares, none rejected, no hardware
errors, the API served over the Ethernet interface, and the WiFi address
correctly gone.

`eth_on` and `wifi_on` are settable through `PATCH /api/system` and reported
by `/api/system/info`, so either can be changed without a factory restore --
which matters, because a factory restore also discards the WiFi credentials
that may be the only remaining way in.

Related, and worth fixing whatever the cause turns out to be: when Ethernet
takes a lease the firmware switches the setup access point off, and never
brings it back if Ethernet later dies. That is how a BC04 ends up holding an
IP address, passing nothing, with no way to reach it.

## Deferring Ethernet left a reboot loop behind it (fixed)

**Status: fixed** in `main/network.c`. Introduced by the fix directly above,
which is the point of writing it down.

Moving Ethernet after the hashboard fixed the W5500, and broke the case where
Ethernet is the only network that works. `network_init()` ends in a loop that
waits for an address from either interface and restarts the miner after five
minutes without one. Once Ethernet is deferred, that loop cannot ever see an
Ethernet address: the interface is not started until `main()` gets past the
call it is blocking inside.

So a BC04 with a good cable in it and an SSID it cannot reach -- an access
point that moved, a password that changed, a neighbour's network it once
joined -- waits five minutes, restarts, and does exactly the same thing again.
For ever, with a working cable plugged into it the whole time. Restarting
cannot help: the restart runs the identical sequence.

The wait is now short and ends in a `return` rather than a restart whenever
Ethernet is deferred: twenty seconds, which is long enough for WiFi to finish
DHCP if it is going to, then on to start the interface that is actually
connected. WiFi keeps retrying in the background, so an access point that is
slow or briefly away still joins afterwards. Nothing changes on a board where
Ethernet was never deferred -- a BC01 takes the same five minutes and the same
restart it always did.

The five-minute backstop is kept, moved to where it can still be satisfied.
It now runs from `network_settle_task()`, started once Ethernet is up, so the
window begins *after* the interface that might supply an address has been
given the chance to. That is the whole difference: the old backstop restarted
a miner that had never tried its cable, and the restart ran the same sequence
again. This one restarts a miner that has tried both, which is a state a
restart might genuinely clear -- a DHCP server that was not up yet, a switch
port still learning, a driver that came up wrong.

It holds its clock at zero for as long as somebody is connected to the setup
access point. That is where a miner with no network gets configured from, and
restarting out from under whoever is doing it is the one thing it must not do.

The same task also finishes the job `network_init()` no longer does. **Every**
deferred path leaves that function early, so the tidy-up at the end of its
wait loop -- drop the setup access point, quieten the WiFi log, tell the
system it is connected -- had stopped running, including on the path shipped
in 2.0.19. A miner that came up on the cable kept an open access point
broadcasting for as long as it was powered, and these images ship with a blank
API password, so that access point was an unauthenticated way in. Putting both
jobs in one task means the tidy-up runs however late the address arrives,
rather than only if it arrives inside some fixed window.

It is a task rather than a loop inside `network_eth_start()` so that main() is
not held for five minutes on a board with no cable in it.

**Not verified on hardware.** The only board here with a W5500 is the BC04,
and it is out for warranty repair with a shorted I2C bus. A BC01 cannot
exercise any of this: `network_init()` forces `eth_on` to 0 on that family, so
`eth_deferred` stays false and every line above is skipped. What a BC01 does
confirm is the part that protects it -- with `eth_deferred` false the timeout
constant is the same `5*60*5` it always was and the new branch is unreachable,
so its behaviour is unchanged. The fix itself needs a BC04 to sign off.

## A failed temperature read looked like a cold board (fixed)

**Status: fixed** in `components/bc_hal/TMP75.c`, `main/device.c` and
`main/tasks/health_maintennance.c`. Present in every release up to and
including **2.0.19**. Full write-up, including exposure by board and
verification status, in [HARDWARE-SAFETY.md](HARDWARE-SAFETY.md).

`TMP75_read_temperature()` reports a failed I2C read as **-60 C**, and
`read_hash_board_temperature()` stored that and returned `ESP_OK` regardless.
Nothing upstream could tell a dead sensor from a very cold board, and both
consumers of the number ran the wrong way:

* thermal protection compares against 71 C, so `-60 > 71` is false and it
  never tripped
* the fan curve compares against `MIN_FAN_TEMP` (30 C), so `-60` put the fan
  at `MIN_PWM_PERCENT`, **18%**

A sensor or bus failure therefore made the firmware cool the board *less*
while it kept hashing at full power, with nothing watching the heat.

The BC01 is the more exposed board. On a BC04 fan RPM comes over the same I2C
bus, so a total bus loss already aborted the health loop and rebooted -- crude,
but it stopped the mining; the exposure there is a partial failure, the TMP75
at 0x48 dead while the EMC2302 at 0x2e still answers. A BC01 reads RPM from a
pulse counter, so nothing aborts and it would sit at 18% fan, blind.

Inherited from upstream, so stock firmware very likely carries it too. It
ships here, so it is fixed here.

`TMP75_get_temperature()` can now report failure, `read_hash_board_temperature()`
returns an error, and the health loop takes the overheat exit -- power off, fan
100%, restart -- after three consecutive failures, about six seconds. Three
rather than one, because a single dropped transaction is not a dead sensor.

**Not verified on hardware.** The only BC04 here has a shorted I2C bus, and the
whole health loop is gated behind `interface_initalized`, which that board
never sets. It needs a working board with an induced sensor failure.

## Switching off an access point that was never on (fixed)

**Status: fixed** in `components/connect/connect.c`, with a guard in
`main/network.c`.

`wifi_softap_off()` called `ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA))`
unconditionally. With `wifi_on=0` there is no WiFi stack, the call answers
`ESP_ERR_WIFI_NOT_INIT`, and `ESP_ERROR_CHECK` aborts:

```
ESP_ERROR_CHECK failed: esp_err_t 0x3001 (ESP_ERR_WIFI_NOT_INIT)
file: "./components/connect/connect.c" line 256
func: wifi_softap_off
```

On an Ethernet-only miner that is a reboot loop with no way in -- come up, take
a lease, try to tidy away an access point that does not exist, panic, repeat.

The abort was latent for any `wifi_on=0` configuration. What made it reachable
was the settle task added with the Ethernet stall watchdog, which drops the
setup access point once Ethernet has an address. Found the same day by
actually running the Ethernet-only configuration rather than assuming it
worked, which is the argument for testing the fault configurations and not
only the healthy one.

Both softap toggles now treat an absent WiFi stack as a configuration rather
than an error, and the settle task checks `wifi_on` before asking. Verified on
hardware, Ethernet-only on a BC04.

## Peripheral init could strand a board with no way in (fixed)

**Status: fixed** in `main/main.c` and `main/network.c`.

`init_all_peripherals()` talks to the hashboard over I2C and, on a board whose
bus is dead, does not return -- measured at over four minutes on the BC04 here,
with every API call printing `i2c handle not initialized`. Everything after
that call is unreachable, including the Ethernet start, so a miner in that
state with no WiFi credentials has no management interface at all.

A watchdog now starts Ethernet if init has not finished in 90 seconds. It
waits on `interface_initalized` rather than running on a bare timer, because
the reason Ethernet is deferred at all is that a W5500 already running when the
core rail steps up stops answering for good. Ethernet therefore starts either
after the hashboard is up or after it is known not to be coming up, never in
between. A healthy BC04 sets the flag around 35 s.

Verified on the failed BC04, with and without WiFi:

```
E (104159) serpentx: Peripheral init has not finished in 90 s -- the hashboard
                     is not coming up. Starting Ethernet anyway
I (106189) NETWORK: Ethernet Link Up
I (108689) esp_netif_handlers: eth ip: 192.168.50.31
```

A DHCP lease needs a full four-way exchange, so this also established that the
failed board's W5500, SPI bus and PHY all work and the fault is confined to the
I2C and hashboard domain.

## Ant Design is handed CSS variables it cannot read

**Where:** `main/http_server/axe-os/src/pages/App.vue`, the `a-config-provider`
theme block.

The theme is built on CSS custom properties, and the Ant Design tokens are set
to those properties as strings:

```js
colorPrimary: 'var(--ant-primary-color)',
colorInfo:    'var(--ant-primary-color)',
colorBgBase:  'var(--surface-ground)',
```

Ant Design v4 computes its palettes in JavaScript — it derives hover states,
active states, borders and contrasting text from `colorPrimary` before any CSS
is involved. A custom property is opaque to that, so every derived colour comes
out unusable. What reaches the page is whatever its fallback produces, which is
usually close to the surrounding background.

This is not a styling preference. It is a whole class of defects, and it has
produced at least three:

- the selected card tab rendered as an empty box, because Ant Design paints the
  label on an inner `.ant-tabs-tab-btn` and gave it a colour close to the tab's
  own background — the text was present and invisible
- an enabled switch looked identical to a disabled one, so there was no way to
  see whether dual mining was on
- anything else derived from the accent colour is suspect until checked

**Current state:** the three known cases are corrected in
`main/http_server/axe-os/src/styles/layout/_antd-fixes.scss`, which sets the
affected properties in ordinary CSS where the variables resolve correctly. That
file is a patch over the cause, not a fix for it, and it will grow every time
another component turns out to derive a colour the same way.

**The real fix** is to stop passing custom properties into the token block and
give Ant Design real colour values, driven from the same source as the CSS
variables so the two cannot drift:

- keep the palette in one place as literal values, in TypeScript
- derive the CSS custom properties from it, and pass the same literals to
  `a-config-provider`
- re-provide the tokens when the theme changes, so switching theme updates both
  halves together

The theme selector offers several accents, so whatever replaces this has to
survive a theme change at runtime rather than being read once at start-up.

**Why it was not done at the time:** it lands in the middle of the component
that mounts the whole interface, on a day that had already produced three
firmware releases. Rewriting the theme system while shipping is how something
breaks quietly. It wants a clear run and a careful look at every component that
reads an accent colour.

## The global stylesheet is compiled twice

**Where:** `main/http_server/axe-os/src/pages/App.vue`, line 305.

`main.ts` imports `styles/layout/layout.scss` globally. `App.vue` then imports
the same file again inside a `<style scoped>` block, so every rule in it is
emitted a second time carrying that component's scope attribute.

Two consequences. The CSS payload is roughly double what it needs to be. And
the scoped copies cannot reach anything Ant Design renders in a portal — a
dropdown, a modal — which is the same trap that left the pool logos unstyled
for as long as they were.

The fix is to delete the `@import` from `App.vue`; the global import in
`main.ts` already covers it. It is one line, but it is in the component that
mounts the entire interface, so it wants a build and a look at the running UI
rather than being done in passing.

Found while checking that the corrections in `_antd-fixes.scss` had reached the
device: the same rules appeared twice in the built bundle, once with a scope
attribute and once without.
