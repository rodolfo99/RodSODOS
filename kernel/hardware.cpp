#include "hardware.hpp"

extern "C" void* memset(void* dst, int c, usize n) {
    auto* d = static_cast<u8*>(dst); for (usize i=0; i<n; ++i) d[i]=u8(c); return dst;
}
extern "C" void* memcpy(void* dst, const void* src, usize n) {
    auto* d = static_cast<u8*>(dst); const auto* s=static_cast<const u8*>(src);
    for (usize i=0; i<n; ++i) d[i]=s[i];
    return dst;
}
extern "C" void* memmove(void* dst, const void* src, usize n) {
    auto* d = static_cast<u8*>(dst); const auto* s=static_cast<const u8*>(src);
    if (reinterpret_cast<usize>(d) < reinterpret_cast<usize>(s)) {
        for (usize i=0; i<n; ++i) d[i]=s[i];
    } else { while (n) { --n; d[n]=s[n]; } }
    return dst;
}
extern "C" int memcmp(const void* a, const void* b, usize n) {
    const auto* x=static_cast<const u8*>(a); const auto* y=static_cast<const u8*>(b);
    for (usize i=0; i<n; ++i) if (x[i]!=y[i]) return int(x[i])-int(y[i]);
    return 0;
}
extern "C" void timer_stub();
extern "C" void fault_stub();
namespace {
struct __attribute__((packed)) Gate { u16 low, selector; u8 zero, flags; u16 high; };
struct __attribute__((packed)) IDTR { u16 size; u32 address; };
Gate idt[256];
volatile u32 clockTicks;
bool serialAvailable;
void gate(int i, void (*fn)()) {
    u32 a=reinterpret_cast<u32>(fn);
    idt[i]={u16(a), 8, 0, 0x8e, u16(a>>16)};
}
void delayIO() { hw::out(0x80, 0); }
u8 cmos(u8 index) { hw::out(0x70, index); delayIO(); return hw::in(0x71); }
u8 bcd(u8 n) { return u8((n>>4)*10 + (n&15)); }
}
extern "C" void timer_interrupt() { clockTicks = clockTicks+1; hw::out(0x20, 0x20); }
extern "C" [[noreturn]] void fatal_interrupt() {
    console::color(0x4f);
    console::line("\nERROR DEL KERNEL: excepcion de CPU o interrupcion inesperada.");
    console::line("Sistema detenido. Reinicie la maquina virtual.");
    for (;;) asm volatile("cli; hlt");
}
namespace hw {
void init() {
    out(0x3f8+1, 0); out(0x3f8+3, 0x80);
    out(0x3f8, 1); out(0x3f8+1, 0); out(0x3f8+3, 3);
    out(0x3f8+2, 0xc7); out(0x3f8+4, 0x0b);
    serialAvailable = in(0x3f8+5) != 0xff;
    for (int i=0; i<256; ++i) gate(i, fault_stub);
    gate(32, timer_stub);
    IDTR descriptor={u16(sizeof(idt)-1), reinterpret_cast<u32>(idt)};
    asm volatile("lidt %0" : : "m"(descriptor) : "memory");
    out(0x20,0x11); delayIO(); out(0xa0,0x11); delayIO();
    out(0x21,0x20); delayIO(); out(0xa1,0x28); delayIO();
    out(0x21,4); delayIO(); out(0xa1,2); delayIO();
    out(0x21,1); delayIO(); out(0xa1,1); delayIO();
    out(0x21,0xfe); out(0xa1,0xff); // IRQ0 only; PS/2 and serial are polled.
    constexpr u16 divisor=11932; // Approximately 100 Hz.
    out(0x43,0x36); out(0x40,u8(divisor)); out(0x40,u8(divisor>>8));
    asm volatile("sti" : : : "memory");
}
void serial(char c) {
    if (!serialAvailable) return;
    for (u32 i=0; i<100000; ++i) {
        if (in(0x3f8+5)&0x20) { out(0x3f8,u8(c)); return; }
        asm volatile("pause");
    }
}
u32 ticks() { return clockTicks; }
void wait() { asm volatile("hlt" : : : "memory"); }
int key() {
    static bool leftShift, rightShift, caps, control, extended;
    static int ansi;
    if (serialAvailable && (in(0x3f8+5)&1)) {
        u8 c=in(0x3f8);
        if (c==27) { ansi=1; return 0; }
        if (ansi==1) { ansi=c=='[' ? 2 : 0; return 0; }
        if (ansi==2) { ansi=0; return c=='A' ? Up : c=='B' ? Down : 0; }
        return c==127 ? 8 : c;
    }
    u8 status=in(0x64);
    if (!(status&1)) return 0;
    u8 code=in(0x60);
    if (status&0x20) return 0; // Ignore mouse data.
    if (code==0xe0) { extended=true; return 0; }
    bool release=(code&0x80)!=0; code &= 0x7f;
    if (extended) {
        extended=false;
        if (code==29) { control=!release; return 0; }
        if (release) return 0;
        if (code==72) return Up;
        if (code==80) return Down;
        if (code==28) return '\r';
        return 0;
    }
    if (code==42) { leftShift=!release; return 0; }
    if (code==54) { rightShift=!release; return 0; }
    if (code==29) { control=!release; return 0; }
    if (release) return 0;
    if (code==58) { caps=!caps; return 0; }
    static const char normal[] =
        "\0\0331234567890-=\b\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
    static const char shifted[] =
        "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
    if (code >= sizeof(normal)-1) return 0;
    bool shift=leftShift||rightShift;
    char c=shift ? shifted[code] : normal[code];
    if (caps && ((c>='a'&&c<='z') || (c>='A'&&c<='Z'))) c^=32;
    if (control && str::upper(c)=='C') return 3;
    if (control && str::upper(c)=='L') return 12;
    return c;
}
bool clock(u8& hour, u8& minute, u8& second, u8& day, u8& month, u16& year) {
    const u8 registers[]={0,2,4,7,8,9}; u8 a[6],b[6]; bool stable=false;
    for (int attempt=0; attempt<5; ++attempt) {
        u32 timeout=100000; while ((cmos(0x0a)&0x80) && --timeout) asm volatile("pause");
        if (!timeout) return false;
        for (int i=0; i<6; ++i) a[i]=cmos(registers[i]);
        if (cmos(0x0a)&0x80) continue;
        for (int i=0; i<6; ++i) b[i]=cmos(registers[i]);
        if (cmos(0x0a)&0x80) continue;
        stable=true; for (int i=0; i<6; ++i) if (a[i]!=b[i]) stable=false;
        if (stable) break;
    }
    if (!stable) return false;
    u8 mode=cmos(0x0b); bool pm=(a[2]&0x80)!=0; a[2]&=0x7f;
    if (!(mode&4)) for (auto& n:a) n=bcd(n);
    if (!(mode&2)) a[2]=u8(a[2]%12+(pm ? 12 : 0));
    second=a[0]; minute=a[1]; hour=a[2]; day=a[3]; month=a[4]; year=u16(2000+a[5]);
    return true;
}
[[noreturn]] void shutdown() {
    console::line("Apagando RodOS. C: temporal; D: archivos persistentes.");
    out16(0x604,0x2000); // QEMU pc/PIIX4 PM default.
    console::line("Sistema detenido. Puede cerrar la ventana.");
    for (;;) asm volatile("cli; hlt");
}
[[noreturn]] void reboot() {
    console::line("Reiniciando RodOS. C: temporal; D: archivos persistentes.");
    asm volatile("cli");
    for (u32 i=0; i<100000; ++i) if (!(in(0x64)&2)) { out(0x64,0xfe); break; }
    // Triple fault if the 8042 reset is unavailable.
    IDTR empty={0,0}; asm volatile("lidt %0; int $3" : : "m"(empty));
    for (;;) asm volatile("hlt");
}
}
