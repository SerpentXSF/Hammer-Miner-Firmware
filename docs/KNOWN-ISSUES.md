# Known issues

Things that are understood, worked around, and worth doing properly.

## BC04 Ethernet stops working the moment the hashboard powers up

**Where:** the hardware, as far as every measurement can tell. The software
side is `network_eth_recover()` in `main/network.c`, which attempts a fix and
does not achieve one.

A BC04 brings its W5500 up cleanly: link, DHCP lease, DNS, NTP. Then the core
regulator switches on and **70 ms later** every socket command times out, for
good.

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
a link *state* change rather than only failed commands, so this reads as the
16 A core regulator resetting or browning out the W5500.

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

**Meanwhile:** run a BC04 on WiFi. `eth_on` and `wifi_on` are settable through
`PATCH /api/system` and reported by `/api/system/info`, so Ethernet can be
turned off without a factory restore -- which matters, because a factory
restore also discards the WiFi credentials that are the only remaining way in.

Related, and worth fixing whatever the cause turns out to be: when Ethernet
takes a lease the firmware switches the setup access point off, and never
brings it back if Ethernet later dies. That is how a BC04 ends up holding an
IP address, passing nothing, with no way to reach it.

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
