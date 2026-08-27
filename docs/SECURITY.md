# Security assessment

Findings against the BC04 source as released, and against a BC01 running
stock firmware 2.0.1. Each finding states how it was confirmed.

**If you run a Hammer miner today, read [Mitigations](#mitigations) first.**
The issues below are in shipping firmware, not only in this source tree.

---

## Summary

| # | Severity | Issue | Status here |
|---|---|---|---|
| [1](#1-no-authentication-on-any-endpoint) | Critical | No authentication on any HTTP endpoint | Fixed |
| [2](#2-unauthenticated-ota-with-obfuscation-standing-in-for-integrity) | High / Critical | Unauthenticated OTA; impact depends on whether Secure Boot eFuses were burned | Mitigated |
| [3](#3-credentials-shipped-in-the-factory-image) | High | WiFi password, API token, third-party payout address in factory NVS | Fixed |
| [4](#4-uninitialised-ota-handle-used-on-error-paths) | Medium | Uninitialised `esp_ota_handle_t` passed to `esp_ota_abort()` | Fixed |
| [5](#5-telemetry-enabled-by-default) | Low | InfluxDB reporting on by default to a hardcoded host | Fixed |

---

## 1. No authentication on any endpoint

The web UI ships a login page. [`Login.vue`](../main/http_server/axe-os/src/pages/Login.vue)
collects a username and password, and
[`api/index.ts`](../main/http_server/axe-os/src/api/index.ts) POSTs them to
`/api/system/login`.

**That route does not exist in the firmware.** The registered URI table in
`http_server.c` contains no login handler, no session issuance, and no
credential check. The login screen is decorative.

Every endpoint is instead gated by `is_network_allowed()`, which resolves
to:

```c
if (ip_in_private_range(origin_ip_addr) == ESP_OK &&
    ip_in_private_range(request_ip_addr) == ESP_OK) {
    return ESP_OK;
}
```

This asks "is the caller on a private network", not "is the caller
authorised". It is a DNS-rebinding mitigation being used as an access
control. Any host on the same LAN — a guest device, a compromised IoT
appliance, a browser on a colleague's laptop — has full administrative
access.

Confirmed live against a stock BC01 at 2.0.1 with no credentials:

```console
$ curl -s http://<miner>/api/system/info
{ "hostname": "HammerBC01", "ASICModel": "BM1370", "stratumURL": ...,
  "stratumUser": "bc1q...", "ssid": "...", ... }
```

The same lack of a check applies to `PATCH /api/system` (repoint the pool),
`POST /api/system/restart`, `POST /api/system/OTA`, and the rest.

An attacker who redirects `stratumURL` and `stratumUser` steals the
device's entire hash output, and the change survives reboot because it is
written to NVS.

### Fix

A shared-secret check is enforced in the firmware for every state-changing
and information-disclosing endpoint, with the network-range test retained
as defence in depth rather than as the sole control. See
[`docs/AUTH.md`](AUTH.md).

---

## 2. Unauthenticated OTA, with obfuscation standing in for integrity

`POST /api/system/OTA` accepts an update image, deobfuscates it, verifies a
SHA-256 that travels inside that same image, writes it to the inactive OTA
slot, and calls `esp_ota_set_boot_partition()`.

**The endpoint requires no authentication.** Per finding 1, any host on the
LAN can start an update.

### What the obfuscation does not do

The images are not encrypted. The scheme is a repeating 16-byte XOR pad
recoverable from any published firmware image without the key -- full
analysis in
[OTA-FORMAT.md](OTA-FORMAT.md#the-obfuscation-is-not-encryption). The
SHA-256 in the header is computed over the plaintext and obfuscated with
that same pad, so it authenticates nothing: an attacker recomputes it over
their own payload.

Verified: `tools/ota_tool.py` unpacks the vendor's own
`bc01-miner-2.0.3-20260625-update.bin` and repacks it byte for byte, using
a pad recovered from ciphertext alone with no key material.

The key is compiled into the application image
(`_binary_flash_encryption_key_bin_start`), so it is identical across units
of a model and extractable from any one of them.

### What does provide integrity

Secure Boot v2. The vendor's `sdkconfig` enables it
(`CONFIG_SECURE_BOOT_V2_ENABLED=y`, `CONFIG_SECURE_BOOT=y`), and the
shipping BC01 2.0.3 image carries a valid signature block at offset
`0x270000`:

```
magic          : 0xe7, version 2 (RSA-PSS)
RSA modulus    : 3072 bits, exponent 65537
block CRC32    : valid
image digest   : matches SHA-256 of the signed region 0..0x270000
s^e mod n      : ends 0xbc, a well-formed PSS trailer
```

When Secure Boot is enabled, `esp_ota_set_boot_partition()` calls
`image_validate(ESP_IMAGE_VERIFY)`, which reaches
`verify_secure_boot_signature()` in `esp_image_format.c`. **A forged image
is rejected at OTA time**, before the boot partition is switched.

So the obfuscation being broken does not, by itself, yield code execution.

### Actual impact

This is the part that cannot be settled remotely, and it decides the
severity:

**If Secure Boot eFuses are burned** — an unauthenticated LAN attacker can
still consume the inactive OTA slot and force repeated failed updates. That
is a denial-of-service and a nuisance, not code execution.

**If they are not burned** — the signature is decorative, `esp_ota` skips
verification, and the same attacker gets persistent code execution. This is
not a hypothetical population: the vendor describes this product as
"pre-production ... released to the market in limited quantities", and
pre-production units are exactly where secure-boot provisioning is most
often skipped.

Signing a binary happens at build time. Enforcing the signature requires
burning eFuses at manufacture. The two are independent, and no HTTP
endpoint reports which was done.

### Check your own device

Over USB, with the miner in download mode:

```bash
esptool.py --port COM3 get_security_info
```

Look for `secure_boot_en`. If it is not set, treat finding 2 as critical
for that unit and isolate it (see [Mitigations](#mitigations)).

### Fix

Authentication now covers the OTA endpoint (finding 1), which is the
control that was actually missing.

The container format is deliberately left unchanged so that stock vendor
images still install. Replacing the XOR pad with real encryption would
break that compatibility while adding nothing: the integrity guarantee that
matters comes from Secure Boot, not from the transport. What the pad is
worth is documented honestly rather than overstated.

For units you flash yourself, enable Secure Boot v2 properly --
see [SECURE-BOOT.md](SECURE-BOOT.md).

---

## 3. Credentials shipped in the factory image

[`config.cvs`](../config.cvs) is flashed into the NVS partition at
manufacture. As released it contains:

| Key | Value | Problem |
|---|---|---|
| `wifissid` / `wifipass` | `GOKJ` / `GOKJ666888` | Factory test WiFi credentials in plaintext in every shipped image. |
| `influx_token` | `f37fh783hf8hq` | API token in plaintext. |
| `stratumuser` | `DG4GqZJ9gdNYvxg9ddYatByqzNKGtZJafx` | Payout address that belongs to neither the buyer nor, on its face, to a BTC wallet. |
| `fbstratumuser` | `LX1ysSCebjGSVPgf8Wqg2Tg9Ya1oRhHWsz` | Same, as fallback. |

A device flashed with the factory NVS and powered on before the owner
finishes configuring it mines to an address the owner does not control.

### Fix

`config.cvs` is replaced with a placeholder template carrying no
credentials and no payout address. Provisioning your own values is a
documented step in [README.md](../README.md), and the build fails rather
than silently flashing placeholders.

---

## 4. Uninitialised OTA handle used on error paths

In `POST_OTA_update()`:

```c
esp_ota_handle_t ota_handle;          /* declared, not initialised */
...
esp_ota_abort(ota_handle);            /* three error paths reach here ... */
...
esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);   /* ... before this */
```

Three failure paths — a short read on the header, a deobfuscation failure,
and a project-id mismatch — call `esp_ota_abort()` on an indeterminate
value before `esp_ota_begin()` ever assigns it. `esp_ota_abort()` looks the
handle up in its internal list, so the practical outcome is a spurious
`ESP_ERR_INVALID_ARG` in the common case, and corruption of an unrelated
in-flight OTA entry if the value happens to collide.

All three are reachable by an unauthenticated LAN client simply by
truncating a request.

### Fix

The handle is initialised to zero, aborts are guarded on a
`begun` flag, and the error paths that precede `esp_ota_begin()` no longer
call abort at all.

---

## 5. Telemetry enabled by default

`config.cvs` sets `influx_enable = 1` with `influx_url =
http://10.98.18.79`, a hardcoded private address, and a bucket and
organisation both named `nerdqaxeplus`.

The destination is unroutable outside the vendor's own network, so this
leaks nothing externally, but it means stock devices attempt periodic
unsolicited reporting by default over plain HTTP.

### Fix

Defaults to disabled. Enabling it is an explicit choice with a
user-supplied destination.

---

## Mitigations

If you are running stock Hammer firmware and are not yet ready to reflash:

1. **Put the miner on an isolated VLAN or a dedicated SSID** with no
   inbound access from general client devices. This is the single most
   effective step and it addresses findings 1 and 2 together.
2. **Do not expose the miner to the internet.** Never port-forward it. The
   private-range check is the only thing standing between the API and the
   open internet, and it fails open behind NAT-hairpin and some proxy
   setups.
3. **Change the payout address immediately** on any device you have not
   already configured, and confirm it in `/api/system/info`.
4. **Change the WiFi password** if your network ever used the factory
   `GOKJ` credentials.
5. **Treat the miner as an untrusted device on your network.** Assume any
   host that can reach it can control it.

---

## Reporting

Security issues in *this* repository can be raised as a GitHub issue, or
privately if you prefer — see the repository contact details.

Issues in Hammer's shipping firmware belong with Hammer. The findings here
concern source the vendor published publicly and a protocol weakness
recoverable from a public download, so they are documented openly rather
than withheld; the reasoning is set out in
[OTA-FORMAT.md](OTA-FORMAT.md#why-this-is-documented-rather-than-withheld).
