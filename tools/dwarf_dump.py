#!/usr/bin/env python3
"""Read the debug information the vendor left in components/a/liba.a.

The blob was compiled with -gdwarf-4 -ggdb, so it still carries the name of
the withheld source file, every function signature, every local variable,
and per-function line numbers. See docs/ASIC-ABSTRACTION.md.

    python tools/dwarf_dump.py components/a/liba.a
    python tools/dwarf_dump.py components/a/liba.a CRC5
"""
import struct
import sys

def parse_ar(data):
    assert data[:8] == b"!<arch>\n", "not ar"
    off = 8
    members = []
    longnames = b""
    while off + 60 <= len(data):
        hdr = data[off:off+60]
        if len(hdr) < 60: break
        name = hdr[0:16].decode('latin1').rstrip()
        try:
            size = int(hdr[48:58].decode('latin1').strip())
        except ValueError:
            break
        body = data[off+60:off+60+size]
        if name.startswith('//'):
            longnames = body
        elif name.startswith('/') and name[1:].isdigit():
            n = int(name[1:])
            end = longnames.find(b'/', n)
            name = longnames[n:end].decode('latin1')
            members.append((name, body))
        elif name in ('/', '__.SYMDEF'):
            pass
        else:
            members.append((name.rstrip('/'), body))
        off += 60 + size + (size & 1)
    return members

TAG = {
    0x01: 'array_type', 0x0d: 'member', 0x04: 'enumeration_type',
    0x28: 'enumerator', 0x05: 'formal_parameter', 0x0b: 'lexical_block',
    0x11: 'compile_unit', 0x13: 'structure_type', 0x15: 'subroutine_type',
    0x16: 'typedef', 0x17: 'union_type', 0x21: 'subrange_type',
    0x24: 'base_type', 0x26: 'const_type', 0x0f: 'pointer_type',
    0x2e: 'subprogram', 0x34: 'variable', 0x35: 'volatile_type',
    0x3b: 'unspecified_parameters', 0x0a: 'label', 0x1d: 'inlined_subroutine',
}
AT = {
    0x01: 'sibling', 0x02: 'location', 0x03: 'name', 0x0b: 'byte_size',
    0x0c: 'bit_offset', 0x0d: 'bit_size', 0x10: 'stmt_list', 0x11: 'low_pc',
    0x12: 'high_pc', 0x1b: 'comp_dir', 0x25: 'producer', 0x27: 'prototyped',
    0x2e: 'bit_stride', 0x2f: 'upper_bound', 0x38: 'data_member_location',
    0x39: 'decl_column', 0x3a: 'decl_file', 0x3b: 'decl_line', 0x3e: 'encoding',
    0x3f: 'external', 0x40: 'frame_base', 0x49: 'type', 0x3c: 'declaration',
    0x6e: 'linkage_name', 0x1c: 'const_value', 0x34: 'artificial',
    0x87: 'noreturn', 0x3d: 'discr_value', 0x21: 'is_optional',
}


def uleb(b, o):
    r = s = 0
    while True:
        x = b[o]; o += 1
        r |= (x & 0x7F) << s; s += 7
        if not x & 0x80:
            return r, o


def sleb(b, o):
    r = s = 0
    while True:
        x = b[o]; o += 1
        r |= (x & 0x7F) << s; s += 7
        if not x & 0x80:
            if x & 0x40:
                r -= 1 << s
            return r, o


