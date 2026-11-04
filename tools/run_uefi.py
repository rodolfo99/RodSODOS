#!/usr/bin/env python3
"""RodOS UEFI USB QEMU/OVMF runner; writes only to a local disk-image copy."""
from pathlib import Path
import argparse
import shutil
import subprocess

root = Path(__file__).resolve().parent.parent
p = argparse.ArgumentParser(description='Inicia RodOS 0.1.3 UEFI con USB virtual persistente')
p.add_argument('--fresh', action='store_true', help='Crea una nueva unidad virtual, borrando solo la anterior copia de pruebas')
p.add_argument('--qemu', default=shutil.which('qemu-system-x86_64'))
p.add_argument('--ovmf-code', help='Ruta a OVMF_CODE.fd, ovmf x64')
p.add_argument('--ovmf-vars', help='Ruta a OVMF_VARS.fd, ovmf x64')
p.add_argument('--display', default='default', choices=['default','none'], help='default: VGA; none: sin GUI (puede no mostrar salida UEFI)')
a=p.parse_args()
base=root/'build/RodOS-0.1.3-BIOS-UEFI-USB.iso'
vm=root/'build/RodOS-0.1.3-VM-UEFI-persistente.img'
varsfile=root/'build/RODOS-OVMF-VARS.fd'
if not base.is_file():p.error('Primero ejecuta make usb')
if not a.qemu:p.error('Instala qemu-system-x86 (Ubuntu: sudo apt install qemu-system-x86)')
if a.fresh or not vm.exists():
    shutil.copyfile(base,vm)
    print('Copia USB virtual creada:',vm,flush=True)
paths=['/usr/share/OVMF/OVMF_CODE_4M.fd','/usr/share/OVMF/OVMF_CODE.fd',
       '/usr/share/ovmf/OVMF.fd','/usr/share/edk2-ovmf/x64/OVMF_CODE.4m.fd',
       '/usr/share/edk2-ovmf/x64/OVMF_CODE.fd']
code=Path(a.ovmf_code) if a.ovmf_code else next((Path(x) for x in paths if Path(x).exists()),None)
if not code or not code.is_file():p.error('OVMF no encontrado; instala ovmf o indica --ovmf-code')
vars_paths=['/usr/share/OVMF/OVMF_VARS_4M.fd','/usr/share/OVMF/OVMF_VARS.fd',
            '/usr/share/edk2-ovmf/x64/OVMF_VARS.4m.fd','/usr/share/edk2-ovmf/x64/OVMF_VARS.fd']
vars_template=Path(a.ovmf_vars) if a.ovmf_vars else next((Path(x) for x in vars_paths if Path(x).exists()),None)
cmd=[a.qemu,'-machine','q35','-accel','tcg','-m','256M','-nic','none']
if vars_template and vars_template.is_file():
    if a.fresh or not varsfile.exists():shutil.copyfile(vars_template,varsfile)
    cmd+=['-drive',f'if=pflash,format=raw,readonly=on,file={code}',
          '-drive',f'if=pflash,format=raw,file={varsfile}']
else:
    cmd+=['-bios',str(code)]
cmd+=['-device','qemu-xhci','-drive',f'if=none,id=stick,format=raw,file={vm}',
      '-device','usb-storage,drive=stick,bootindex=1']
if a.display=='none':cmd+=['-display','none','-serial','mon:stdio']
else:cmd+=['-display','default','-serial','none']
print('RodOS UEFI: USB virtual persistente:',vm,flush=True)
print('ATENCION: ejecuta PERSISTTEST; REBOOT; TYPE PERSIS.TXT; y PERSISTTEST.',flush=True)
raise SystemExit(subprocess.call(cmd))
