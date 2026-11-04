#pragma once
#include "fs.hpp"
namespace fat {
struct FileInfo {
    bool used;
    bool directory;
    char name[13];
    u32 size;
};
class Volume {
public:
    bool init();
    bool mounted() const { return mounted_; }
    u32 partitionStart() const { return partStart_; }
    u32 partitionSectors() const { return partSectors_; }
    u32 clusterCount() const { return clusters_; }
    u16 rootEntries() const { return rootEntries_; }
    bool entry(u16 index, FileInfo& out);
    fs::Error read(const char* name, char* out, u32 capacity, u32& bytes);
    fs::Error write(const char* name, const char* text, u32 size, bool append=false);
    fs::Error remove(const char* name);
    fs::Error rename(const char* name, const char* newName);
private:
    bool mounted_ = false;
    u32 partStart_=0, partSectors_=0, fatStart_=0, rootStart_=0;
    u32 dataStart_=0, clusters_=0;
    u16 rootEntries_=0, sectorsPerFat_=0;
    u8 sectorsPerCluster_=0;
    u8 fat_[65536] = {};
    bool dirtyFat_[128] = {};
    u8 sector_[512] = {};
    u8 cache_[512] = {};
    u32 cachedLba_=0xffffffffu;
    static u16 rd16(const u8* p) { return u16(p[0]|(u16(p[1])<<8)); }
    static u32 rd32(const u8* p) { return u32(p[0])|(u32(p[1])<<8)|(u32(p[2])<<16)|(u32(p[3])<<24); }
    static void wr16(u8* p,u16 v) { p[0]=u8(v);p[1]=u8(v>>8); }
    static void wr32(u8* p,u32 v) { wr16(p,u16(v));wr16(p+2,u16(v>>16)); }
    static bool shortName(const char* name, u8 out[11]);
    bool validCluster(u16 c) const { return c>=2 && u32(c)<clusters_+2; }
    u32 clusterLba(u16 c) const { return dataStart_+(u32(c)-2)*sectorsPerCluster_; }
    bool root(u16 index, u8 dest[32]);
    bool locate(const u8 name[11],u16& index,u8 data[32]);
    bool freeEntry(u16& index);
    bool update(u16 index,const u8 data[32]);
    bool loadFat();
    bool flushFat();
    u16 next(u16 c) const { return rd16(fat_+2*c); }
    void setNext(u16 c,u16 n) { wr16(fat_+2*c,n);dirtyFat_[u32(c)/256]=true; }
    bool freeChain(u16 first);
};
}
