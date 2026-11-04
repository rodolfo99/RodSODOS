# Validación RodOS 0.1.2 — 3 de noviembre de 2026

## Verificado en este entorno

- El kernel i386 C++17 freestanding y el módulo ensamblador BIOS compilaron y enlazaron
  con GCC/GNU binutils (`make build/kernel.bin`). Tamaño final del kernel: ver salida de build.
- ISO USB creada sin utilidades externas ISO, a partir de código fuente y Python.
- `file` identificó ISO9660 con sector DOS/MBR; firmas MBR (`55aa`), PVD (`CD001`)
  y sector FAT16 (`55aa`) validados por lectura de bytes.
- La partición FAT16 ocupa LBA 4096–36863; los archivos FAT16 se escribieron
  únicamente allí en el simulador de bloques.
- `make test-unit` pasó 79 072 aserciones de RAMFS y 2 000 mutaciones.
- `make test-fat16` pasó: lectura de LEEME.TXT; creación, lectura, sobreescritura,
  anexado, archivo máximo, renombrado, eliminación, reinicio del objeto volumen,
  archivo persistente; comprobación de bytes ISO/arranque invariables.
- Pruebas C++ en anfitrión compiladas con AddressSanitizer y UBSan.

## No verificado

- Arranque real con QEMU: no está instalado/disponible en este entorno.
- Arranque desde memoria USB real, compatibilidad de distintos BIOS/CSM.
- La transición de CPU protegido ↔ real durante `INT 13h` en ejecución y
  escritura efectiva de un sector a través del firmware BIOS.
- Funcionamiento en UEFI puro, que **no** está implementado.
- Recuperación de corrupción FAT por pérdida de energía / USB retirado en escritura.

## Límites conocidos

- D: solo raíz FAT16, nombres 8.3; archivos de texto ≤4095 bytes.
- Operaciones FAT16 no ofrecen journaling, bloqueo entre sistemas ni transacciones
  tolerantes a cortes de luz. Se recomienda una USB de pruebas, sin datos importantes.
- EDD requerido para leer el kernel desde el dispositivo de arranque.
- En Rufus, **modo DD**; modo ISO extraído no preserva el MBR/FAT16 especial.
- Los archivos preexistentes en la USB se destruyen cuando Rufus escribe la imagen.

## Probar en una máquina con QEMU

1. Ejecutar `python3 tools/run.py --fresh --terminal`.
2. Confirmar `USBINFO`, `D:`, `DIR`, `TYPE LEEME.TXT`.
3. `ECHO PERSISTE > NUEVO.TXT` y `TYPE NUEVO.TXT`.
4. `REBOOT`, `D:`, `TYPE NUEVO.TXT`: verificar si persiste.
5. Cerrar QEMU y abrir nuevamente sin `--fresh`; repetir TYPE.
6. Si falla, no grabar USB físico hasta depurar la causa.
