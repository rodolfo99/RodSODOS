#include "shell.hpp"
#include "hardware.hpp"
namespace {
void error(fs::Error e) { if (e!=fs::Error::Ok) { console::print("Error: "); console::line(fs::message(e)); } }
bool command(const char* a,const char* b) { return str::ieq(a,b); }
int tokenize(char* text,char** args,int capacity) {
    int count=0; char* p=text;
    while (*p) {
        while (*p==' '||*p=='\t') ++p;
        if (!*p) break;
        if (count==capacity) return -1;
        if (*p=='"') {
            args[count++]=++p;
            while (*p && *p!='"') ++p;
            if (!*p) return -1;
            *p++=0;
            if (*p && *p!=' '&&*p!='\t') return -1;
        } else {
            args[count++]=p;
            while (*p&&*p!=' '&&*p!='\t') ++p;
            if (*p) *p++=0;
        }
    }
    return count;
}
bool arity(int actual,int expected,const char* usage) {
    if (actual==expected) return true;
    console::print("Uso: "); console::line(usage); return false;
}
int hex(char c) {
    c=str::upper(c); if (c>='0'&&c<='9') return c-'0';
    return c>='A'&&c<='F' ? c-'A'+10 : -1;
}
void two(u32 n) { if (n<10) console::put('0'); console::number(n); }
const char helpText[] =
    "HELP / AYUDA           Muestra estos comandos\n"
    "DIR [ruta]             Lista archivos y directorios\n"
    "CD [ruta] / CD.. / CD\\ Cambia de directorio; sin ruta lo muestra\n"
    "MD ruta / MKDIR ruta   Crea un directorio\n"
    "RD ruta / RMDIR ruta   Borra un directorio vacio\n"
    "TREE [ruta]            Muestra el arbol de directorios\n"
    "TYPE archivo           Muestra un archivo de texto\n"
    "ECHO texto             Imprime texto\n"
    "ECHO texto > archivo   Crea o reemplaza un archivo\n"
    "ECHO texto >> archivo  Agrega texto al final\n"
    "COPY origen destino    Copia un archivo (reemplaza destino)\n"
    "REN ruta nombre        Cambia el nombre dentro del directorio\n"
    "DEL archivo            Borra un archivo\n"
    "EDIT archivo           Reemplaza texto; . guarda, .CANCEL cancela\n"
    "CALL archivo.BAT       Ejecuta un guion; tambien: DEMO\n"
    "CLS / CLEAR            Limpia la pantalla\n"
    "COLOR BF               Color hexadecimal: fondo B, texto F\n"
    "VER / ABOUT / MEM      Version, informacion y uso de RAM\n"
    "TIME / DATE / UPTIME   Hora RTC, fecha y tiempo encendido\n"
    "PAUSE / REM texto      Espera una tecla / comentario BAT\n"
    "REBOOT / SHUTDOWN      Reinicia / apaga la maquina virtual\n"
    "C: RAM temporal | D: FAT16 USB persistente (raiz 8.3)\n"
    "USBINFO                 Muestra estado de la unidad USB\n";
}
bool Shell::isUsb(const char* path) const {
    if (path && path[0] && path[1]==':') return str::upper(path[0])=='D';
    return onUsb_;
}
const char* Shell::usbName(const char* path) const {
    if (path && path[0] && path[1]==':') path+=2;
    while (str::separator(*path)) ++path;
    return path;
}
fs::Error Shell::readText(const char* path,char* out,u32& size) {
    size=0;
    if (isUsb(path)) return usb_.read(usbName(path),out,fs::FileCapacity,size);
    int index=disk_.resolve(path);
    if (index<0) return fs::Error::NotFound;
    const auto& n=disk_.node(index);
    if (n.directory) return fs::Error::IsDirectory;
    size=n.size;str::copy(out,n.data,fs::FileCapacity);return fs::Error::Ok;
}
fs::Error Shell::writeText(const char* path,const char* text,bool append) {
    if (isUsb(path)) return usb_.write(usbName(path),text,u32(str::len(text)),append);
    return disk_.write(path,text,append);
}
void Shell::listUsb() {
    if (!usb_.mounted()) {error(fs::Error::IoError);return;}
    console::line(" Directorio de D:\\ (FAT16 USB)");
    u32 count=0,bytes=0;
    for (u32 i=0;i<usb_.rootEntries();++i) {
        fat::FileInfo e;
        if(!usb_.entry(u16(i),e)) {error(fs::Error::IoError);return;}
        if (!e.used) continue;
        console::put(' ');console::print(e.name);
        for (usize p=str::len(e.name);p<15;++p)console::put(' ');
        if(e.directory) console::line("<DIR>");
        else {console::number(e.size);console::line(" bytes");bytes+=e.size;++count;}
    }
    console::put(' ');console::number(count);console::print(" archivo(s), ");
    console::number(bytes);console::line(" bytes.");
}
void Shell::init() {
    disk_.init(); batchEcho_=true; onUsb_=false; usb_.init();
    disk_.mkdir("DOCS");
    disk_.write("README.TXT",
        "Bienvenido a RodOS 0.1.3.\n"
        "Este PC arranco un kernel propio, escrito en C++ sin biblioteca estandar.\n"
        "La pequena parte de ensamblador configura la CPU y las interrupciones.\n"
        "\nPruebe: MD TRABAJO, CD TRABAJO, ECHO Hola > NOTA.TXT, TYPE NOTA.TXT.\n"
        "EDIT reemplaza todo el texto. Una linea . guarda; .CANCEL cancela.\n"
        "\nC: usa RAM y D: usa FAT16 USB persistente (si BIOS la detecta).\n"
        "No ejecuta .EXE/.COM de MS-DOS. Sin red ni multitarea.\n"
        "Teclado PS/2: distribucion estadounidense (US), texto ASCII.\n");
    disk_.write("DOCS\\GUIA.TXT",
        "C: RAM con 64 entradas (incluida la raiz), 4095 bytes por archivo.\n"
        "Rutas de hasta 126 caracteres; nombres 8.3: letras, numeros, _ y -.\n"
        "No hay comodines, tuberias, variables, GOTO ni parametros BAT.\n"
        "Los archivos BAT ejecutan comandos en orden; CALL anida hasta 4.\n"
        "ECHO admite > y >>. EDIT crea texto y guiones BAT linea a linea.\n"
        "Las flechas arriba/abajo recuerdan los ultimos ocho comandos.\n"
        "Ctrl+C cancela la entrada actual. El reloj usa la hora del RTC virtual.\n");
    disk_.write("DEMO.BAT",
        "@REM Ejemplo que no modifica archivos.\n"
        "@ECHO OFF\n"
        "ECHO Bienvenido a tu sistema operativo en C++!\n"
        "VER\n"
        "DIR\n"
        "ECHO Escribe TYPE README.TXT para conocerlo.\n");
}
void Shell::prompt() {
    char path[fs::PathCapacity]; disk_.path(disk_.cwd(),path,sizeof(path));
    if (onUsb_) console::print("D:\\"); else console::print(path); console::put('>');
}
[[noreturn]] void Shell::run() {
    console::banner(); char line[256];
    for (;;) { prompt(); if (console::readLine(line,sizeof(line))) execute(line); }
}
void Shell::list(const char* path) {
    if (isUsb(path)) { listUsb(); return; }
    int index=disk_.resolve(path);
    if (index<0) { error(fs::Error::NotFound); return; }
    if (!disk_.node(index).directory) { error(fs::Error::NotDirectory); return; }
    char full[fs::PathCapacity]; disk_.path(index,full,sizeof(full));
    console::print(" Directorio de "); console::line(full);
    u32 files=0,dirs=0,bytes=0;
    for (int i=1; i<fs::MaxNodes; ++i) {
        const auto& n=disk_.node(i);
        if (!n.used || n.parent!=index) continue;
        console::put(' '); console::print(n.name);
        for (usize c=str::len(n.name); c<15; ++c) console::put(' ');
        if (n.directory) { console::line("<DIR>"); ++dirs; }
        else { console::number(n.size); console::line(" bytes"); ++files; bytes+=n.size; }
    }
    console::put(' '); console::number(files); console::print(" archivo(s), ");
    console::number(dirs); console::print(" directorio(s), "); console::number(bytes); console::line(" bytes.");
}
void Shell::tree(int index,int depth) {
    for (int i=1; i<fs::MaxNodes; ++i) {
        const auto& n=disk_.node(i);
        if (!n.used || n.parent!=index) continue;
        for (int k=0; k<depth; ++k) console::print("  ");
        console::print(n.directory ? "[+] " : "    "); console::line(n.name);
        if (n.directory) tree(i,depth+1);
    }
}
void Shell::edit(const char* path) {
    int old=isUsb(path) ? -1 : disk_.resolve(path);
    if (old>=0 && disk_.node(old).directory) { error(fs::Error::IsDirectory); return; }
    console::line("EDITOR: escriba el NUEVO contenido (reemplaza el anterior).");
    console::line("Una linea . guarda; .CANCEL o Ctrl+C cancela. Maximo 4095 bytes.");
    console::status(" EDITOR  |  . guarda  |  .CANCEL cancela  |  D: = persistente");
    char text[fs::FileCapacity]; char line[256]; usize n=0; text[0]=0;
    for (;;) {
        console::print("| ");
        if (!console::readLine(line,sizeof(line),false) || command(line,".CANCEL")) {
            console::line("Edicion cancelada; archivo original conservado."); break;
        }
        if (str::eq(line,".")) {
            fs::Error e=writeText(path,text);
            if (e==fs::Error::Ok) console::line(isUsb(path)?"Archivo guardado en USB FAT16.":"Archivo guardado en RAM."); else error(e);
            break;
        }
        usize size=str::len(line);
        if (n+size+1>=sizeof(text)) { console::line("Sin espacio: esa linea no se agrego. Use . para guardar."); continue; }
        str::copy(text+n,line,sizeof(text)-n); n+=size; text[n++]='\n'; text[n]=0;
    }
    console::status(" HELP: ayuda  |  C: RAM temporal  |  D: FAT16 persistente USB");
}
void Shell::batch(int index,int depth) {
    if (depth>=4) { console::line("Error: limite de 4 niveles BAT alcanzado."); return; }
    if (disk_.node(index).directory) { error(fs::Error::IsDirectory); return; }
    batchText(disk_.node(index).data,depth);
}
void Shell::batchText(const char* content,int depth) {
    char script[fs::FileCapacity];str::copy(script,content,sizeof(script));
    bool savedEcho=batchEcho_;
    char* p=script;
    while (*p) {
        char line[256]; usize n=0; bool tooLong=false;
        while (*p && *p!='\n') { if (n+1<sizeof(line)) line[n++]=*p; else tooLong=true; ++p; }
        if (*p=='\n') ++p;
        line[n]=0;
        if (tooLong) { console::line("Error: linea BAT mayor de 255 caracteres; guion detenido."); break; }
        char* commandLine=str::trim(line);
        if (!*commandLine) continue;
        bool silent=*commandLine=='@'; if (silent) ++commandLine;
        if (batchEcho_ && !silent) { prompt(); console::line(commandLine); }
        execute(commandLine,depth+1);
    }
    batchEcho_=savedEcho;
}
void Shell::execute(char* raw,int depth) {
    char* start=str::trim(raw); if (!*start) return;
    char* rest=start; while (*rest && *rest!=' '&&*rest!='\t') ++rest;
    if (*rest) *rest++=0;
    rest=str::trim(rest);
    if (command(start,"REM")) return;
    if (command(start,"ECHO")) {
        char* redirect=rest; bool quoted=false;
        for (; *redirect; ++redirect) { if (*redirect=='"') quoted=!quoted; if (*redirect=='>'&&!quoted) break; }
        if (*redirect) {
            *redirect++=0; bool append=*redirect=='>'; if (append) ++redirect;
            char* target[1]; int count=tokenize(str::trim(redirect),target,1);
            if (count!=1) { console::line("Uso: ECHO texto > archivo (o >> archivo)"); return; }
            char text[258]; char* content=str::trim(rest);
            str::copy(text,content,sizeof(text)); usize size=str::len(text); text[size++]='\n'; text[size]=0;
            error(writeText(target[0],text,append));
        } else if (depth && command(rest,"OFF")) batchEcho_=false;
        else if (depth && command(rest,"ON")) batchEcho_=true;
        else console::line(rest);
        return;
    }
    char* args[3]; int argc=tokenize(rest,args,3);
    if (argc<0) { console::line("Error: demasiados argumentos o comillas sin cerrar."); return; }
    if (command(start,"HELP")||command(start,"AYUDA")) { console::print(helpText); return; }
    if (command(start,"CLS")||command(start,"CLEAR")) { console::clear(); return; }
    if (command(start,"VER")) { console::line("RodOS version 0.1.3 | kernel C++17 x86 de 32 bits"); return; }
    if (command(start,"ABOUT")) {
        console::line("RodOS: un sistema educativo independiente, inspirado en la consola DOS.");
        console::line("BIOS -> cargador propio -> modo protegido -> kernel C++ -> shell.");
        console::line("VGA, PS/2, UART, PIT, RTC, RAMFS y FAT16 USB via BIOS (UEFI x64 via BOOTX64.EFI)."); return;
    }
    if (command(start,"DIR")) { if (argc<=1) list(argc?args[0]:""); else arity(argc,1,"DIR [ruta]"); return; }
    if (command(start,"CD..")||command(start,"CD\\")||command(start,"CD/")) {
        if (arity(argc,0,"CD.. o CD\\") && !onUsb_) error(disk_.chdir(command(start,"CD..")?"..":"\\"));
        return;
    }
    if (command(start,"CD")||command(start,"CHDIR")) {
        if (!argc) { if(onUsb_) console::line("D:\\"); else {char path[fs::PathCapacity]; disk_.path(disk_.cwd(),path,sizeof(path));console::line(path);} }
        else if (arity(argc,1,"CD [ruta]")) {
            if (isUsb(args[0])) {
                const char* p=usbName(args[0]);
                if (!*p || str::eq(p,".") || str::eq(p,"..")) onUsb_=true;
                else error(fs::Error::Unsupported);
            } else {fs::Error e=disk_.chdir(args[0]);if(e==fs::Error::Ok)onUsb_=false;else error(e);}
        }
        return;
    }
    if (command(start,"C:")) {onUsb_=false;return;}
    if (command(start,"D:")) {
        if (usb_.mounted()) onUsb_=true;
        else console::line("D: no disponible. Se requiere USB BIOS/CSM con FAT16.");
        return;
    }
    if (command(start,"USBINFO")) {
        if (!usb_.mounted()) {console::line("D: no detectada o sin particion FAT16 valida.");return;}
        console::print("D: FAT16 | LBA ");console::number(usb_.partitionStart());
        console::print(" | sectores ");console::number(usb_.partitionSectors());
        console::print(" | clusters ");console::number(usb_.clusterCount());console::put('\n');return;
    }
    if (command(start,"MD")||command(start,"MKDIR")) { if (arity(argc,1,"MD ruta")) error(isUsb(args[0])?fs::Error::Unsupported:disk_.mkdir(args[0])); return; }
    if (command(start,"RD")||command(start,"RMDIR")) { if (arity(argc,1,"RD ruta")) error(isUsb(args[0])?fs::Error::Unsupported:disk_.remove(args[0],true)); return; }
    if (command(start,"DEL")||command(start,"ERASE")) { if (arity(argc,1,"DEL archivo")) error(isUsb(args[0])?usb_.remove(usbName(args[0])):disk_.remove(args[0],false)); return; }
    if (command(start,"COPY")) {
        if (arity(argc,2,"COPY origen destino")) {
            if (!isUsb(args[0]) && !isUsb(args[1])) {
                fs::Error e=disk_.copy(args[0],args[1]);if(e==fs::Error::Ok)console::line("1 archivo copiado.");else error(e);
            } else {
                char buffer[fs::FileCapacity];u32 size=0;fs::Error e=readText(args[0],buffer,size);
                if (e==fs::Error::Ok) {
                    char destination[fs::PathCapacity];str::copy(destination,args[1],sizeof(destination));
                    usize n=str::len(destination);
                    if (n && (destination[n-1]==':' || str::separator(destination[n-1]))) {
                        const char* name=args[0];
                        for (const char* p=args[0];*p;++p) if (str::separator(*p)||*p==':') name=p+1;
                        if (n+str::len(name)>=sizeof(destination)) e=fs::Error::InvalidPath;
                        else str::copy(destination+n,name,sizeof(destination)-n);
                    }
                    if (e==fs::Error::Ok) e=isUsb(destination)?usb_.write(usbName(destination),buffer,size):disk_.write(destination,buffer);
                }
                if (e==fs::Error::Ok)console::line("1 archivo copiado.");else error(e);
            }
        }return;
    }
    if (command(start,"REN")||command(start,"RENAME")) { if (arity(argc,2,"REN ruta nombre")) error(isUsb(args[0])?usb_.rename(usbName(args[0]),args[1]):disk_.rename(args[0],args[1])); return; }
    if (command(start,"TYPE")) {
        if (!arity(argc,1,"TYPE archivo")) return;
        char data[fs::FileCapacity];u32 size=0;
        fs::Error e=readText(args[0],data,size);
        if (e!=fs::Error::Ok) error(e);
        else {console::print(data);if(size && data[size-1]!='\n')console::put('\n');}
        return;
    }
    if (command(start,"EDIT")) { if (arity(argc,1,"EDIT archivo")) edit(args[0]); return; }
    if (command(start,"TREE")) {
        if (argc>1) { arity(argc,1,"TREE [ruta]"); return; }
        if (isUsb(argc?args[0]:"")) {error(fs::Error::Unsupported);return;}
        int i=disk_.resolve(argc?args[0]:"");
        if (i<0) error(fs::Error::NotFound);
        else if (!disk_.node(i).directory) error(fs::Error::NotDirectory);
        else { char path[fs::PathCapacity]; disk_.path(i,path,sizeof(path)); console::line(path); tree(i); }
        return;
    }
    if (command(start,"MEM")) {
        console::print("RAMFS: "); console::number(u32(disk_.count())); console::line(" / 64 entradas usadas (incluye raiz).");
        console::print("Texto almacenado: "); console::number(disk_.bytes()); console::line(" bytes; maximo 4095 por archivo.");
        console::print("Reserva fija RAMFS: "); console::number(sizeof(fs::RamFS)); console::line(" bytes.");
        console::line("Sin asignacion dinamica; MEM informa del RAMFS, no de toda la RAM del PC."); return;
    }
    if (command(start,"TIME")||command(start,"DATE")) {
        u8 h,m,s,d,month; u16 year;
        if (!hw::clock(h,m,s,d,month,year)) { console::line("No se pudo leer el RTC."); return; }
        if (command(start,"TIME")) { two(h); console::put(':'); two(m); console::put(':'); two(s); }
        else { two(d); console::put('/'); two(month); console::put('/'); console::number(year); }
        console::put('\n'); return;
    }
    if (command(start,"UPTIME")) { console::number(hw::ticks()/100); console::line(" segundos desde el arranque."); return; }
    if (command(start,"COLOR")) {
        if (!arity(argc,1,"COLOR BF (ejemplo: COLOR 0A)")) return;
        if (str::len(args[0])!=2 || hex(args[0][0])<0 || hex(args[0][0])>7 || hex(args[0][1])<0 || hex(args[0][0])==hex(args[0][1]))
            console::line("Color invalido: fondo 0-7 y texto 0-F deben ser distintos.");
        else console::color(u8(hex(args[0][0])*16+hex(args[0][1])));
        return;
    }
    if (command(start,"PAUSE")) { console::line("Pulse una tecla para continuar..."); while (!hw::key()) hw::wait(); return; }
    if (command(start,"REBOOT")) { hw::reboot(); }
    if (command(start,"SHUTDOWN")) { hw::shutdown(); }
    if (command(start,"CALL")) {
        if (!arity(argc,1,"CALL archivo.BAT")) return;
        usize n=str::len(args[0]);
        if (n<4 || !str::ieq(args[0]+n-4,".BAT")) { console::line("CALL requiere un archivo .BAT."); return; }
        if (isUsb(args[0])) {char data[fs::FileCapacity];u32 bytes=0;fs::Error e=readText(args[0],data,bytes);if(e==fs::Error::Ok)batchText(data,depth);else error(e);}
        else {int i=disk_.resolve(args[0]);if(i<0)error(fs::Error::NotFound);else batch(i,depth);}return;
    }
    if (!argc) {
        char path[fs::PathCapacity]; usize size=str::len(start);
        if (size+4<sizeof(path)) {
            str::copy(path,start,sizeof(path));
            if (size<4 || !str::ieq(path+size-4,".BAT")) str::copy(path+size,".BAT",sizeof(path)-size);
            if (isUsb(path)) {char data[fs::FileCapacity];u32 bytes=0;if (readText(path,data,bytes)==fs::Error::Ok){batchText(data,depth);return;}}
            else {int i=disk_.resolve(path);if(i>=0){batch(i,depth);return;}}
        }
    }
    console::print("Comando no reconocido: "); console::line(start);
    console::line("Escriba HELP para ver los comandos disponibles.");
}
