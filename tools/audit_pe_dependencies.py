"""Audit normal and delay-load PE imports against package/local Windows DLLs.

Development-side verification uses Python's standard library only. This does
not resolve LoadLibrary names constructed at runtime; verify-portable.ps1
complements it with actual loaded-module and plugin logs.
"""
import argparse
import json
import os
import struct
from pathlib import Path


def imports(path):
    data = path.read_bytes()
    def u16(pos): return struct.unpack_from('<H', data, pos)[0]
    def u32(pos): return struct.unpack_from('<I', data, pos)[0]
    if data[:2] != b'MZ':
        raise ValueError(f'Not PE: {path}')
    pe = u32(0x3c)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError(f'Invalid PE: {path}')
    optional = pe + 24
    magic = u16(optional)
    if magic not in (0x10b, 0x20b):
        raise ValueError(f'Unsupported PE: {path}')
    directory = optional + (112 if magic == 0x20b else 96)
    directory_count = u32(optional + (108 if magic == 0x20b else 92))
    image_base = struct.unpack_from('<Q' if magic == 0x20b else '<I', data, optional + (24 if magic == 0x20b else 28))[0]
    sections = optional + u16(pe + 20)
    def offset(rva):
        for i in range(u16(pe + 6)):
            row = sections + 40 * i
            size, address, raw_size, raw = struct.unpack_from('<IIII', data, row + 8)
            if address <= rva < address + max(size, raw_size):
                return raw + rva - address
        if rva < u32(optional + 60):
            return rva
        raise ValueError(f'Unmapped RVA {rva:x}: {path}')
    def name(rva):
        start = offset(rva)
        return data[start:data.index(b'\0', start)].decode('ascii').lower()
    found = set()
    if directory_count > 1 and u32(directory + 8):
        pos = offset(u32(directory + 8))
        while any(data[pos:pos+20]):
            found.add(name(u32(pos + 12)))
            pos += 20
    if directory_count > 13 and u32(directory + 13*8):
        pos = offset(u32(directory + 13*8))
        while any(data[pos:pos+32]):
            attributes, address = u32(pos), u32(pos+4)
            found.add(name(address if attributes & 1 else address-image_base))
            pos += 32
    return sorted(found)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    root = args.package.resolve()
    system = Path(os.environ['SystemRoot']) / 'System32'
    records, missing = [], []
    for binary in sorted(root.rglob('*')):
        if binary.suffix.lower() not in ('.dll', '.exe'):
            continue
        for dependency in imports(binary):
            if dependency.startswith(('api-ms-win-', 'ext-ms-win-')):
                # API-set names are virtual contracts implemented by Windows,
                # and do not have to exist as files beside the application.
                origin = 'Windows API-set contract'
            else:
                candidates = [binary.parent/dependency, root/dependency, system/dependency]
                resolved = next((p for p in candidates if p.is_file()), None)
                origin = str(resolved) if resolved else None
            records.append({'binary':str(binary.relative_to(root)), 'import':dependency, 'resolved':origin})
            if origin is None:
                missing.append(records[-1])
    report = {'package':str(root), 'binaries':len({r['binary'] for r in records}),
        'import_records':len(records), 'missing':missing, 'imports':records,
        'scope':'Normal/delay PE imports against package paths and current Windows System32; runtime LoadLibrary imports require the separate launch check.'}
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f"PE audit: {report['binaries']} binaries, {len(records)} imports, {len(missing)} unresolved")
    for row in missing:
        print(row['binary'], 'requires', row['import'])
    raise SystemExit(bool(missing))


if __name__ == '__main__':
    main()
