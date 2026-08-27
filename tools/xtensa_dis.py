"""Minimal Xtensa disassembler -- enough to read a small I2C driver.

Covers the instructions that carry the information wanted here: immediate
loads, byte stores into transfer buffers, literal-pool loads and calls.
Anything else prints as a length-correct placeholder so the stream stays in
sync, which is all that matters for reading constants out of a function.
"""
import struct
import sys

from espimg import Image

# 16-bit (density option) opcodes; everything else is 24-bit.
NARROW_OP0 = {0x8, 0x9, 0xA, 0xB, 0xC, 0xD}


def sign_extend(value, bits):
    sign = 1 << (bits - 1)
    return (value & (sign - 1)) - (value & sign)


def decode(data, offset, pc):
    """Return (text, length, immediate_or_None)."""
    b0 = data[offset]
    op0 = b0 & 0x0F
    narrow = op0 in NARROW_OP0
    length = 2 if narrow else 3

    if offset + length > len(data):
        return ".short", 2, None

    if narrow:
        insn = data[offset] | (data[offset + 1] << 8)
        t = (insn >> 4) & 0xF
        s = (insn >> 8) & 0xF
        r = (insn >> 12) & 0xF
        if op0 == 0xC and not (r & 0x8):
            imm = sign_extend((r << 4) | s, 7)
            return f"movi.n  a{t}, {imm}", 2, imm
        if op0 == 0x8:
            return f"l32i.n  a{t}, a{s}, {r * 4}", 2, None
        if op0 == 0x9:
            return f"s32i.n  a{t}, a{s}, {r * 4}", 2, None
        if op0 == 0xB:
            imm = r if r else -1
            return f"addi.n  a{t}, a{s}, {imm}", 2, imm
        if op0 == 0xA:
            return f"add.n   a{r}, a{s}, a{t}", 2, None
        if op0 == 0xD:
            if r == 0x0:
                return f"mov.n   a{t}, a{s}", 2, None
            if r == 0xF and t == 0x0:
                return "ret.n", 2, None
            return f"<st3 r={r}>", 2, None
        return f"<narrow op0={op0:x}>", 2, None

    insn = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
    t = (insn >> 4) & 0xF
    s = (insn >> 8) & 0xF
    r = (insn >> 12) & 0xF
    imm8 = (insn >> 16) & 0xFF
    op2 = (insn >> 20) & 0xF

    if op0 == 0x1:  # L32R
        imm16 = (insn >> 8) & 0xFFFF
        target = (((pc + 3) & 0xFFFFFFFC) + ((0xFFFC0000 | (imm16 << 2)) - (1 << 32))) & 0xFFFFFFFF
        return f"l32r    a{t}, 0x{target:08x}", 3, target

    if op0 == 0x5:  # CALLn
        n = (insn >> 4) & 0x3
        off18 = sign_extend(insn >> 6, 18)
        target = ((pc & 0xFFFFFFFC) + (off18 << 2) + 4) & 0xFFFFFFFF
        return f"call{n * 4:<3} 0x{target:08x}", 3, target

    if op0 == 0x2:
        # LSAI: the operation is selected by r (bits 12-15), not by the top
        # nibble. MOVI is r == 0xA, and its 12-bit immediate is split with
        # the high nibble in s.
        names = {0x0: "l8ui", 0x1: "l16ui", 0x2: "l32i", 0x4: "s8i",
                 0x5: "s16i", 0x6: "s32i", 0x9: "l16si", 0xB: "l32ai",
                 0xC: "addi", 0xD: "addmi"}
        if r == 0xA:
            imm12 = sign_extend((s << 8) | imm8, 12)
            return f"movi    a{t}, {imm12}  (0x{imm12 & 0xFFF:02x})", 3, imm12
        if r in names:
            scaled = imm8 * (4 if r in (0x2, 0x6, 0xB) else (2 if r in (0x1, 0x5, 0x9) else 1))
            return f"{names[r]:<7} a{t}, a{s}, {scaled}", 3, scaled
        return f"<lsai r={r:x}>", 3, None

    if op0 == 0x0:  # QRST
        op1 = (insn >> 16) & 0xF
        if op1 == 0x0:  # RST0
            logic = {0x1: "and", 0x2: "or", 0x3: "xor",
                     0x8: "add", 0x9: "addx2", 0xA: "addx4", 0xB: "addx8",
                     0xC: "sub", 0xD: "subx2", 0xE: "subx4", 0xF: "subx8"}
            if op2 in logic:
                return f"{logic[op2]:<7} a{r}, a{s}, a{t}", 3, None
            if op2 == 0x0:  # ST0
                if r == 0x0 and s == 0x0 and t == 0x0:
                    return "ret", 3, None
                if r == 0x0 and (insn >> 12) & 0xF == 0 and s == 0xC:
                    return f"callx8  a{t}", 3, None
                return f"<st0 r={r:x} s={s:x} t={t:x}>", 3, None
        if op1 in (0x4, 0x5):  # EXTUI
            shift = ((op1 & 1) << 4) | s
            width = r + 1
            return f"extui   a{t}, a{s}, {shift}, {width}", 3, None
        return f"<qrst op1={op1:x} op2={op2:x}>", 3, None

    if op0 == 0x6:
        n = (insn >> 4) & 0x3
        if n == 0x3:  # ENTRY
            frame = (((insn >> 12) & 0xFFF) * 8)
            return f"entry   a{s}, {frame}", 3, frame
        if n == 0x0:  # J
            off18 = sign_extend(insn >> 6, 18)
            return f"j       0x{(pc + 4 + off18) & 0xFFFFFFFF:08x}", 3, None
        if n == 0x1:  # BZ
            kinds = {0x0: "beqz", 0x1: "bnez", 0x2: "bltz", 0x3: "bgez"}
            off12 = sign_extend(insn >> 12, 12)
            kind = kinds.get((insn >> 6) & 0x3, "bz?")
            return f"{kind:<7} a{s}, 0x{(pc + 4 + off12) & 0xFFFFFFFF:08x}", 3, None
        return f"<bri12 n={n}>", 3, None

    if op0 == 0x7:  # branches comparing two operands / immediates
        kinds = {0x0: "bnone", 0x1: "beq", 0x2: "blt", 0x3: "bltu",
                 0x4: "ball", 0x5: "bbc", 0x8: "bany", 0x9: "bne",
                 0xA: "bge", 0xB: "bgeu", 0xC: "bnall", 0xD: "bbs"}
        off8 = sign_extend(imm8, 8)
        return (f"{kinds.get(r, 'b?'):<7} a{s}, a{t}, 0x{(pc + 4 + off8) & 0xFFFFFFFF:08x}",
                3, None)

    return f"<op0={op0:x}>", 3, None


