#!/usr/bin/env python3
"""Pack, unpack and inspect Hammer miner OTA update images.

The update container and the obfuscation it uses are described in
docs/OTA-FORMAT.md. In short: a 0xAA type byte, a 48-byte header, then the
payload, with header and payload XORed against a repeating 16-byte pad
derived from a build-time key.

The pad can be recovered from a single image with no key, because firmware
contains long runs of a constant byte -- see `recover`.

    python tools/ota_tool.py inspect  update.bin
    python tools/ota_tool.py unpack   update.bin -o app.bin
    python tools/ota_tool.py pack     app.bin    -o update.bin --pad <hex>
    python tools/ota_tool.py recover  update.bin
"""

import argparse
import collections
import hashlib
import struct
import sys
import zlib

# Application images are tagged 0xAA and web-UI images 0x55; the firmware
# rejects the wrong one outright. The APP payload is obfuscated with the
# pad, the WWW payload is not -- see docs/OTA-FORMAT.md.
TYPE_APP = 0xAA
TYPE_WWW = 0x55
HEADER_SIZE = 48
PAD_SIZE = 16
PROJECT_ID = b"GULLPOWER"
APP_DESC_OFFSET = 0x20
APP_DESC_MAGIC = 0xABCD5432


def xor_pad(data, pad, offset=0):
    """Apply the repeating pad, continuing the keystream from `offset`."""
    return bytes(b ^ pad[(offset + i) % PAD_SIZE] for i, b in enumerate(data))


def pad_from_key(key):
    """Derive the pad the firmware would use for a 32-byte key."""
    if len(key) != 32:
        raise ValueError(f"key must be 32 bytes, got {len(key)}")
    return hashlib.sha256(key).digest()[:PAD_SIZE]


def recover_pad(payload, plain_byte=0x00):
    """Recover the pad from ciphertext by frequency analysis.

    Assumes the most common plaintext byte at each position modulo the pad
    length is `plain_byte`, which holds for firmware images because of
    padding and zero-filled BSS.
    """
    pad = bytearray(PAD_SIZE)
    for index in range(PAD_SIZE):
        column = payload[index::PAD_SIZE]
        most_common = collections.Counter(column).most_common(1)[0][0]
        pad[index] = most_common ^ plain_byte
    return bytes(pad)


def split(blob):
    """Split a container into (header_ciphertext, payload_ciphertext)."""
    if len(blob) < 1 + HEADER_SIZE:
        raise ValueError("file is too short to be an update image")
    if blob[0] not in (TYPE_APP, TYPE_WWW):
        raise ValueError(f"bad type flag 0x{blob[0]:02x}, expected 0x{TYPE_APP:02x} (app) "
                         f"or 0x{TYPE_WWW:02x} (www)")
    return blob[1:1 + HEADER_SIZE], blob[1 + HEADER_SIZE:]


def parse_header(header):
    """Decode a decrypted 48-byte header."""
    return {
        "sha256": header[:32],
        "project_id": header[32:42].split(b"\0")[0],
        "length": struct.unpack_from("<I", header, 42)[0],
        "reserved": header[46:48],
    }


def build_header(payload, reserved=b"\0\0"):
    header = bytearray(HEADER_SIZE)
    header[0:32] = hashlib.sha256(payload).digest()
    header[32:32 + len(PROJECT_ID)] = PROJECT_ID
    struct.pack_into("<I", header, 42, len(payload))
    header[46:48] = reserved
    return bytes(header)


def describe_app(image):
    """Read esp_app_desc_t out of a plaintext ESP32 application image."""
    if len(image) < APP_DESC_OFFSET + 176 or image[0] != 0xE9:
        return None
    magic, = struct.unpack_from("<I", image, APP_DESC_OFFSET)
    if magic != APP_DESC_MAGIC:
        return None

    def field(start, size):
        return image[APP_DESC_OFFSET + start:APP_DESC_OFFSET + start + size].split(b"\0")[0].decode("latin1")

    return {
        "version": field(16, 32),
        "project_name": field(48, 32),
        "time": field(80, 16),
        "date": field(96, 16),
        "idf_ver": field(112, 32),
    }