def sections(obj):
    e_shoff, = struct.unpack_from('<I', obj, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', obj, 46)
    raw = []
    for i in range(e_shnum):
        f = struct.unpack_from('<IIIIIIIIII', obj, e_shoff + i * e_shentsize)
        raw.append({'name': f[0], 'off': f[4], 'size': f[5]})
    sh = raw[e_shstrndx]
    strtab = obj[sh['off']:sh['off'] + sh['size']]
    out = {}
    for s in raw:
        end = strtab.find(b'\0', s['name'])
        out[strtab[s['name']:end].decode('latin1')] = obj[s['off']:s['off'] + s['size']]
    return out


def read_abbrev(b):
    table, o = {}, 0
    while o < len(b):
        code, o = uleb(b, o)
        if code == 0:
            continue
        tag, o = uleb(b, o)
        children = b[o]; o += 1
        attrs = []
        while True:
            at, o = uleb(b, o)
            form, o = uleb(b, o)
            if at == 0 and form == 0:
                break
            attrs.append((at, form))
        table[code] = (tag, children, attrs)
    return table


def read_form(b, o, form, cu_off, strs):
    if form == 0x01: v = struct.unpack_from('<I', b, o)[0]; o += 4          # addr
    elif form == 0x0b: v = b[o]; o += 1                                      # data1
    elif form == 0x05: v = struct.unpack_from('<H', b, o)[0]; o += 2         # data2
    elif form == 0x06: v = struct.unpack_from('<I', b, o)[0]; o += 4         # data4
    elif form == 0x07: v = struct.unpack_from('<Q', b, o)[0]; o += 8         # data8
    elif form == 0x0d: v, o = sleb(b, o)                                     # sdata
    elif form == 0x0f: v, o = uleb(b, o)                                     # udata
    elif form == 0x08:                                                       # string
        e = b.find(b'\0', o); v = b[o:e].decode('latin1'); o = e + 1
    elif form == 0x0e:                                                       # strp
        p = struct.unpack_from('<I', b, o)[0]; o += 4
        e = strs.find(b'\0', p); v = strs[p:e].decode('latin1')
    elif form == 0x11: v = cu_off + b[o]; o += 1                             # ref1
    elif form == 0x12: v = cu_off + struct.unpack_from('<H', b, o)[0]; o += 2
    elif form == 0x13: v = cu_off + struct.unpack_from('<I', b, o)[0]; o += 4
    elif form == 0x15: n, o = uleb(b, o); v = cu_off + n
    elif form == 0x10: v, o = uleb(b, o)                                     # ref_addr
    elif form == 0x17: v = struct.unpack_from('<I', b, o)[0]; o += 4         # sec_offset
    elif form == 0x18: n, o = uleb(b, o); v = b[o:o + n]; o += n             # exprloc
    elif form == 0x19: v = True                                              # flag_present
    elif form == 0x0c: v = bool(b[o]); o += 1                                # flag
    elif form == 0x09: n = b[o]; o += 1; v = b[o:o + n]; o += n              # block1
    elif form == 0x0a: n, o = uleb(b, o); v = b[o:o + n]; o += n
    elif form == 0x20: v = struct.unpack_from('<Q', b, o)[0]; o += 8
    else:
        raise NotImplementedError(f'form 0x{form:x}')
    return v, o


def parse(objpath):
    name, obj = parse_ar(open(objpath, 'rb').read())[0]
    sec = sections(obj)
    info, abbrev, strs = sec['.debug_info'], sec['.debug_abbrev'], sec['.debug_str']

    dies, o = {}, 0
    while o < len(info):
        cu_off = o
        unit_len, ver, abbrev_off, addr_size = struct.unpack_from('<IHIB', info, o)
        o += 11
        end = cu_off + 4 + unit_len
        table = read_abbrev(abbrev[abbrev_off:])
        stack = []
        while o < end:
            die_off = o
            code, o = uleb(info, o)
            if code == 0:
                if stack:
                    stack.pop()
                continue
            tag, has_children, attrs = table[code]
            die = {'tag': TAG.get(tag, hex(tag)), 'off': die_off,
                   'children': [], 'parent': stack[-1] if stack else None}
            for at, form in attrs:
                if form == 0x16:  # indirect
                    form, o = uleb(info, o)
                v, o = read_form(info, o, form, cu_off, strs)
                die[AT.get(at, hex(at))] = v
            dies[die_off] = die
            if stack:
                stack[-1]['children'].append(die)
            if has_children:
                stack.append(die)
        o = end
    return dies


def typename(dies, ref, depth=0):
    if ref is None or depth > 8:
        return 'void'
    d = dies.get(ref)
    if d is None:
        return '?'
    t = d['tag']
    if t in ('base_type', 'typedef', 'structure_type', 'enumeration_type', 'union_type'):
        return d.get('name', f'anon_{t}')
    if t == 'pointer_type':
        return typename(dies, d.get('type'), depth + 1) + ' *'
    if t == 'const_type':
        return 'const ' + typename(dies, d.get('type'), depth + 1)
    if t == 'volatile_type':
        return 'volatile ' + typename(dies, d.get('type'), depth + 1)
    if t == 'array_type':
        return typename(dies, d.get('type'), depth + 1) + '[]'
    return t


if __name__ == '__main__':
    dies = parse(sys.argv[1])
    want = sys.argv[2] if len(sys.argv) > 2 else None
    for d in dies.values():
        if d['tag'] != 'subprogram' or 'name' not in d:
            continue
        if want and d['name'] != want:
            continue
        params = [c for c in d['children'] if c['tag'] == 'formal_parameter']
        sig = ', '.join(f"{typename(dies, p.get('type'))} {p.get('name','?')}" for p in params)
        print(f"\n{typename(dies, d.get('type'))} {d['name']}({sig})"
              f"   [line {d.get('decl_line','?')}]")

        def walk(node, ind='    '):
            for c in node['children']:
                if c['tag'] == 'variable' and 'name' in c:
                    print(f"{ind}{typename(dies, c.get('type'))} {c['name']};"
                          f"  /* line {c.get('decl_line','?')} */")
                elif c['tag'] == 'lexical_block':
                    print(f"{ind}{{")
                    walk(c, ind + '    ')
                    print(f"{ind}}}")
        walk(d)
