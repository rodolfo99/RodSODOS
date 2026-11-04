// RodOS 0.1.3: UEFI x64, shell tipo DOS, particion FAT16 marcada RODOS.DAT.
// The EFI application stays in Boot Services; it is NOT the 32-bit BIOS kernel.
#include "efi.hpp"
namespace {
EFI_SYSTEM_TABLE* sys=nullptr;
EFI_FILE_PROTOCOL* data_root=nullptr;
using LocateHandles = EFI_STATUS (EFIAPI *)(int, const EFI_GUID*,void*,UINTN*,EFI_HANDLE**);
using HandleProtocol = EFI_STATUS (EFIAPI *)(EFI_HANDLE,const EFI_GUID*,void**);
using FreePool = EFI_STATUS (EFIAPI *)(void*);
using WaitForEvent = EFI_STATUS (EFIAPI *)(UINTN, EFI_EVENT*,UINTN*);
using ResetSystem = void (EFIAPI *)(int, EFI_STATUS,UINTN,void*);
// No libc or dynamic allocation: fixed buffers and explicit bounds.
char linebuf[256]={};
char filebuf[4096]={};
char auxbuf[4096]={};
int length(const char* str) {int i=0;while(str[i] && i<4096)++i;return i;}
char upper(char c) {return c>='a'&&c<='z'?char(c-32):c;}
bool equal(const char* a,const char* b) {for(;*a && *b;++a,++b)if(upper(*a)!=upper(*b))return false;return !*a && !*b;}
bool starts(const char* s,const char* prefix) {for(;*prefix;++s,++prefix)if(upper(*s)!=upper(*prefix))return false;return true;}
void print(const char* s) {
    CHAR16 buf[128];
    while(*s) {
        int n=0;
        while(*s && n<123) {
            char c=*s++;
            if(c=='\n') {buf[n++]=u'\r';if(n>=123)break;}
            buf[n++]=(uint8_t)c;
        }
        buf[n]=0;
        sys->con_out->OutputString(sys->con_out,buf);
    }
}
void println(const char* s) {print(s);print("\n");}
void print_number(uint64_t x) {
    char b[24];int i=0;
    do {b[i++]=char('0'+x%10);x/=10;} while(x && i<23);
    while(i--) {char c[2]={b[i],0};print(c);}
}
void print_wide(const CHAR16* text) {
    char out[128];int n=0;
    for(int i=0;text[i] && i<120;++i) {
        char c=text[i]<=127?char(text[i]):'?';out[n++]=c;
    }
    out[n]=0;print(out);
}
void read_line(char* out,int capacity) {
    int n=0;out[0]=0;
    auto wait=reinterpret_cast<WaitForEvent>(sys->boot_services->entry[9]);
    while(true) {
        EFI_INPUT_KEY key={};
        EFI_STATUS s=sys->con_in->ReadKeyStroke(sys->con_in,&key);
        if(!ok(s)) {
            EFI_EVENT ev=sys->con_in->WaitForKey;UINTN ix=0;
            if(ev)wait(1,&ev,&ix);
            continue;
        }
        uint16_t c=key.unicode_char;
        if(c==13) {print("\n");out[n]=0;return;}
        if(c==8 && n>0) {--n;out[n]=0;print("\b \b");continue;}
        if(c>=32 && c<127 && n+1<capacity) {
            out[n++]=char(c);out[n]=0;
            char t[2]={char(c),0};print(t);
        }
    }
}
char* strip(char* s) {
    while(*s==' '||*s=='\t')++s;
    int n=length(s);
    while(n>0&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'))s[--n]=0;
    return s;
}
bool path(const char* input,CHAR16 out[16]) {
    if(starts(input,"D:\\"))input+=3;
    else if(starts(input,"D:"))input+=2;
    // Avoid traversal or accessing other volumes; the shell only handles root 8.3.
    int name=0,ext=0,pos=0;
    if(!*input)return false;
    while(*input && *input!='.') {
        char c=upper(*input++);
        if(!(c>='A'&&c<='Z')&&!(c>='0'&&c<='9')&&c!='_'&&c!='-'&&c!='$'&&c!='~')return false;
        if(++name>8)return false;
        out[pos++]=CHAR16(c);
    }
    if(*input=='.') {
        ++input;if(!*input)return false;
        out[pos++]=u'.';
        while(*input) {
            char c=upper(*input++);
            if(!(c>='A'&&c<='Z')&&!(c>='0'&&c<='9')&&c!='_'&&c!='-'&&c!='$'&&c!='~')return false;
            if(++ext>3)return false;
            out[pos++]=CHAR16(c);
        }
    }
    out[pos]=0;return true;
}
bool protected_file(const char* s) {
    if(starts(s,"D:\\"))s+=3;
    else if(starts(s,"D:"))s+=2;
    return equal(s,"RODOS.DAT");
}
EFI_FILE_PROTOCOL* open(const char* name, uint64_t mode) {
    if(!data_root)return nullptr;
    CHAR16 wide[16];if(!path(name,wide))return nullptr;
    EFI_FILE_PROTOCOL* file=nullptr;
    EFI_STATUS s=data_root->Open(data_root,&file,wide,mode,EFI_FILE_ARCHIVE);
    return ok(s)?file:nullptr;
}
bool read(const char* name, char* out,UINTN& count) {
    count=0;
    EFI_FILE_PROTOCOL* file=open(name,EFI_FILE_MODE_READ);
    if(!file)return false;
    UINTN got=sizeof(filebuf)-1;
    EFI_STATUS s=file->Read(file,&got,out);
    if(ok(s) && got<sizeof(filebuf)) {
        // Refuse files larger than 4095 bytes instead of returning partial data.
        UINTN extra=1;char test=0;
        EFI_STATUS chk=file->Read(file,&extra,&test);
        if(!ok(chk)||extra!=0)s=1;
        else {count=got;out[count]=0;}
    } else s=1;
    file->Close(file);
    return ok(s);
}
bool trunc(EFI_FILE_PROTOCOL* file,uint64_t size) {
    alignas(8) uint8_t raw[512]={};UINTN n=sizeof(raw);
    EFI_STATUS s=file->GetInfo(file,&FILE_INFO_GUID,&n,raw);
    if(!ok(s)||n>sizeof(raw))return false;
    auto* info=reinterpret_cast<EFI_FILE_INFO*>(raw);
    info->file_size=size;
    return ok(file->SetInfo(file,&FILE_INFO_GUID,n,raw));
}
bool write(const char* name,const char* content,UINTN count,bool append=false) {
    if(protected_file(name)||count>4095)return false;
    EFI_FILE_PROTOCOL* file=open(name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE);
    if(!file)return false;
    EFI_STATUS s=0;
    if(append) s=file->SetPosition(file,~uint64_t(0));
    else s=file->SetPosition(file,0);
    UINTN written=count;
    if(ok(s))s=file->Write(file,&written,content);
    if(!ok(s)||written!=count){file->Close(file);return false;}
    if(!append && !trunc(file,count)) {file->Close(file);return false;}
    s=file->Flush(file);
    EFI_STATUS c=file->Close(file);
    return ok(s)&&ok(c);
}
bool file_exists(const char* name) {
    EFI_FILE_PROTOCOL* f=open(name,EFI_FILE_MODE_READ);
    if(!f)return false;
    f->Close(f);return true;
}
bool remove(const char* name) {
    if(protected_file(name))return false;
    EFI_FILE_PROTOCOL* file=open(name,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE);
    if(!file)return false;
    alignas(8) uint8_t raw[512]={};UINTN n=sizeof(raw);
    EFI_STATUS s=file->GetInfo(file,&FILE_INFO_GUID,&n,raw);
    if(!ok(s)||n>sizeof(raw)||(reinterpret_cast<EFI_FILE_INFO*>(raw)->attribute & EFI_FILE_DIRECTORY)) {
        file->Close(file);return false;
    }
    // EFI_FILE_PROTOCOL.Delete closes the handle even on failure.
    return ok(file->Delete(file));
}
void list() {
    println("Directorio D:\\  (FAT16 / UEFI)");
    if(!data_root) {println("Unidad D: no encontrada.");return;}
    if(!ok(data_root->SetPosition(data_root,0))) {println("No se pudo posicionar el directorio.");return;}
    for(int i=0;i<512;++i) {
        alignas(8) uint8_t raw[1024]={}; UINTN n=sizeof(raw);
        EFI_STATUS s=data_root->Read(data_root,&n,raw);
        if(!ok(s)){println("Error leyendo directorio.");return;}
        if(n==0)break;
        if(n<offsetof(EFI_FILE_INFO,file_name)+2)continue;
        auto* info=reinterpret_cast<EFI_FILE_INFO*>(raw);
        if(info->file_name[0]==0)continue;
        print("  ");print_wide(info->file_name);
        if(info->attribute & EFI_FILE_DIRECTORY)print(" <DIR>");
        else {print("  ");print_number(info->file_size);print(" B");}
        print("\n");
    }
}
bool mount_data() {
    auto locate=reinterpret_cast<LocateHandles>(sys->boot_services->entry[36]);
    auto handle=reinterpret_cast<HandleProtocol>(sys->boot_services->entry[16]);
    auto free_pool=reinterpret_cast<FreePool>(sys->boot_services->entry[6]);
    UINTN count=0;EFI_HANDLE* handles=nullptr;
    if(!ok(locate(2,&SIMPLE_FS_GUID,nullptr,&count,&handles)))return false;
    for(UINTN i=0;i<count;++i) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs=nullptr;
        if(!ok(handle(handles[i],&SIMPLE_FS_GUID,reinterpret_cast<void**>(&fs)))||!fs)continue;
        EFI_FILE_PROTOCOL* root=nullptr;
        if(!ok(fs->OpenVolume(fs,&root))||!root)continue;
        EFI_FILE_PROTOCOL* marker=nullptr;
        EFI_STATUS s=root->Open(root,&marker,u"RODOS.DAT",EFI_FILE_MODE_READ,0);
        if(ok(s)&&marker) {
            char buf[24]={};UINTN n=sizeof(buf);
            s=marker->Read(marker,&n,buf);marker->Close(marker);
            if(ok(s) && n>=10 && starts(buf,"RODOS-DATA")) {
                data_root=root;break;
            }
        }
        root->Close(root);
    }
    if(handles)free_pool(handles);
    return data_root!=nullptr;
}
void persisttest() {
    char b[4096];UINTN n=0;uint64_t v=0;
    if(read("PERSIS.TXT",b,n)) {
        if(n==0||n>7) {println("PERSIS.TXT no es un contador de RodOS. Sin cambios.");return;}
        for(UINTN i=0;i<n;++i) {
            if(b[i]<'0'||b[i]>'9') {println("PERSIS.TXT no es numerico. Sin cambios.");return;}
            v=v*10+uint64_t(b[i]-'0');
        }
    }
    if(v>=9999999){println("Contador agotado.");return;}
    ++v;
    char tmp[16];int n2=0;uint64_t t=v;
    do{tmp[n2++]=char('0'+t%10);t/=10;}while(t);
    char txt[16];for(int i=0;i<n2;++i)txt[i]=tmp[n2-i-1];txt[n2]=0;
    if(!write("PERSIS.TXT",txt,UINTN(n2))) {println("ERROR: FAT16 no escribible / fallo de Flush.");return;}
    UINTN check=0;
    if(!read("PERSIS.TXT",b,check)||check!=UINTN(n2)||!equal(b,txt)) {println("ERROR: lectura posterior no coincide.");return;}
    print("PERSIS.TXT guardado y releido: ");println(txt);
    println("Reinicia, ejecuta TYPE PERSIS.TXT y repite PERSISTTEST.");
}
void command(char* input) {
    char* cmd=strip(input);if(!*cmd)return;
    if(equal(cmd,"HELP")||equal(cmd,"AYUDA")) {
        println("HELP VER CLS DIR USBINFO D: TYPE <nombre> ");
        println("ECHO texto > ARCH.TXT  |  ECHO texto >> ARCH.TXT");
        println("EDIT <nombre> | COPY <origen> <destino>");
        println("REN <viejo> <nuevo> | DEL <nombre> | PERSISTTEST");
        println("REBOOT | EXIT");return;
    }
    if(equal(cmd,"VER")) {println("RodOS 0.1.3 UEFI x64, DOS-style shell (no Linux)");return;}
    if(equal(cmd,"CLS")) {
        if(sys->con_out->ClearScreen)
            reinterpret_cast<EFI_STATUS(EFIAPI*)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)>(sys->con_out->ClearScreen)(sys->con_out);
        return;
    }
    if(equal(cmd,"DIR")) {list();return;}
    if(equal(cmd,"D:")) {println("D:\\ (FAT16 USB)");return;}
    if(equal(cmd,"USBINFO")) {println("UEFI x64 / SimpleFileSystem / volumen marcado RODOS.DAT");println("Unidad D: particion FAT16 persistente. Flush activo.");return;}
    if(equal(cmd,"PERSISTTEST")) {persisttest();return;}
    if(equal(cmd,"REBOOT")) {auto reset=reinterpret_cast<ResetSystem>(sys->runtime_services->entry[10]);reset(0,0,0,nullptr);return;}
    if(starts(cmd,"TYPE ")) {
        const char* filename=strip(cmd+5);UINTN n=0;
        if(!read(filename,filebuf,n))println("Error: archivo ausente, invalido o demasiado grande.");
        else {for(UINTN i=0;i<n;++i){char c[2]={filebuf[i],0};if(c[0]=='\r')continue;print(c);}print("\n");}
        return;
    }
    if(starts(cmd,"DEL ")) {println(remove(strip(cmd+4))?"Archivo eliminado.":"No se pudo eliminar.");return;}
    if(starts(cmd,"EDIT ")) {
        char* file=strip(cmd+5);CHAR16 wide[16];if(!path(file,wide)||protected_file(file)) {println("Nombre 8.3 invalido o protegido.");return;}
        println("Editor: introduce una linea (max 255 caracteres). Se sobrescribe.");
        print("Texto> ");read_line(auxbuf,256);
        println(write(file,auxbuf,UINTN(length(auxbuf)))?"Guardado (Flush completado).":"Error al guardar.");return;
    }
    if(starts(cmd,"ECHO ")) {
        char* p=cmd+5;char* sep=nullptr;bool append=false;
        for(char* q=p;*q;++q)if(*q=='>'){sep=q;break;}
        if(!sep) {println(p);return;}
        append=sep[1]=='>';
        char* fn=strip(sep+(append?2:1));
        while(sep>p && sep[-1]==' ')--sep;
        *sep=0;
        UINTN n=UINTN(length(p));
        if(append) {
            UINTN prior=0;
            if(read(fn,filebuf,prior)) {
                if(prior+n+2>4095) {println("Archivo demasiado grande.");return;}
                for(UINTN i=0;i<prior;++i)auxbuf[i]=filebuf[i];
                auxbuf[prior]='\r';auxbuf[prior+1]='\n';
                for(UINTN i=0;i<n;++i)auxbuf[prior+2+i]=p[i];
                println(write(fn,auxbuf,prior+n+2)?"Guardado.":"Error de escritura.");return;
            }
            if(file_exists(fn)) {println("Archivo no legible; no se sobrescribe.");return;}
        }
        println(write(fn,p,n)?"Guardado.":"Error de escritura.");return;
    }
    if(starts(cmd,"COPY ")||starts(cmd,"REN ")) {
        bool ren=starts(cmd,"REN ");char* args=strip(cmd+(ren?4:5));
        char* b=args;while(*b && *b!=' ')++b;
        if(!*b) {println("Uso: COPY/REN origen destino");return;}
        *b++=0;b=strip(b);
        CHAR16 a1[16],a2[16];
        if(!path(args,a1)||!path(b,a2)||protected_file(b)||protected_file(args) || equal(args,b)) {
            println("Nombres invalidos, protegidos o iguales.");return;
        }
        if(file_exists(b)) {println("El destino ya existe. No se sobrescribe.");return;}
        UINTN n=0;if(!read(args,filebuf,n)) {println("No se puede leer origen.");return;}
        if(!write(b,filebuf,n)) {println("No se puede escribir destino.");return;}
        if(ren && !remove(args)) {println("Se copio, pero no se pudo borrar origen.");return;}
        println(ren?"Renombrado.":"Copiado.");return;
    }
    println("Comando desconocido. HELP para ayuda.");
}
} // namespace
extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE* st) {
    if(!st||!st->boot_services||!st->runtime_services||!st->con_out||!st->con_in)return 1;
    sys=st;
    println("RodOS 0.1.3 | UEFI x64 | Consola estilo MS-DOS");
    println("Arranque UEFI nativo; no se ha iniciado el kernel BIOS de 32 bits.");
    if(!mount_data()) {
        println("ERROR: No se encontro volumen FAT con marcador RODOS.DAT.");
        println("No se escribira en ninguna unidad para evitar danos.");
        println("Presiona ENTER para regresar al firmware.");
        read_line(linebuf,sizeof(linebuf));return 1;
    }
    println("D: USB FAT16 detectada. HELP para ver comandos.");
    for(;;) {
        print("D:\\>");read_line(linebuf,sizeof(linebuf));
        char* cmd=strip(linebuf);
        if(equal(cmd,"EXIT")||equal(cmd,"SALIR"))break;
        command(cmd);
    }
    data_root->Close(data_root);
    return 0;
}
// UEFI applications use the firmware's file system and must retain Boot Services.
// A subsequent version can boot a native 64-bit kernel via ExitBootServices.
#if defined(__clang__)
#define RODOS_OPTNONE __attribute__((optnone))
#else
#define RODOS_OPTNONE __attribute__((optimize("O0")))
#endif
extern "C" RODOS_OPTNONE void* memset(void* out,int value,size_t bytes) {
    volatile unsigned char* p=static_cast<volatile unsigned char*>(out);
    for(size_t i=0;i<bytes;++i)p[i]=static_cast<unsigned char>(value);
    return out;
}
// Keep one base relocation: some UEFI PE loaders reject fixed-address images.
extern "C" {
__attribute__((used)) void* rodos_relocation_anchor = (void*)&efi_main;
}
