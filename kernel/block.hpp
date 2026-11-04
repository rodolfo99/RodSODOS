#pragma once
#include "base.hpp"
namespace block {
// Synchronous one-sector access to the BIOS boot disk (INT 13h EDD).
// Caller must not be an interrupt handler. Writes only used for FAT partition.
bool read(u32 lba, u8* out);
bool write(u32 lba, const u8* in);
}
