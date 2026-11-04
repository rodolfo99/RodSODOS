# RodOS 0.1.3 — sistema educativo tipo MS-DOS en C++

RodOS es un proyecto independiente (no usa Linux, FreeDOS ni MS-DOS) con dos rutas de arranque:

| Arranque | Ejecución | Acceso a disco |
| --- | --- | --- |
| **BIOS/CSM** x86 | Kernel propio C++17 de 32 bits en modo protegido; VGA/PS2/RTC | BIOS INT 13h EDD + controlador FAT16 propio (`kernel/`) |
| **UEFI x64** | Aplicación **PE32+ EFI** nativa en C++17; consola DOS | `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` y `EFI_FILE_PROTOCOL` (`uefi/`) |

> **Importante:** la aplicación UEFI todavía no inicia el kernel BIOS de 32 bits. Ejecuta un entorno DOS-style separado mientras los servicios de arranque UEFI permanecen activos. Es una transición hacia un kernel UEFI de 64 bits, no una migración ya completada.

## Novedades 0.1.3

- `EFI/BOOT/BOOTX64.EFI`: arranque UEFI x64 desde una **partición ESP FAT16** de 8 MiB.
- Partición **D: FAT16** de 16 MiB dedicada a datos, separada de ESP y del arranque BIOS.
- Tanto el kernel BIOS como la aplicación UEFI pueden leer y escribir archivos de texto en `D:`.
- El shell UEFI localiza `D:` únicamente si contiene el archivo marcador de sólo lectura `RODOS.DAT` con firma `RODOS-DATA-V1`, para evitar escribir en otras particiones FAT del equipo.
- `PERSISTTEST` en UEFI aumenta `PERSIS.TXT`, fuerza `Flush` y lo relee. Después de `REBOOT`, `TYPE PERSIS.TXT` permite verificar que el contador se conservó.
- Scripts y pruebas verifican el formato MBR/ISO9660, FAT16 ESP, PE32+ EFI con relocaciones, mutaciones FAT16 simuladas y el intérprete UEFI mediante servicios EFI simulados.

**Imagen única para USB (Rufus modo DD):** `build/RodOS-0.1.3-BIOS-UEFI-USB.iso` (aprox. 26 MiB). No utilizar extracción ISO, pues se perderían las particiones.

## Compilar en Ubuntu x86_64

Requiere `make`, GCC con compilación `-m32` freestanding, GNU binutils, Python 3, **Clang** y **lld-link**.

```bash
sudo apt install build-essential binutils clang lld python3 make
make -j2
make test
```

`make usb` crea la imagen en `build/` y `build/BOOTX64.EFI`; `make test` prueba RAMFS, FAT16, la estructura EFI y el intérprete UEFI con servicios EFI simulados. **No prueba físicamente que UEFI arranque o escriba en el USB.**

## Probar primero en QEMU con UEFI/OVMF

Instala el emulador y firmware:

```bash
sudo apt install qemu-system-x86 ovmf
python3 tools/run_uefi.py --fresh
```

El script trabaja exclusivamente con una **copia** `build/RodOS-0.1.3-VM-UEFI-persistente.img` y crea un archivo de variables OVMF independiente. Para reutilizar los archivos guardados, ejecuta sin `--fresh`.

En la consola UEFI de RodOS (distribución de teclado proporcionada por el firmware):

```text
HELP
D:\>DIR
D:\>TYPE LEEME.TXT
D:\>ECHO Mi primera nota > NOTA.TXT
D:\>TYPE NOTA.TXT
D:\>PERSISTTEST
D:\>REBOOT
D:\>TYPE PERSIS.TXT
D:\>PERSISTTEST
```

El primer `PERSISTTEST` debería guardar `1`; tras reiniciar, `TYPE PERSIS.TXT` debería mostrar `1`, y el siguiente `PERSISTTEST` subir a `2`. Esto comprueba persistencia **solo si se observa realmente en QEMU/OVMF o hardware**. La ISO original **no cambia**.

Para comprobar BIOS/CSM:

```bash
python3 tools/run.py --fresh
```

En modo BIOS también existe la unidad temporal `C:` RAM, directorios y comandos BAT; la ruta UEFI se centra en D: raíz 8.3.

## Instalar en una memoria USB de pruebas con Rufus

1. **Respaldar todo el contenido**; Rufus sobrescribe la memoria elegida.
2. Seleccionar `RodOS-0.1.3-BIOS-UEFI-USB.iso` y grabar en **modo DD**.
3. Probar primero **UEFI x64 con Secure Boot desactivado**: `BOOTX64.EFI` no está firmado.
4. Seleccionar el dispositivo USB en el menú de arranque del firmware.
5. Ejecutar `PERSISTTEST`, reiniciar, `TYPE PERSIS.TXT`. No guardar documentos importantes.

En BIOS Legacy/CSM debe conservarse la compatibilidad con la ruta anterior, pero no se ha verificado aquí en una máquina virtual.

### Límites

- En UEFI, el firmware proporciona `SimpleFileSystem`; no se desarrolló un nuevo controlador xHCI/NVMe/USB. Algunos firmwares no exponen todas las particiones de una memoria USB.
- UEFI ofrece consola, `HELP`, `VER`, `DIR`, `TYPE`, `ECHO >`, `ECHO >>`, `EDIT` (una línea), `COPY`, `REN`, `DEL`, `PERSISTTEST`, `REBOOT`, `EXIT` y `USBINFO`; por ahora solo `D:` raíz FAT16, nombres 8.3, texto de hasta 4095 bytes.
- BIOS preserva los comandos previos y la unidad `C:` temporal.
- Imagen para USB **DD**, no CD/DVD UEFI ni instalador en disco duro; `Secure Boot` no es compatible.
- FAT16 carece de journal. **No retirar el USB ni cortar la electricidad mientras se escribe.** El guardado no está garantizado ante cortes de energía.
- El PE32+ EFI, FAT16 y la escritura simulada han pasado pruebas estructurales, pero **no se ha probado ejecución real de UEFI**, ni `PERSISTTEST` en QEMU/OVMF o USB físico en este entorno.

## Estructura

- `boot/`: MBR BIOS, salto a kernel de 32 bits, puente BIOS EDD.
- `kernel/`: código C++17 BIOS, FAT16 por sectores y RAMFS.
- `uefi/`: aplicación UEFI x64 freestanding C++ y definiciones ABI EFI.
- `tools/build_usb_iso.py`: crea imagen MBR + ESP + FAT16 de datos + zona ISO9660.
- `tools/run_uefi.py`: ejecuta QEMU/OVMF contra copia persistente de la imagen.
- `tests/`: pruebas de FAT16, RAMFS y validación de imagen UEFI.
- `docs/VALIDACION-0.1.3.md`: evidencia verificable y límites.

**Licencia MIT** para las fuentes de RodOS (`LICENSE`). RodOS no ejecuta programas `.COM` ni `.EXE` de MS-DOS.
