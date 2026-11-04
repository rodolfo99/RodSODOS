#!/usr/bin/env python3
"""Build RodOS 0.1.2 USB image: BIOS MBR + valid ISO9660 header + FAT16 data.

For Rufus choose raw/DD mode (not ISO extraction). ISO9660 header is read-only;
D: is a *separate* FAT16 partition visible in the partition table. Not UEFI.
No host drive is read/written or formatted; only an output file is created.
"""
from pathlib import Path
import datetime as dt
import hashlib
import struct
import subprocess

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build'
OUT = BUILD / 'RodOS-0.1.2-USB-FAT16.iso'
SECTOR = 512
KERNEL_START = 256
DATA_START = 4096
DATA_COUNT = 32768
TOTAL_SECTORS = DATA_START + DATA_COUNT
FAT_SECTORS = 64
SPC = 2
FAT_ROOT = 512
FAT_FIRST_ROOT = DATA_START + 1 + 2 * FAT_SECTORS
FAT_FIRST_DATA = FAT_FIRST_ROOT + 32


def put16both(buf, offset, number):
    struct.pack_into('<H', buf, offset, number)
    struct.pack_into('>H', buf, offset + 2, number)


def put32both(buf, offset, number):
    struct.pack_into('<I', buf, offset, number)
    struct.pack_into('>I', buf, offset + 4, number)


def record(extent, size, name, flags=0):
    name = name if isinstance(name, bytes) else name.encode('ascii')
    n = 33 + len(name) + (len(name) % 2 == 0)
    b = bytearray(n)
    b[0] = n
    put32both(b, 2, extent)
    put32both(b, 10, size)
    b[18:25] = bytes([126, 11, 3, 12, 0, 0, 0])
    b[25] = flags
    put16both(b, 28, 1)
    b[32] = len(name)
    b[33:33 + len(name)] = name
    return b


def store_at(fh, sector, data):
    fh.seek(sector * SECTOR)
    fh.write(data)


