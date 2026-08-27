"""Map an ESP32 application image and cross-reference its literal pools.

Xtensa loads constants and string addresses with L32R, a PC-relative load
from a nearby literal pool. So to find the code that uses a string, find the
string's virtual address, find the pool word holding it, then find the L32R
that points at that word.
"""
import struct
import sys


def segments(image):
    """Yield (index, load_addr, file_off, length) for each image segment."""
    count = image[1]
    off = 24
    out = []
    for i in range(count):
        load_addr, length = struct.unpack_from('<II', image, off)
        off += 8
        out.append((i, load_addr, off, length))
        off += length
    return out


class Image:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        self.segs = segments(self.data)

    def vaddr_of(self, file_off):
        for _, load, foff, length in self.segs:
            if foff <= file_off < foff + length:
                return load + (file_off - foff)
        return None

    def file_off_of(self, vaddr):
        for _, load, foff, length in self.segs:
            if load <= vaddr < load + length:
                return foff + (vaddr - load)
        return None

    def read32(self, vaddr):
        off = self.file_off_of(vaddr)
        if off is None or off + 4 > len(self.data):
            return None
        return struct.unpack_from('<I', self.data, off)[0]

    def is_exec(self, vaddr):
        """ESP32-S3 instruction RAM/flash windows."""
        return (0x40000000 <= vaddr < 0x40800000) or (0x42000000 <= vaddr < 0x44000000)

    def find_bytes(self, needle):
        out, start = [], 0
        while True:
            i = self.data.find(needle, start)
            if i < 0:
                return out
            out.append(i)
            start = i + 1

    def literals_pointing_to(self, target_vaddr):
        """Pool words whose value is target_vaddr. Returns their vaddrs."""
        needle = struct.pack('<I', target_vaddr)
        hits = []
        for off in self.find_bytes(needle):
            if off % 4:
                continue
            va = self.vaddr_of(off)
            if va is not None:
                hits.append(va)
        return hits

    def l32r_refs(self, literal_vaddr):
        """Find L32R instructions whose target is literal_vaddr.

        L32R is RI16: byte0 = (t << 4) | 0x1, then a 16-bit little-endian
        immediate. The target is
            ((PC + 3) & ~3) + (0xFFFC0000 | (imm16 << 2))
        i.e. always a backward reference, at most 256 KB away.
        """
        refs = []
        for _, load, foff, length in self.segs:
            if not self.is_exec(load):
                continue
            for i in range(length - 2):
                if self.data[foff + i] & 0x0F != 0x01:
                    continue
                imm16 = self.data[foff + i + 1] | (self.data[foff + i + 2] << 8)
                pc = load + i
                target = (((pc + 3) & 0xFFFFFFFC) + ((0xFFFC0000 | (imm16 << 2)) - (1 << 32))) & 0xFFFFFFFF
                if target == literal_vaddr:
                    refs.append(pc)
        return refs


if __name__ == '__main__':
    img = Image(sys.argv[1])
    print("segments:")
    for i, load, foff, length in img.segs:
        print(f"  {i}  vaddr=0x{load:08x}  file=0x{foff:06x}  len={length} (0x{length:x})"
              f"  {'EXEC' if img.is_exec(load) else ''}")

    for needle in sys.argv[2:]:
        raw = needle.encode()
        print(f"\n=== {needle!r} ===")
        for off in img.find_bytes(raw):
            va = img.vaddr_of(off)
            if va is None:
                continue
            print(f"  string at file 0x{off:06x}  vaddr 0x{va:08x}")
            for lit in img.literals_pointing_to(va):
                refs = img.l32r_refs(lit)
                print(f"    literal 0x{lit:08x} -> L32R at "
                      + (", ".join(f"0x{r:08x}" for r in refs) if refs else "(none)"))