def disassemble(img, start_va, end_va, annotate=None):
    off = img.file_off_of(start_va)
    if off is None:
        print(f"0x{start_va:08x} is not mapped")
        return
    pc = start_va
    while pc < end_va:
        o = img.file_off_of(pc)
        if o is None:
            break
        text, length, imm = decode(img.data, o, pc)
        note = ""
        if text.startswith("l32r") and imm is not None:
            value = img.read32(imm)
            if value is not None:
                note = f"   ; = 0x{value:08x}"
                s = read_cstr(img, value)
                if s:
                    note += f" {s!r}"
        if annotate and pc in annotate:
            note += f"   ; <<< {annotate[pc]}"
        print(f"  0x{pc:08x}:  {text:<34}{note}")
        pc += length


def read_cstr(img, vaddr, limit=90):
    off = img.file_off_of(vaddr)
    if off is None:
        return None
    end = img.data.find(b"\0", off, off + limit)
    if end < 0:
        return None
    raw = img.data[off:end]
    if len(raw) < 3 or not all(0x20 <= c < 0x7F for c in raw):
        return None
    return raw.decode("latin1")


if __name__ == "__main__":
    img = Image(sys.argv[1])
    start = int(sys.argv[2], 16)
    end = int(sys.argv[3], 16)
    disassemble(img, start, end)
