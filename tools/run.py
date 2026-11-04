#!/usr/bin/env python3
"""Start RodOS 0.1.2 in a BIOS QEMU VM with a persistent *copy* of the USB image."""
import argparse
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description='Ejecuta RodOS BIOS + D: FAT16 en QEMU.')
parser.add_argument('--terminal', action='store_true', help='Consola serie, sin interfaz grafica')
parser.add_argument('--qemu', help='Ruta del ejecutable qemu-system-i386')
parser.add_argument('--fresh', action='store_true', help='Recrea la copia virtual y borra sus cambios anteriores')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
iso = root/'build/RodOS-0.1.2-USB-FAT16.iso'
drive = root/'build/RodOS-0.1.2-VM-persistente.img'
if not iso.is_file():
    raise SystemExit('No existe la ISO. Ejecute make usb primero.')
qemu = args.qemu or shutil.which('qemu-system-i386') or shutil.which('qemu-system-x86_64')
if not qemu:
    raise SystemExit('QEMU ausente. En Ubuntu: sudo apt install qemu-system-x86')
if args.fresh or not drive.is_file():
    shutil.copyfile(iso,drive)
    print(f'Unidad de prueba creada: {drive} (no modifica la ISO original).',flush=True)
print('D: se guarda en la copia .img. Al reiniciar QEMU, se conservan sus cambios.',flush=True)
cmd=[qemu,'-machine','pc','-accel','tcg','-m','32M',
     '-drive',f'file={str(drive).replace(",", ",,")},format=raw,if=ide,media=disk',
     '-boot','order=c','-rtc','base=localtime','-nic','none']
if args.terminal:
    cmd+=['-display','none','-serial','mon:stdio']
    print('Salir de QEMU: Ctrl+A, luego X. Dentro de RodOS: SHUTDOWN.',flush=True)
else:
    cmd+=['-serial','none','-monitor','none']
raise SystemExit(subprocess.call(cmd))
