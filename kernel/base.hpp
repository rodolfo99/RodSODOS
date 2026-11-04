#pragma once
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using usize = __SIZE_TYPE__;
static_assert(sizeof(u32) == 4, "32-bit unsigned required");

namespace str {
inline usize len(const char* s) { usize n = 0; while (s[n]) ++n; return n; }
inline char upper(char c) { return c >= 'a' && c <= 'z' ? char(c - 32) : c; }
inline bool eq(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
inline bool ieq(const char* a, const char* b) {
    while (*a && upper(*a) == upper(*b)) { ++a; ++b; } return *a == *b;
}
inline void copy(char* dst, const char* src, usize capacity) {
    if (!capacity) return;
    usize n = 0; while (src[n] && n + 1 < capacity) { dst[n] = src[n]; ++n; }
    dst[n] = 0;
}
inline char* trim(char* s) {
    while (*s == ' ' || *s == '\t') ++s;
    usize n = len(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r')) s[--n] = 0;
    return s;
}
inline bool separator(char c) { return c == '/' || c == '\\'; }
}
