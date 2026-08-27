# Secure Boot and flash encryption

**Read this before flashing. Some of what it describes cannot be undone.**

---

## Check your device first

Signing a firmware image and enforcing that signature are separate things.
The vendor's build signs images — the shipping BC01 2.0.3 payload carries a
valid RSA-3072-PSS signature block. Whether the device *enforces* it depends
on eFuses burned at manufacture, and nothing in the HTTP API reports that.

Put the miner in download mode and ask it directly:

```bash
esptool.py --port COM3 get_security_info
```

### `secure_boot_en` is **false**

The signature is decorative on that unit. You can flash this firmware.

It also means [SECURITY.md finding 2](SECURITY.md#2-unauthenticated-ota-with-obfuscation-standing-in-for-integrity)
is **critical** for your device rather than a denial-of-service: an
unauthenticated host on your network can install arbitrary firmware today.
Set an API password and isolate the miner.

### `secure_boot_en` is **true**

**Do not flash unsigned firmware.** The bootloader will refuse to boot an
image not signed by a key whose digest is burned into the eFuses, and that
key is Hammer's. You do not have it, and eFuses cannot be un-burned.

Your options are to keep running vendor firmware, or to use this repository
for its analysis and tooling rather than as a replacement image. Rolling
back is not possible: a device with Secure Boot enabled cannot be returned
to accepting unsigned images.

---

## Flash encryption

The vendor `sdkconfig` shipped with:

```
CONFIG_SECURE_FLASH_ENC_ENABLED=y
CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y
```

**Release-mode flash encryption is irreversible.** On first boot the
bootloader generates a key, burns it into eFuses, encrypts the flash, and
disables the ROM download-mode paths that could read it back. There is no
way out.

Worse, in the vendor tree that option did double duty: `http_server.c`
selected which OTA container format to accept with
`#ifdef CONFIG_SECURE_FLASH_ENC_ENABLED`. You could not build a device that
spoke the shipping update format without also arming irreversible flash
encryption.

This repository separates them. The container format is now selected by
`CONFIG_HAMMER_OTA_OBFUSCATED`, and both flash encryption and Secure Boot
are **off** in the default `sdkconfig`, because a default that permanently
alters hardware on first boot is not a safe default for a build anyone can
run.

The vendor's original configuration is preserved verbatim as
`sdkconfig.vendor-reference` for comparison.

---

## Enabling Secure Boot on your own units

Only worth doing on hardware whose eFuses are still unburned, and only if
you accept that it is permanent.

Generate a signing key and keep it somewhere you will not lose it. If you
lose it you can never update that device again:

```bash
espsecure.py generate_signing_key --version 2 secure/secure_boot_signing_key.pem
```

`secure/` is gitignored. Never commit the key.

Then enable, in `idf.py menuconfig` under Security features:

```
CONFIG_SECURE_BOOT=y
CONFIG_SECURE_BOOT_V2_ENABLED=y
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure/secure_boot_signing_key.pem"
```

Build and flash over USB. The eFuses burn on first boot.

From then on `esp_ota_set_boot_partition()` verifies the signature before
switching partitions, so a forged OTA image is rejected rather than
installed — which is the property the XOR obfuscation was standing in for
and never provided.

---

## What this does and does not buy you

Secure Boot stops someone replacing the firmware. It does not stop someone
who can already reach an unauthenticated API from repointing your pool and
taking your hashrate — the change is written to NVS, not to the app
partition, and no signature covers it.

Set an API password ([AUTH.md](AUTH.md)) and keep the miner off untrusted
networks. Secure Boot is the last line, not the first.
