# Historial de cambios

## 0.1.3 — 2026-11-03

- Nuevo intérprete UEFI x64 en C++17 freestanding (`uefi/`).
- Binario EFI PE32+ reubicable, con entrada `efi_main` y sistema de archivos EFI.
- Partición de sistema ESP FAT16 (8 MiB) con `EFI/BOOT/BOOTX64.EFI`.
- Partición de datos FAT16 (16 MiB) con marcador de identificación `RODOS.DAT`.
- Imagen híbrida MBR + ISO9660 + ESP + datos para USB (Rufus modo DD).
- Persistencia: `PERSISTTEST` con escritura, `Flush`, cierre y nueva lectura.
- Protección para no escribir en otras particiones no marcadas; marcador protegido.
- Pruebas: escritura/remontaje FAT16 simulados y comandos UEFI con FileProtocol simulado.
- Lanzador QEMU/OVMF y CI para compilar/verificar imagen.
- Limitación: arranque real UEFI/BIOS y guardado persistente en hardware físico o QEMU siguen pendientes.

## 0.1.2 — 2026-11-03

- Almacenamiento FAT16 persistente en D: para ruta BIOS/CSM.
- MBR con kernel y código de acceso BIOS EDD, imagen híbrida ISO+USB.
- Pruebas de sistema de archivos FAT16 con reinicialización simulada.
