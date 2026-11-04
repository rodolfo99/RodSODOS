#include "hardware.hpp"
namespace {
volatile u16* const vga=reinterpret_cast<volatile u16*>(0xb8000);
int row=1, col=0;
u8 attribute=7;
char history[8][256]; int historyCount, historyNext;
void cursor() {
    u16 p=u16(row*80+col);
    hw::out(0x3d4,14); hw::out(0x3d5,u8(p>>8)); hw::out(0x3d4,15); hw::out(0x3d5,u8(p));
}
void scroll() {
    if (row<24) return;
    for (int i=80; i<23*80; ++i) vga[i]=vga[i+80];
    for (int i=23*80; i<24*80; ++i) vga[i]=u16(attribute<<8|' ');
    row=23;
}
void fixedRow(int r, const char* text, u8 color) {
    int c=0; while (text[c] && c<80) { vga[r*80+c]=u16(color<<8|u8(text[c])); ++c; }
    while (c<80) vga[r*80+c++]=u16(color<<8|' ');
}
void eraseLine(usize& n) { while (n) { console::put('\b'); --n; } }
}
namespace console {
void init() { clear(); }
void clear() {
    for (int i=80; i<24*80; ++i) vga[i]=u16(attribute<<8|' ');
    fixedRow(0," RodOS 0.1.2  |  C++ / x86  |  CONSOLA DE COMANDOS",0x1f);
    status(" HELP: ayuda   |   Flechas: historial   |   C: RAM temporal  |  D: USB FAT16 persistente");
    row=1; col=0; cursor();
    const char* clearSequence="\033[2J\033[H";
    for (; *clearSequence; ++clearSequence) hw::serial(*clearSequence);
}
void status(const char* s) { fixedRow(24,s,0x70); }
void color(u8 c) { attribute=c; }
void put(char c) {
    if (c=='\n') { col=0; ++row; hw::serial('\r'); hw::serial('\n'); }
    else if (c=='\r') { col=0; hw::serial(c); }
    else if (c=='\b') {
        if (col) --col;
        else if (row>1) { --row; col=79; }
        vga[row*80+col]=u16(attribute<<8|' ');
        hw::serial('\b'); hw::serial(' '); hw::serial('\b');
    } else if (c=='\t') { do { put(' '); } while (col%4); return; }
    else if (u8(c)>=32 && u8(c)<=126) {
        vga[row*80+col]=u16(attribute<<8|u8(c)); hw::serial(c);
        if (++col>=80) { col=0; ++row; }
    }
    scroll(); cursor();
}
void print(const char* s) { while (*s) put(*s++); }
void line(const char* s) { print(s); put('\n'); }
void number(u32 n) {
    char buffer[11]; int count=0;
    do { buffer[count++]=char('0'+n%10); n/=10; } while (n);
    while (count) put(buffer[--count]);
}
void banner() {
    line(); color(0x0b); line("  RodOS 0.1.2"); color(7);
    line("  Sistema operativo educativo en C++ | PC x86 de 32 bits");
    line("  Arranque propio. Consola inspirada en MS-DOS.");
    line();
    line("  C: RAM temporal | D: FAT16 persistente en USB (BIOS/CSM).");
    line("  Escriba HELP para comenzar o DEMO para ver un archivo BAT.");
    line();
}
bool readLine(char* out, usize capacity, bool useHistory) {
    static bool ignoreLF;
    usize n=0; int historyOffset=0; char draft[256]; draft[0]=0;
    if (!capacity) return false;
    out[0]=0;
    for (;;) {
        int c=hw::key();
        if (!c) { hw::wait(); continue; }
        if (c=='\n' && ignoreLF) { ignoreLF=false; continue; }
        ignoreLF=false;
        if (c=='\r'||c=='\n') {
            ignoreLF=c=='\r'; out[n]=0; put('\n');
            if (useHistory && n) {
                str::copy(history[historyNext],out,sizeof(history[0]));
                historyNext=(historyNext+1)%8; if (historyCount<8) ++historyCount;
            }
            return true;
        }
        if (c==3) { line("^C"); out[0]=0; return false; }
        if (c==12) { clear(); print(out); continue; }
        if (c==8) { if (n) { put('\b'); out[--n]=0; } continue; }
        if (useHistory && (c==hw::Up||c==hw::Down)) {
            if (!historyOffset) str::copy(draft,out,sizeof(draft));
            if (c==hw::Up && historyOffset<historyCount) ++historyOffset;
            if (c==hw::Down && historyOffset) --historyOffset;
            eraseLine(n);
            const char* replacement=historyOffset ? history[(historyNext-historyOffset+8)%8] : draft;
            str::copy(out,replacement,capacity); n=str::len(out); print(out); continue;
        }
        if (c>=32 && c<=126 && n+1<capacity) { out[n++]=char(c); out[n]=0; put(char(c)); }
    }
}
}
