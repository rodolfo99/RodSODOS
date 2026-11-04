#!/usr/bin/env python3
"""Create a bootable 1.44 MiB floppy image without third-party Python packages."""
import argparse
import hashlib
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--as', dest='assembler', default='as')
parser.add_argument('--ld', dest='linker', default='ld')
args = parser.parse_args()
build = root / 'build'
kernel = (build / 'kernel.bin').read_bytes()
sectors = (len(kernel) + 511) // 512
if not 1 <= sectors <= 768:
    raise SystemExit('Kernel size must be between 1 byte and 384 KiB.')
subprocess.run([args.assembler, '--32', '--defsym', f'KERNEL_SECTORS={sectors}',
                str(root/'boot/boot.S'), '-o', str(build/'boot.o')], check=True)
subprocess.run([args.linker, '-m', 'elf_i386', '-Ttext', '0x7c00', '--oformat',
                'binary', '-e', '_start', str(build/'boot.o'), '-o', str(build/'boot.bin')], check=True)
boot = (build/'boot.bin').read_bytes()
if len(boot) != 512 or boot[-2:] != b'\x55\xaa':
    raise SystemExit('Invalid boot sector size or signature.')
image = bytearray(1440 * 1024)
image[:512] = boot
image[512:512+len(kernel)] = kernel
target = build/'rodos.img'
target.write_bytes(image)
(build/'SHA256SUMS').write_text(hashlib.sha256(image).hexdigest()+'  rodos.img\n')
print(f'RodOS: kernel {len(kernel)} bytes, {sectors} sectors; image {len(image)} bytes.')
