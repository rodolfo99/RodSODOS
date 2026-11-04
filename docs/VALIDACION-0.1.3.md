# Validación de RodOS 0.1.3 — 3 de noviembre de 2026

## Verificado en entorno de desarrollo

- Compilación freestanding del kernel BIOS i386 con GNU GCC/binutils.
- Compilación de `uefi/main.cpp` x64 con Clang 17 y enlace `lld-link` a PE32+ `IMAGE_SUBSYSTEM_EFI_APPLICATION` (subsystem 10), 8192 bytes.
- Imagen MBR/ISO9660 con ESP FAT16 en LBA 4096, 16384 sectores (8 MiB), tipo 0xEF; y datos FAT16 en LBA 20480, 32768 sectores (16 MiB), tipo 0x06.
- Archivo `EFI/BOOT/BOOTX64.EFI` recorriendo la cadena FAT16 hasta verificar cabecera MZ/PE de arquitectura AMD64.
- Comprobación de que `RODOS.DAT` tiene atributo read-only y contiene firma `RODOS-DATA-V1`.
- Prueba del FAT16 por sectores para BIOS, con lecturas/escrituras y reinicialización del objeto volumen; comprobación de que la zona previa a datos no cambia.
- Prueba simulada en Python de escritura a clúster libre FAT16 en partición D:, persistencia de bytes tras un remonte lógico y protección de la zona EFI/BIOS.
- Prueba de la consola UEFI real ejecutada con servicios EFI simulados: detección del marcador, dos arranques del intérprete, `PERSISTTEST` incrementa 1 a 2, confirma `Flush`, y ejecuta `ECHO`, `TYPE`, `REN`, `DEL`.
- Archivo PE32+ con tabla de relocaciones válida para reubicación UEFI.
- Pruebas de RAMFS de la versión anterior.

## No verificado en este entorno

- Ejecución real de QEMU con firmware OVMF: no disponible en el entorno de construcción.
- Que el firmware reconozca el ESP y arranque `BOOTX64.EFI` desde USB.
- Que `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` permita escritura en el segundo volumen FAT16 en QEMU o USB físico.
- Persistencia de escritura UEFI tras `REBOOT` real; los tests FAT16 Python y `uefi_mock_test.cpp` son **simuladores**, no una prueba en firmware EFI real.
- Arranque BIOS, puente INT13, y comportamiento con controladoras físicas.

## Procedimiento de aceptación pendiente

1. `sudo apt install qemu-system-x86 ovmf`.
2. `python3 tools/run_uefi.py --fresh` (se crea un archivo de imagen de prueba).
3. Ver consola `RodOS 0.1.3 | UEFI x64` y mensaje `D: USB FAT16 detectada`.
4. `DIR`, `TYPE LEEME.TXT`, `PERSISTTEST`, `TYPE PERSIS.TXT`.
5. `REBOOT` y verificar `TYPE PERSIS.TXT` devuelve `1`.
6. `PERSISTTEST` otra vez: debe devolver `2`.
7. Cerrar QEMU, abrir nuevamente sin `--fresh`, comprobar que `TYPE PERSIS.TXT` muestra `2`.
8. Repetir con USB físico **solo después de validar máquina virtual**, con un dispositivo vacío y respaldado.

## Seguridad de almacenamiento

- UEFI selecciona únicamente FAT con marcador `RODOS.DAT`; no modifica otros discos voluntariamente.
- La prueba de host rechaza escrituras fuera de la partición de datos.
- `PERSISTTEST` modifica exclusivamente `PERSIS.TXT`; si su contenido no es un número válido, no lo altera.
- La imagen ISO entregada es plantilla de solo lectura; las pruebas deben escribir en copias.
- No se ofrece tolerancia a fallo eléctrico ni journaling.