def describe_signature(image):
    """Locate and validate an ESP32 Secure Boot v2 signature block.

    The block occupies the final 4 KB sector of a signed image. It is
    self-checking: a CRC32 over its own body, and a SHA-256 of everything
    preceding it.
    """
    if len(image) < 4096 or len(image) % 4096:
        return None
    offset = len(image) - 4096
    block = image[offset:offset + 1216]
    if block[0] != 0xE7 or block[1] != 0x02:
        return None

    digest = block[4:36]
    modulus = int.from_bytes(block[36:36 + 384], "little")
    exponent, = struct.unpack_from("<I", block, 36 + 384)
    signature = int.from_bytes(block[812:812 + 384], "little")
    stored_crc, = struct.unpack_from("<I", block, 1196)

    signed = image[:offset]
    encoded = pow(signature, exponent, modulus).to_bytes(384, "big")

    return {
        "offset": offset,
        "modulus_bits": modulus.bit_length(),
        "exponent": exponent,
        "crc_ok": zlib.crc32(block[:1196]) & 0xFFFFFFFF == stored_crc,
        "digest_ok": hashlib.sha256(signed).digest() == digest,
        "pss_trailer_ok": encoded[-1] == 0xBC,
        "digest": digest.hex(),
    }


def resolve_pad(args, payload):
    if args.pad:
        pad = bytes.fromhex(args.pad.replace(" ", ""))
        if len(pad) != PAD_SIZE:
            sys.exit(f"--pad must be {PAD_SIZE} bytes, got {len(pad)}")
        return pad, "supplied"
    if args.key:
        with open(args.key, "rb") as handle:
            return pad_from_key(handle.read()), f"derived from {args.key}"
    return recover_pad(payload), "recovered from ciphertext"


def cmd_inspect(args):
    blob = open(args.image, "rb").read()
    header_ct, payload_ct = split(blob)
    pad, source = resolve_pad(args, payload_ct)

    header = parse_header(xor_pad(header_ct, pad, 0))
    # The web-UI path deobfuscates only the header; it writes and hashes the
    # payload as received, so a www payload is plaintext on the wire.
    is_www = blob[0] == TYPE_WWW
    payload = payload_ct if is_www else xor_pad(payload_ct, pad, HEADER_SIZE)
    digest = hashlib.sha256(payload).digest()

    print(f"file            : {args.image}")
    print(f"type            : 0x{blob[0]:02x} ({'www, payload plaintext' if is_www else 'app, payload obfuscated'})")
    print(f"size            : {len(blob)} bytes")
    print(f"pad             : {pad.hex(' ')}  ({source})")
    print(f"project id      : {header['project_id'].decode('latin1')!r}")
    print(f"declared length : {header['length']}")
    print(f"actual payload  : {len(payload)}")
    print(f"reserved [46:48]: {header['reserved'].hex(' ')}")
    print(f"embedded sha256 : {header['sha256'].hex()}")
    print(f"computed sha256 : {digest.hex()}")
    print(f"integrity       : {'OK' if digest == header['sha256'] else 'MISMATCH'}")

    desc = describe_app(payload)
    if desc:
        print("\nesp_app_desc_t:")
        for key, value in desc.items():
            print(f"  {key:<13}: {value}")

    sig = describe_signature(payload)
    if sig:
        print("\nSecure Boot v2 signature block:")
        print(f"  offset       : 0x{sig['offset']:x}")
        print(f"  RSA          : {sig['modulus_bits']} bits, exponent {sig['exponent']}")
        print(f"  block CRC32  : {'OK' if sig['crc_ok'] else 'BAD'}")
        print(f"  image digest : {'OK' if sig['digest_ok'] else 'BAD'}")
        print(f"  PSS trailer  : {'OK' if sig['pss_trailer_ok'] else 'BAD'}")
        print("  note         : enforced only if Secure Boot eFuses are burned")
    else:
        print("\nSecure Boot v2 signature block: none found (image is unsigned)")

    return 0 if digest == header["sha256"] else 1


