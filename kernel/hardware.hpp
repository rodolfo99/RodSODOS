#pragma once
#include "base.hpp"
namespace hw {
inline void out(u16 port, u8 value) { asm volatile("outb %0, %1" : : "a"(value), "Nd"(port)); }
inline u8 in(u16 port) { u8 value; asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
inline void out16(u16 port, u16 value) { asm volatile("outw %0, %1" : : "a"(value), "Nd"(port)); }
void init();
void serial(char c);
int key();
u32 ticks();
void wait();
[[noreturn]] void shutdown();
[[noreturn]] void reboot();
bool clock(u8& hour, u8& minute, u8& second, u8& day, u8& month, u16& year);
constexpr int Up = 256, Down = 257;
}
namespace console {
void init();
void clear();
void put(char c);
void print(const char* s);
void line(const char* s = "");
void number(u32 n);
void color(u8 c);
void banner();
void status(const char* s);
bool readLine(char* out, usize capacity, bool history = true);
}
