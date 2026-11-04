#include "block.hpp"
extern "C" int bios_sector(u32 lba, u32 write);
namespace block {
bool read(u32 lba, u8* out) {
    if (!out) return false;
    if (!bios_sector(lba, 0)) return false;
    const volatile u8* bounce = reinterpret_cast<const volatile u8*>(0x5200);
    for (u32 i=0; i<512; ++i) out[i]=bounce[i];
    return true;
}
bool write(u32 lba, const u8* in) {
    if (!in) return false;
    volatile u8* bounce = reinterpret_cast<volatile u8*>(0x5200);
    for (u32 i=0; i<512; ++i) bounce[i]=in[i];
    return bios_sector(lba, 1)!=0;
}
}
