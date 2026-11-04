# RodOS 0.1.2 · sistema operativo educativo tipo MS-DOS en C++

**RodOS no utiliza Linux ni FreeDOS.** Es un kernel independiente C++17 freestanding
para x86 de 32 bits, con consola de comandos DOS, VGA, PS/2, UART, PIT, RTC,
sistema de archivos RAM y acceso FAT16 a la unidad que lo arrancó a través de
la BIOS (INT 13h Extensions). La pequeña parte de ensamblador es necesaria para
arrancar y pasar entre modo protegido y modo real durante el acceso al disco.

## Cambios desde RodOS 0.1.0 y 0.1.1

- `C:` conserva los comandos existentes, pero **es temporal** (RAM).
- `D:` es una **partición FAT16 de datos de 16 MiB**, separada del área ISO.
  Los archivos de `D:` están destinados a persistir al reiniciar.
- `DIR`, `TYPE`, `ECHO >`, `ECHO >>`, `EDIT`, `DEL`, `REN`, `COPY` y `CALL`
  pueden operar sobre archivos de texto de `D:`; `COPY` permite C:↔D:.
- Se añadió el comando `USBINFO` y una imagen ISO9660 + MBR para Rufus **DD**.
- El kernel lee/escribe sectores por una rutina BIOS EDD controlada (sin escribir
  fuera de la partición FAT16). Esto depende de la compatibilidad del firmware.

**Limitación de D:** esta versión solo administra archivos 8.3 de texto en el
**directorio raíz**, con un máximo de 4095 bytes por archivo. Puede listar
subdirectorios FAT16 existentes, pero no crearlos, recorrerlos ni modificarlos.
Los archivos binarios no están admitidos por el shell de texto. `C:` continúa
admitiendo carpetas y archivos temporales en RAM.

## Usar con Rufus: USB real (solo BIOS legado o UEFI-CSM)

1. Haz copia de seguridad de la memoria USB: **Rufus borrará su contenido**.
2. Abre Rufus y selecciona `RodOS-0.1.2-USB-FAT16.iso`.
3. Cuando Rufus lo permita, selecciona **modo DD** (escritura byte a byte):
   **NO** modo ISO (extracción de archivos). No modifiques las particiones.
4. Arranca el equipo desde USB con **Legacy BIOS/CSM** habilitado.
5. Si arranca, usa `USBINFO`, `D:`, `DIR`, `TYPE LEEME.TXT`, crea una nota y
   reinicia para comprobar la persistencia. En Windows, FAT16 se puede leer
   como una partición de datos.

**Advertencias importantes:** esta imagen **no arranca en UEFI puro** ni con
Secure Boot. Requiere que el firmware presente la USB como disco BIOS con
INT 13h extensions (EDD). Algunos equipos modernos no tienen CSM o bloquean
las escrituras BIOS; en ellos no funcionará o `D:` no estará disponible.
La ISO9660 en sí es de solo lectura: **solo se modifica la partición FAT16**.
No contiene instalador ni ejecuta programas `.EXE` o `.COM` de MS-DOS.
**No confíes datos importantes a esta versión experimental.**

## Ejemplo de comandos

```text
VER
HELP
DIR
TYPE README.TXT
USBINFO
D:
DIR
TYPE LEEME.TXT
ECHO Hola desde RodOS > NOTA.TXT
TYPE NOTA.TXT
ECHO Una segunda linea >> NOTA.TXT
EDIT MIARCH.TXT
REN NOTA.TXT NUEVA.TXT
COPY NUEVA.TXT C:\RESP.TXT
C:
TYPE RESP.TXT
COPY RESP.TXT D:\COPIA.TXT
D:
DEL COPIA.TXT
REBOOT
D:
TYPE NUEVA.TXT
```

## Probar en QEMU sin USB físico

Instala QEMU en Ubuntu o Debian:

```bash
sudo apt install qemu-system-x86 python3
```

Desde la carpeta del proyecto:

```bash
python3 tools/run.py --terminal
```

Se crea una copia de la ISO llamada `build/RodOS-0.1.2-VM-persistente.img`,
que QEMU usa como disco IDE grabable. Los cambios en `D:` quedan en esta copia
virtual, **no en la ISO original**. Para recrearla desde cero (borra SOLO los
archivos de prueba de esa copia de máquina virtual):

```bash
python3 tools/run.py --fresh --terminal
```

Para la interfaz VGA, omite `--terminal`. `iniciar.sh` o `iniciar.bat` son
lanzadores equivalentes. No se ha podido ejecutar QEMU en el entorno de
preparación; el arranque real **sigue pendiente de comprobarse**.

## Compilar los fuentes

En Linux x86-64 (Ubuntu/Debian) instala `g++`, `binutils`, `make` y `python3`:

```bash
sudo apt install build-essential binutils python3
make -j2
make test
```

`make` construye `build/RodOS-0.1.2-USB-FAT16.iso`; `make test` ejecuta las
pruebas de RAMFS y FAT16 en el anfitrión con sanitizadores. Estas pruebas
validan la lógica C++ y la estructura de la ISO, **no** el puente de BIOS ni
el funcionamiento en hardware USB. `build/rodos.img` y `boot/boot.S` se
conservan como alternativa de disquete de la base original; no tienen soporte
persistente real por sí solos y **no** deben usarse en Rufus para este objetivo.

Para revisar integridad:

```bash
cd build
sha256sum -c RodOS-0.1.2-USB-FAT16.iso.sha256
```

## Componentes principales

- `boot/usb_mbr.S`: MBR cargador BIOS con lecturas EDD del kernel en LBA256.
- `boot/biosdisk.S`: puente controlado protected mode → real mode → BIOS.
- `boot/entry.S`: entrada de CPU de 32 bits, BSS, interrupciones.
- `kernel/block.*`: lecturas/escrituras BIOS de un sector.
- `kernel/fat16.*`: volumen FAT16, raíz, lectura y escritura, FAT duplicadas.
- `kernel/fs.*`: sistema de archivos temporal C: (original RodOS).
- `kernel/shell.*`: intérprete DOS, editor, BAT y selección de unidad.
- `tools/build_usb_iso.py`: genera ISO9660 mínima + MBR + FAT16 de datos.
- `tests/fat16_test.cpp`: pruebas FAT16 sobre copia virtual de la ISO.
- `tests/fs_test.cpp`: pruebas originales de RAMFS.

## Validación

Lee `docs/VALIDACION-0.1.2.md`. Las pruebas C++ se completaron en Linux x86-64,
pero **no se afirma** haber arrancado QEMU ni un USB real. La escritura USB
es experimental y requiere validación de firmware BIOS/CSM.

Licencia del código original y de los cambios: MIT (`LICENSE`).
RodOS es independiente de Microsoft y no ofrece compatibilidad binaria con MS-DOS.
