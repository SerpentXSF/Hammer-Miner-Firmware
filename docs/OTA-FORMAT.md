# The OTA update container

Specification of the `.bin` update format accepted by `/api/system/OTA`
and `/api/system/OTAWWW`, and an assessment of the obfuscation it uses.

The reference implementation is [`tools/ota_tool.py`](../tools/ota_tool.py).

---

## Container layout

```
offset  size  field
------  ----  --------------------------------------------------------
     0     1  type flag, must be 0xAA
     1    48  header, obfuscated
    49     n  payload, obfuscated
```

The header decrypts to:

```
offset  size  field
------  ----  --------------------------------------------------------
     0    32  SHA-256 of the plaintext payload
    32    10  project id, NUL-padded ASCII: "GULLPOWER"
    42     4  payload length, uint32 little-endian
    46     2  unused
```

The firmware reads only bytes 0–45; it never touches 46–47. The stock
2.0.3 image carries `8b bb` there. That value does not match CRC-16
(CCITT, XMODEM, Modbus, IBM), CRC-32, Adler-32, or a byte sum over either
the header or the payload, so it is most likely uninitialized memory from
the vendor's packaging tool rather than a field. `ota_tool.py pack`
defaults them to zero and accepts `--reserved` to reproduce a given image
byte for byte.

The device checks the project id, checks the declared length against the
target partition size, streams the payload into the OTA partition while
hashing it, and compares against the embedded digest before setting the
boot partition.

The keystream offset is continuous across the two regions: the header is
decrypted at offset 0, and the payload begins at offset 48. Because 48 is
a multiple of the 16-byte period, payload byte 0 lands on keystream index 0.

---

## The obfuscation is not encryption

The routine is named `aes_decrypt` and commented `简化版AES-ECB解密实现`
("simplified AES-ECB decryption"). It contains no AES:

```c
unsigned char hash_val[32];
mbedtls_sha256(key, 32, hash_val);          /* key is a build-time constant */

for (i = 0; i < input_len; i++) {
    size_t j = (start_byte_offset + i) % 16;
    output[i] = input[i] ^ hash_val[j % 32];
}
```

`j` is the result of `% 16`, so it is always less than 16 and the
subsequent `% 32` never does anything. Only **the first 16 bytes** of the
SHA-256 output are ever used. The result is a repeating 16-byte XOR pad —
a Vigenère cipher with a 16-byte key.

Three consequences follow, and all of them are practical rather than
theoretical:

**It is trivially recoverable from ciphertext alone.** Firmware images
contain long runs of a constant byte. Taking the most frequent byte at
each position modulo 16 recovers the pad without any knowledge of the key,
in one pass, with no guessing. That is how the pad in this repository's
test vectors was obtained.

**It provides no authenticity.** The SHA-256 in the header is computed
over the plaintext and then obfuscated with the same broken pad. Anyone
who recovers the pad can produce a header that validates for arbitrary
payload. The container itself carries no signature and no MAC — the
authenticity that does exist comes from the Secure Boot v2 signature block
inside the payload, not from the container. See
[The signature block](#the-signature-block).

**The key is shared and static.** It is compiled into the application
image as `_binary_flash_encryption_key_bin_start`, so every unit in a
model family carries the same one, and it is extractable from any single
device's flash.

### Recovered pad

The BC01 2.0.3 image is obfuscated with:

```
69 cc 74 ae af 0c e6 83 22 9d 42 2f 54 42 8a 54
```

Applying it to `bc01-miner-2.0.3-20260625-update.bin` yields a payload
whose SHA-256 matches the digest embedded in that file's own header
exactly, which confirms the format above end to end.

---

## What the decrypted image proves

The plaintext is a standard ESP32-S3 application image. Its
`esp_app_desc_t` reads:

```
project_name : bc01-miner
version      : 2.0.3 20260625
time / date  : 17:16:20 Jun 25 2026
idf_ver      : v5.5.1-390-g1a6bc9685a-dirty
```

Its embedded `__FILE__` paths are the BC04 tree's own paths, including the
misspelling `./main/tasks/health_maintennance.c` that is present in this
repository. The image also carries strings for `BC01`, `BC02`, `BC04`, and
`BC08`, and for `BM1370`, `BM1373`, and `LT0051`.

The BC01 and the BC04 are therefore built from one codebase with runtime
model selection — which is why the BC01 shipping with no source at all is
a gap in the vendor's compliance rather than a separate product.

---

## The signature block

The payload is a signed image. The last 4 KB, at offset `0x270000`, hold an
ESP32 Secure Boot v2 signature block, and it validates:

```
magic            0xe7      version 2 (RSA-PSS)
RSA modulus      3072 bits, exponent 65537
block CRC32      0x8014b924, matches the computed value
image digest     b0e68c9f0b691392dd08fc432086f6a832687444b72e297d1a5945e46a2624ba
sha256(0..0x270000)  identical to the above
s^e mod n        ends in 0xbc, a well-formed PSS trailer
```

`tools/ota_tool.py inspect` reports this. It is why the obfuscation being
broken does not on its own let an attacker run code on a properly
provisioned device: `esp_ota_set_boot_partition()` verifies this block
before switching partitions.

The signature is only enforced if Secure Boot eFuses were actually burned
at manufacture — a separate step from signing the binary. See
[SECURITY.md](SECURITY.md#2-unauthenticated-ota-with-obfuscation-standing-in-for-integrity).

---

## Why this is documented rather than withheld

The obfuscation cannot be repaired by keeping quiet about it. A 16-byte
repeating pad is recoverable by anyone holding one firmware image, and
those are published downloads. Treating the scheme as a secret protects
nobody while leaving owners unaware of how their miners accept updates.

What actually protects update integrity on these devices is **Secure Boot
v2**, not the pad. The vendor's `sdkconfig` enables it, and the shipping
BC01 2.0.3 image carries a valid RSA-3072-PSS signature block at offset
`0x270000` whose digest matches the signed region. With Secure Boot on,
`esp_ota_set_boot_partition()` runs `image_validate(ESP_IMAGE_VERIFY)` and
rejects an unsigned or altered image before switching the boot partition.

That protection depends on eFuses having been burned at manufacture, which
is independent of whether the binary was signed and cannot be determined
over the network. Owners can check with `esptool.py get_security_info` over
USB. See [SECURITY.md](SECURITY.md#2-unauthenticated-ota-with-obfuscation-standing-in-for-integrity)
for what follows from each answer.

The defect worth acting on is therefore not the weak pad. It is that **the
OTA endpoint has no authentication at all**, so any host on the network can
initiate an update at will.

## A bad update now reverts itself

Rollback is enabled, so an image installed over the air boots once in
`PENDING_VERIFY`. If it does not reach `confirm_running_image()` in `system.c`
-- it panics, hangs before the system task runs, or reboots -- the bootloader
puts the previous image back on the next reset.

This matters more here than on most hardware. Recovery of last resort is a
serial flash, and on these units the ESP32-S3 is a socketed module behind a
screwed-on lid; an update that boots into a crash loop otherwise means opening
the case. These images also go to other people through a web flasher, where
that is not a reasonable thing to ask.

The bar for confirming is that the build is alive and has stayed alive for 60
seconds, not that it is mining. Tying it to shares would let an unreachable
pool revert a perfectly good update, which is a worse failure than the one
being guarded against.

A serially flashed image is `UNDEFINED` rather than `PENDING_VERIFY` and needs
no confirmation, so this costs one partition read on a normal boot.

Verified by packing a build, installing it through `/api/system/OTA`, and
watching it confirm itself: `update confirmed after 60s; keeping this image`,
followed by normal mining.
