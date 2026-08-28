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
key is Hammer's.

This is not hypothetical. A retail BC01 measured here reports:

```
Secure Boot:      Enabled
  BLOCK_KEY0    - SECURE_BOOT_DIGEST0
  Secure Boot Key1 is Revoked
  Secure Boot Key2 is Revoked
Flash Encryption: Enabled
  SPI_BOOT_CRYPT_CNT: 0x7
  BLOCK_KEY1    - XTS_AES_128_KEY
JTAG:             Permanently Disabled
```

Every escape route is closed, deliberately:

- Secure Boot v2 holds up to three key digests. Slot 0 is Hammer's, and
  **slots 1 and 2 are revoked**, so there is no free slot to burn your own
  key into. Revocation is permanent.
- Flash encryption is on in release mode with the count at `0x7`, and the
  XTS key is read-protected. Writing a plaintext image over serial produces
  something the chip cannot decrypt, and you cannot produce a correctly
  encrypted one without a key you cannot read.
- JTAG is permanently disabled, so there is no debug path in.
- eFuses cannot be un-burned.

That ESP32-S3 runs Hammer's firmware or nothing. As a posture on the
vendor's part it is defensible; it does also mean owners cannot replace
software on hardware they own, on a product the vendor has said it will
not maintain.

### It does not have to be the end of it

Secure Boot is enforced by the **chip**, and on these miners the chip is on
a **socketed, off-the-shelf LilyGO T-Display-S3**. Fitting a fresh module
gives you an ESP32-S3 with unburned eFuses, which runs this firmware
normally. No exploit, no glitching, nothing irreversible — and the original
module goes back in if you change your mind.

The per-unit ASIC calibration lives in the hashboard EEPROM rather than on
the module, so it survives the swap.

See [HARDWARE-SWAP.md](HARDWARE-SWAP.md).

[SECURITY.md](SECURITY.md) applies either way — the missing authentication
is in NVS-backed configuration that no signature covers.

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
`CONFIG_STAYOPEN_OTA_OBFUSCATED`, and both flash encryption and Secure Boot
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
