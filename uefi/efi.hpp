#pragma once
// Minimal, dependency-free subset of UEFI 2.x protocols for RodOS.
// x86_64 UEFI uses the Microsoft x64 calling convention.
#include <stdint.h>
#include <stddef.h>
#define EFIAPI __attribute__((ms_abi))
using UINTN = uintptr_t;
using EFI_STATUS = uintptr_t;
using EFI_HANDLE = void*;
using EFI_EVENT = void*;
using CHAR16 = char16_t;
struct EFI_GUID { uint32_t a; uint16_t b,c; uint8_t d[8]; };
struct EFI_TABLE_HEADER { uint64_t signature; uint32_t revision,header_size,crc32,reserved; };
struct EFI_INPUT_KEY { uint16_t scan_code,unicode_char; };
struct EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL*, bool);
    EFI_STATUS (EFIAPI *ReadKeyStroke)(EFI_SIMPLE_TEXT_INPUT_PROTOCOL*, EFI_INPUT_KEY*);
    EFI_EVENT WaitForKey;
};
struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *Reset)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*,bool);
    EFI_STATUS (EFIAPI *OutputString)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*,const CHAR16*);
    void *TestString,*QueryMode,*SetMode,*SetAttribute,*ClearScreen,*SetCursorPosition,*EnableCursor,*Mode;
};
struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER header;
    void* entry[44]; // See UEFI Boot Services table, RaiseTPL through CreateEventEx.
};
struct EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER header;
    void* entry[14]; // GetTime through QueryVariableInfo.
};
struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER header;
    CHAR16* firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _padding;
    EFI_HANDLE console_in_handle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* con_in;
    EFI_HANDLE console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* con_out;
    EFI_HANDLE standard_error_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* std_err;
    EFI_RUNTIME_SERVICES* runtime_services;
    EFI_BOOT_SERVICES* boot_services;
    UINTN number_of_table_entries;
    void* configuration_table;
};
struct EFI_FILE_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (EFIAPI *Open)(EFI_FILE_PROTOCOL*,EFI_FILE_PROTOCOL**,const CHAR16*,uint64_t,uint64_t);
    EFI_STATUS (EFIAPI *Close)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Delete)(EFI_FILE_PROTOCOL*);
    EFI_STATUS (EFIAPI *Read)(EFI_FILE_PROTOCOL*,UINTN*,void*);
    EFI_STATUS (EFIAPI *Write)(EFI_FILE_PROTOCOL*,UINTN*,const void*);
    EFI_STATUS (EFIAPI *GetPosition)(EFI_FILE_PROTOCOL*,uint64_t*);
    EFI_STATUS (EFIAPI *SetPosition)(EFI_FILE_PROTOCOL*,uint64_t);
    EFI_STATUS (EFIAPI *GetInfo)(EFI_FILE_PROTOCOL*,const EFI_GUID*,UINTN*,void*);
    EFI_STATUS (EFIAPI *SetInfo)(EFI_FILE_PROTOCOL*,const EFI_GUID*,UINTN,void*);
    EFI_STATUS (EFIAPI *Flush)(EFI_FILE_PROTOCOL*);
};
struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (EFIAPI *OpenVolume)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*,EFI_FILE_PROTOCOL**);
};
struct EFI_TIME {
    uint16_t year;
    uint8_t month,day,hour,minute,second,pad1;
    uint32_t nanosecond;
    int16_t timezone;
    uint8_t daylight,pad2;
};
struct EFI_FILE_INFO {
    uint64_t size,file_size,physical_size;
    EFI_TIME create_time,last_access_time,modification_time;
    uint64_t attribute;
    CHAR16 file_name[1];
};
static_assert(offsetof(EFI_FILE_INFO,file_name)==80, "EFI_FILE_INFO layout mismatch");
static_assert(offsetof(EFI_SYSTEM_TABLE,boot_services)==96, "EFI system table layout mismatch");
static_assert(sizeof(EFI_TABLE_HEADER)==24, "EFI table header layout mismatch");
inline bool ok(EFI_STATUS result) {return result==0;}
constexpr uint64_t EFI_FILE_MODE_READ=1, EFI_FILE_MODE_WRITE=2;
constexpr uint64_t EFI_FILE_MODE_CREATE=0x8000000000000000ULL;
constexpr uint64_t EFI_FILE_DIRECTORY=0x10, EFI_FILE_ARCHIVE=0x20;
constexpr EFI_GUID SIMPLE_FS_GUID={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
constexpr EFI_GUID FILE_INFO_GUID={0x09576e92,0x6d3f,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