def make_iso():
    kernel = (BUILD / 'kernel.bin').read_bytes()
    count = (len(kernel) + 511) // 512
    if not 1 <= count <= 256 or (KERNEL_START + count) * 512 > DATA_START * 512:
        raise SystemExit('Kernel must fit in reserved pre-partition space (max 128 KiB).')
    boot_o = BUILD / 'usb_mbr.o'
    boot_bin = BUILD / 'usb_mbr.bin'
    subprocess.run(['as', '--32', '--defsym', f'KERNEL_SECTORS={count}',
                    str(ROOT / 'boot/usb_mbr.S'), '-o', str(boot_o)], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-Ttext', '0x7c00', '--oformat', 'binary',
                    '-e', '_start', str(boot_o), '-o', str(boot_bin)], check=True)
    mbr_code = boot_bin.read_bytes()
    if len(mbr_code) > 446:
        raise SystemExit('MBR boot code overlaps partition table.')
    mbr = bytearray(512)
    mbr[:len(mbr_code)] = mbr_code
    # Active partition type 0x06: FAT16 CHS with LBA from modern BIOS EDD.
    mbr[446] = 0x80
    mbr[447:450] = b'\xfe\xff\xff'
    mbr[450] = 0x06
    mbr[451:454] = b'\xfe\xff\xff'
    struct.pack_into('<II', mbr, 454, DATA_START, DATA_COUNT)
    mbr[510:512] = b'\x55\xaa'
    # ISO9660 metadata occupies logical sectors 16..21 (512-sector 64..87).
    # Kernel at sector 256 avoids overwriting the ISO9660 descriptors.
    pvd = bytearray(2048)
    pvd[0:7] = b'\x01CD001\x01'
    pvd[8:40] = b'RODOS_EDUCATIONAL'.ljust(32)
    pvd[40:72] = b'RODOS_012_USB'.ljust(32)
    put32both(pvd, 80, DATA_START * 512 // 2048)
    put16both(pvd, 120, 1)
    put16both(pvd, 124, 1)
    put16both(pvd, 128, 2048)
    put32both(pvd, 132, 10)
    struct.pack_into('<I', pvd, 140, 18)
    struct.pack_into('>I', pvd, 148, 19)
    pvd[156:190] = record(20, 2048, b'\x00', flags=2)
    for offset, value in [(190, 'RODOS'), (318, 'RODOS 0.1.2'), (446, 'BIOS BOOT USB DD FAT16')]:
        pvd[offset:offset + len(value)] = value.encode('ascii')
    pvd[813:830] = b'2026110312000000\x00'  # ISO datetime format
    pvd[881] = 1  # file structure version
    vdst = bytearray(2048)
    vdst[:7] = b'\xffCD001\x01'
    lpath = bytearray(2048)
    lpath[:10] = b'\x01\x00\x14\x00\x00\x00\x01\x00\x00\x00'
    mpath = bytearray(2048)
    mpath[:10] = b'\x01\x00\x00\x00\x00\x14\x00\x01\x00\x00'
    iso_readme = ("RODOS 0.1.2 USB: ARRANQUE BIOS/CSM CON RUFUS EN MODO DD.\r\n"
                  "C: contiene archivos temporales en RAM.\r\n"
                  "D: es una particion FAT16, modificable desde RodOS.\r\n"
                  "No seleccionar modo ISO de extraccion en Rufus.\r\n"
                  "No es compatible con arranque UEFI puro / Secure Boot.\r\n"
                  "El software es experimental; haz respaldo de tu USB.\r\n").encode('ascii')
    directory = bytearray(2048)
    pos = 0
    for entry in (record(20, 2048, b'\x00', 2),
                  record(20, 2048, b'\x01', 2),
                  record(21, len(iso_readme), 'README.TXT;1')):
        directory[pos:pos+len(entry)] = entry
        pos += len(entry)
    # FAT16 BPB + volume signature. FAT partition is data-only.
    bpb = bytearray(512)
    bpb[:3] = b'\xeb\x3c\x90'
    bpb[3:11] = b'RODOS012'
    struct.pack_into('<HBHBHHBHHHII', bpb, 11,
                     512, SPC, 1, 2, FAT_ROOT, DATA_COUNT, 0xF8, FAT_SECTORS,
                     63, 255, DATA_START, 0)
    bpb[36] = 0x80
    bpb[38] = 0x29
    struct.pack_into('<I', bpb, 39, 0x01202611)
    bpb[43:54] = b'RODOS DATA '
    bpb[54:62] = b'FAT16   '
    bpb[62:80] = b'Datos de RodOS USB'
    bpb[510:512] = b'\x55\xaa'
    fat = bytearray(512 * FAT_SECTORS)
    struct.pack_into('<HHH', fat, 0, 0xFFF8, 0xFFFF, 0xFFFF)
    root = bytearray(512)
    welcome = (b'Bienvenido a D: FAT16 de RodOS 0.1.2.\r\n'
               b'Crea archivos con ECHO Hola > D:\\NOTA.TXT\r\n'
               b'Los cambios en D: deben conservarse al reiniciar.\r\n')
    root[:11] = b'LEEME   TXT'
    root[11] = 0x20
    struct.pack_into('<H', root, 26, 2)
    struct.pack_into('<I', root, 28, len(welcome))
    with OUT.open('wb') as fh:
        fh.truncate(TOTAL_SECTORS * SECTOR)
        store_at(fh, 0, mbr)
        for sec, val in [(64,pvd),(68,vdst),(72,lpath),(76,mpath),(80,directory),
                         (84,iso_readme),(DATA_START,bpb),
                         (DATA_START+1,fat),(DATA_START+1+FAT_SECTORS,fat),
                         (FAT_FIRST_ROOT,root),(FAT_FIRST_DATA,welcome)]:
            store_at(fh, sec, val)
        store_at(fh,KERNEL_START,kernel)
    digest = hashlib.sha256(OUT.read_bytes()).hexdigest()
    (BUILD/'RodOS-0.1.2-USB-FAT16.iso.sha256').write_text(f'{digest}  {OUT.name}\n')
    print(f'{OUT} | {OUT.stat().st_size:,} B | kernel {len(kernel):,} B | SHA256 {digest}')

if __name__ == '__main__':
    make_iso()