def cmd_unpack(args):
    blob = open(args.image, "rb").read()
    header_ct, payload_ct = split(blob)
    pad, _ = resolve_pad(args, payload_ct)

    header = parse_header(xor_pad(header_ct, pad, 0))
    payload = payload_ct if blob[0] == TYPE_WWW else xor_pad(payload_ct, pad, HEADER_SIZE)

    if hashlib.sha256(payload).digest() != header["sha256"] and not args.force:
        sys.exit("digest mismatch; wrong pad or corrupt image (use --force to write anyway)")

    payload = payload[:header["length"]]
    with open(args.output, "wb") as handle:
        handle.write(payload)
    print(f"wrote {len(payload)} bytes to {args.output}")
    return 0


def cmd_pack(args):
    payload = open(args.image, "rb").read()
    if not args.pad and not args.key:
        sys.exit("pack requires --pad or --key; there is nothing to recover a pad from")
    pad, _ = resolve_pad(args, payload)

    reserved = bytes.fromhex(args.reserved.replace(" ", ""))
    if len(reserved) != 2:
        sys.exit("--reserved must be exactly 2 bytes")

    header = build_header(payload, reserved)

    # The two update paths differ in more than the type byte. The application
    # updater deobfuscates the whole image; the web-UI updater deobfuscates the
    # header only and writes the payload exactly as received, so a www payload
    # goes out in the clear. Packing a web-UI image like an application one
    # produces something the miner accepts and then writes as garbage.
    if args.type == "www":
        blob = bytes([TYPE_WWW]) + xor_pad(header, pad, 0) + payload
    else:
        blob = bytes([TYPE_APP]) + xor_pad(header, pad, 0) + xor_pad(payload, pad, HEADER_SIZE)
    with open(args.output, "wb") as handle:
        handle.write(blob)
    print(f"wrote {len(blob)} bytes to {args.output}")
    print(f"payload sha256: {hashlib.sha256(payload).hexdigest()}")
    return 0


def cmd_recover(args):
    blob = open(args.image, "rb").read()
    _, payload_ct = split(blob)
    pad = recover_pad(payload_ct, args.plain_byte)
    print(pad.hex(" "))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def add_pad_options(p):
        p.add_argument("--pad", help=f"{PAD_SIZE}-byte pad as hex")
        p.add_argument("--key", help="file holding the 32-byte key to derive the pad from")

    p = sub.add_parser("inspect", help="show header fields and verify integrity")
    p.add_argument("image")
    add_pad_options(p)
    p.set_defaults(func=cmd_inspect)

    p = sub.add_parser("unpack", help="decrypt to a raw application image")
    p.add_argument("image")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--force", action="store_true", help="write even if the digest mismatches")
    add_pad_options(p)
    p.set_defaults(func=cmd_unpack)

    p = sub.add_parser("pack", help="build an update image from a raw application or web-UI image")
    p.add_argument("image")
    p.add_argument("-o", "--output", required=True)
    p.add_argument("--type", choices=("app", "www"), default="app",
                   help="which updater the image is for: the application "
                        "(0xAA, payload obfuscated) or the web UI (0x55, "
                        "payload plaintext). Default app.")
    p.add_argument("--reserved", default="0000",
                   help="2 bytes for header[46:48], which the firmware never reads "
                        "(default 0000; stock 2.0.3 carries 8bbb)")
    add_pad_options(p)
    p.set_defaults(func=cmd_pack)

    p = sub.add_parser("recover", help="recover the pad from ciphertext alone")
    p.add_argument("image")
    p.add_argument("--plain-byte", type=lambda v: int(v, 0), default=0x00,
                   help="assumed most common plaintext byte (default 0)")
    p.set_defaults(func=cmd_recover)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
