#!/usr/bin/env python3
"""Offline structural FAT16/UEFI tests; does NOT emulate UEFI firmware."""
import struct
import sys
from pathlib import Path

SECTOR=512

def le16(b,at):return struct.unpack_from('<H',b,at)[0]
def le32(b,at):return struct.unpack_from('<I',b,at)[0]

class Fat16:
    def __init__(self, disk, start, total):
        self.disk=disk;self.start=start;self.total=total
        self.base=start*SECTOR
        b=disk[self.base:self.base+SECTOR]
        assert b[510:512]==b'\x55\xaa'
        self.bps=le16(b,11);self.spc=b[13];self.reserved=le16(b,14)
        self.fats=b[16];self.roots=le16(b,17);self.spf=le16(b,22)
        assert self.bps==512 and self.spc in (1,2,4,8,16,32,64)
        assert self.fats==2 and self.reserved>=1 and self.roots==512
        assert (le16(b,19) or le32(b,32))==total
        self.root=self.base+(self.reserved+self.fats*self.spf)*SECTOR
        self.datastart=self.root+self.roots*32
        clusters=(self.base+self.total*SECTOR-self.datastart)//(self.spc*SECTOR)
        assert 4085<=clusters<65525
        assert (clusters+2)*2<=self.spf*SECTOR
        assert disk[self.base+SECTOR:self.base+(1+self.spf)*SECTOR]==disk[self.base+(1+self.spf)*SECTOR:self.base+(1+2*self.spf)*SECTOR]
    def dirent(self,start,name):
        for i in range(32 if start else self.roots):
            ofs=self.cluster(start)+32*i if start else self.root+32*i
            b=self.disk[ofs:ofs+32]
            if b[0]==0:return None
            if b[0] in (0xe5,):continue
            if b[0:11]==name:return (ofs,b)
        return None
    def cluster(self,n):
        return self.datastart+(n-2)*self.spc*SECTOR
    def fat(self,n):
        return le16(self.disk,self.base+SECTOR+n*2)
    def setfat(self,n,v):
        for ofs in (self.base+SECTOR,self.base+(self.reserved+self.spf)*SECTOR):
            struct.pack_into('<H',self.disk,ofs+n*2,v)
    def contents(self,entry):
        c=le16(entry,26); size=le32(entry,28)
        result=bytearray();visited=set()
        while len(result)<size:
            assert c>=2 and c<65525 and c not in visited
            visited.add(c)
            result+=self.disk[self.cluster(c):self.cluster(c)+self.spc*SECTOR]
            if len(result)<size:c=self.fat(c)
        return bytes(result[:size])

image=Path(sys.argv[1])
disk=bytearray(image.read_bytes())
assert len(disk)==(4096+16384+32768)*SECTOR
assert disk[510:512]==b'\x55\xaa'
assert disk[64*SECTOR+1:64*SECTOR+6]==b'CD001'
assert disk[446+4]==0xef and disk[462+4]==0x06
assert le32(disk,446+8)==4096 and le32(disk,446+12)==16384
assert le32(disk,462+8)==20480 and le32(disk,462+12)==32768
esp=Fat16(disk,4096,16384)
data=Fat16(disk,20480,32768)
efi=esp.dirent(0,b'EFI        ')
assert efi and efi[1][11]&0x10
boot=esp.dirent(le16(efi[1],26),b'BOOT       ')
assert boot and boot[1][11]&0x10
entry=esp.dirent(le16(boot[1],26),b'BOOTX64 EFI')
assert entry and entry[1][11]==0x20
pe=esp.contents(entry[1]);assert pe[0:2]==b'MZ'
peoff=le32(pe,0x3c)
assert pe[peoff:peoff+4]==b'PE\0\0'
assert le16(pe,peoff+4)==0x8664
assert le16(pe,peoff+24)==0x20b  # PE32+ optional header
assert le16(pe,peoff+24+68)==10 # IMAGE_SUBSYSTEM_EFI_APPLICATION
# Boot Services PE loader must be able to relocate the image in memory.
reloc_rva=le32(pe,peoff+24+112+5*8)
reloc_size=le32(pe,peoff+24+112+5*8+4)
assert reloc_rva and reloc_size>=8, 'Missing UEFI base relocation table' 
assert b'RodOS' in pe or b'R\x00o\x00d\x00O\x00S\x00' in pe
marker=data.dirent(0,b'RODOS   DAT')
assert marker and (marker[1][11] & 1)
assert data.contents(marker[1]).startswith(b'RODOS-DATA-V1')
welcome=data.dirent(0,b'LEEME   TXT')
assert welcome and b'RodOS 0.1.3' in data.contents(welcome[1])

# A host-side FAT16 mutation + simulated remount: the immutable ESP and boot
# region must not change; this does NOT prove UEFI FileProtocol writes in VM.
original=bytes(disk[:20480*SECTOR]); original_end=bytes(disk[(20480+32768)*SECTOR:])
newfile=data.dirent(0,b'TEST013 TXT')
assert newfile is None
root=data.root+2*32
cluster=4
assert data.fat(cluster)==0
payload=b'RodOS PERSIST 0.1.3\r\n'
disk[data.cluster(cluster):data.cluster(cluster)+len(payload)]=payload
entry=bytearray(32);entry[:11]=b'TEST013 TXT';entry[11]=0x20
struct.pack_into('<H',entry,26,cluster)
struct.pack_into('<I',entry,28,len(payload))
disk[root:root+32]=entry
data.setfat(cluster,0xffff)
# This creates a new logical FAT16 mount from modified bytes.
remounted=Fat16(disk,20480,32768)
found=remounted.dirent(0,b'TEST013 TXT')
assert found and remounted.contents(found[1])==payload
assert bytes(disk[:20480*SECTOR])==original
assert bytes(disk[(20480+32768)*SECTOR:])==original_end
print(f'OK: BIOS MBR + ESP FAT16 EFI/BOOT/BOOTX64.EFI ({len(pe)} bytes), FAT16 D:,')
print('    marker protegido, PE32+ UEFI x64 y escritura/remontaje FAT16 simulado.')
print('PENDIENTE: arranque y persistencia mediante firmware QEMU/OVMF o USB real.')
