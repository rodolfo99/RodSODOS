#include "fat16.hpp"
#include "block.hpp"
namespace fat {
bool Volume::shortName(const char* input, u8 out[11]) {
    if (!input || !fs::RamFS::validName(input)) return false;
    for (int i=0;i<11;++i) out[i]=' ';
    int i=0;
    while (*input) {
        char c=str::upper(*input++);
        if (c=='.') {i=8;continue;}
        out[i++]=u8(c);
    }
    return true;
}
bool Volume::init() {
    mounted_=false;cachedLba_=0xffffffffu;
    u8 mbr[512],first[512];
    if (!block::read(0,mbr) || mbr[510]!=0x55 || mbr[511]!=0xaa) return false;
    for (int index=0;index<4;++index) {
        const u8* p=mbr+446+16*index;
        if (p[4]!=0x04 && p[4]!=0x06 && p[4]!=0x0e) continue;
        u32 start=rd32(p+8),size=rd32(p+12);
        if (start<128 || size<8192 || start>0xffffffffu-size) continue;
        if (!block::read(start,first)) return false;
        if (first[510]!=0x55 || first[511]!=0xaa) continue;
        u16 bps=rd16(first+11),reserved=rd16(first+14);
        u8 spc=first[13],copies=first[16];
        u16 entries=rd16(first+17),spf=rd16(first+22);
        u32 sectors=rd16(first+19) ? rd16(first+19) : rd32(first+32);
        if (bps!=512 || !spc || (spc&(spc-1)) || spc>32 || reserved<1 || copies!=2 ||
            !entries || entries%16 || !spf || spf>128 || sectors>size || sectors<8192) continue;
        u32 rootSectors=(u32(entries)*32+511)/512;
        u32 overhead=u32(reserved)+u32(spf)*copies+rootSectors;
        if (overhead>=sectors) continue;
        u32 clusters=(sectors-overhead)/spc;
        if (clusters<4085 || clusters>65524 || (clusters+2)*2>u32(spf)*512) continue;
        partStart_=start; partSectors_=size; sectorsPerCluster_=spc;
        sectorsPerFat_=spf; rootEntries_=entries; clusters_=clusters;
        fatStart_=start+reserved;
        rootStart_=fatStart_+2*spf;
        dataStart_=rootStart_+rootSectors;
        mounted_=true;
        return true;
    }
    return false;
}
bool Volume::root(u16 index,u8 dest[32]) {
    if (!mounted_ || index>=rootEntries_) return false;
    u32 lba=rootStart_+u32(index)/16;
    if (cachedLba_!=lba) {
        if (!block::read(lba,cache_)) return false;
        cachedLba_=lba;
    }
    for (int j=0;j<32;++j) dest[j]=cache_[(index%16)*32+j];
    return true;
}
bool Volume::locate(const u8 name[11],u16& index,u8 data[32]) {
    for (u16 i=0;i<rootEntries_;++i) {
        if (!root(i,data)) return false;
        if (data[0]==0) return false;
        if (data[0]==0xe5 || data[11]==0x0f || (data[11]&0x08)) continue;
        bool eq=true;for (int j=0;j<11;++j) if (data[j]!=name[j]) eq=false;
        if (eq) {index=i;return true;}
    }
    return false;
}
bool Volume::freeEntry(u16& index) {
    u8 data[32];
    for (u16 i=0;i<rootEntries_;++i) {
        if (!root(i,data)) return false;
        if (data[0]==0 || data[0]==0xe5) {index=i;return true;}
    }
    return false;
}
bool Volume::update(u16 index,const u8 data[32]) {
    if (!mounted_ || index>=rootEntries_) return false;
    u32 lba=rootStart_+u32(index)/16;
    if (!block::read(lba,sector_)) return false;
    for (int j=0;j<32;++j) sector_[(index%16)*32+j]=data[j];
    if (!block::write(lba,sector_)) {cachedLba_=0xffffffffu;return false;}
    for (int j=0;j<512;++j) cache_[j]=sector_[j];
    cachedLba_=lba;
    return true;
}
bool Volume::entry(u16 index,FileInfo& out) {
    out.used=false;out.directory=false;out.size=0;out.name[0]=0;
    u8 data[32];if (!root(index,data)) return false;
    if (data[0]==0 || data[0]==0xe5 || data[11]==0x0f || (data[11]&0x08)) return true;
    out.used=true;out.directory=(data[11]&0x10)!=0;out.size=rd32(data+28);
    int n=0;for (int j=0;j<8 && data[j]!=' ';++j) out.name[n++]=char(data[j]);
    if (data[8]!=' ') {
        out.name[n++]='.';
        for (int j=8;j<11 && data[j]!=' ';++j) out.name[n++]=char(data[j]);
    }
    out.name[n]=0;return true;
}
bool Volume::loadFat() {
    for (u32 i=0;i<128;++i) dirtyFat_[i]=false;
    for (u32 i=0;i<sectorsPerFat_;++i)
        if (!block::read(fatStart_+i,fat_+512*i)) return false;
    return true;
}
bool Volume::flushFat() {
    for (u32 copy=0;copy<2;++copy)
        for (u32 i=0;i<sectorsPerFat_;++i)
            if (dirtyFat_[i] && !block::write(fatStart_+copy*sectorsPerFat_+i,fat_+512*i)) return false;
    for (u32 i=0;i<sectorsPerFat_;++i) dirtyFat_[i]=false;
    return true;
}
bool Volume::freeChain(u16 first) {
    for (u32 i=0;validCluster(first) && i<clusters_;++i) {
        u16 after=next(first);setNext(first,0);first=after;
    }
    return !validCluster(first);
}
fs::Error Volume::read(const char* name,char* out,u32 capacity,u32& bytes) {
    bytes=0;
    if (!mounted_) return fs::Error::IoError;
    u8 shortN[11];if (!shortName(name,shortN)) return fs::Error::InvalidName;
    u16 index=0;u8 e[32];if (!locate(shortN,index,e)) return fs::Error::NotFound;
    if (e[11]&0x10) return fs::Error::IsDirectory;
    u32 size=rd32(e+28);
    if (!capacity || size>=capacity) return fs::Error::TooLarge;
    if (!loadFat()) return fs::Error::IoError;
    u16 cluster=rd16(e+26);
    u32 remain=size;
    while (remain) {
        if (!validCluster(cluster)) return fs::Error::IoError;
        for (u32 s=0;s<sectorsPerCluster_ && remain;++s) {
            u8 buffer[512];
            if (!block::read(clusterLba(cluster)+s,buffer)) return fs::Error::IoError;
            u32 n=remain<512?remain:512;
            for (u32 k=0;k<n;++k) out[bytes+k]=char(buffer[k]);
            bytes+=n;remain-=n;
        }
        if (remain) cluster=next(cluster);
    }
    out[bytes]=0;return fs::Error::Ok;
}
fs::Error Volume::write(const char* name,const char* text,u32 size,bool append) {
    if (!mounted_) return fs::Error::IoError;
    u8 shortN[11];if (!shortName(name,shortN)) return fs::Error::InvalidName;
    if (size>=fs::FileCapacity) return fs::Error::TooLarge;
    u16 index=0;u8 e[32]={};bool exists=locate(shortN,index,e);
    if (exists && (e[11]&0x10)) return fs::Error::IsDirectory;
    if (exists && (e[11]&1)) return fs::Error::Protected;
    char merge[fs::FileCapacity];
    if (append && exists) {
        u32 prior=0;fs::Error er=read(name,merge,sizeof(merge),prior);
        if (er!=fs::Error::Ok) return er;
        if (prior+size>=sizeof(merge)) return fs::Error::TooLarge;
        for (u32 j=0;j<size;++j) merge[prior+j]=text[j];
        size+=prior;text=merge;
    }
    if (!exists && !freeEntry(index)) return fs::Error::Full;
    if (!loadFat()) return fs::Error::IoError;
    u16 old=exists?rd16(e+26):0;
    u32 bytesPerCluster=512u*sectorsPerCluster_;
    u32 needed=(size+bytesPerCluster-1)/bytesPerCluster;
    u16 fresh[8];if (needed>8) return fs::Error::TooLarge;
    u32 count=0;
    for (u32 c=2;c<clusters_+2 && count<needed;++c)
        if (next(u16(c))==0) fresh[count++]=u16(c);
    if (count!=needed) return fs::Error::Full;
    /* Write into unallocated space: old file stays readable on data error. */
    u32 position=0;
    for (u32 k=0;k<needed;++k) {
        for (u32 s=0;s<sectorsPerCluster_;++s) {
            u8 buffer[512]={};
            for (u32 j=0;j<512 && position<size;++j) buffer[j]=u8(text[position++]);
            if (!block::write(clusterLba(fresh[k])+s,buffer)) return fs::Error::IoError;
        }
    }
    for (u32 k=0;k<needed;++k) setNext(fresh[k],k+1<needed?fresh[k+1]:0xffff);
    if (!flushFat()) return fs::Error::IoError;
    if (!exists) {
        for (int i=0;i<32;++i) e[i]=0;
        for (int i=0;i<11;++i) e[i]=shortN[i];
        e[11]=0x20;
    }
    wr16(e+26,needed?fresh[0]:0);
    wr32(e+28,size);
    if (!update(index,e)) return fs::Error::IoError;
    /* Directory now points to new data; free old chain only afterward. */
    if (old && !freeChain(old)) return fs::Error::IoError;
    if (old && !flushFat()) return fs::Error::IoError;
    return fs::Error::Ok;
}
fs::Error Volume::remove(const char* name) {
    if (!mounted_) return fs::Error::IoError;
    u8 shortN[11];if (!shortName(name,shortN)) return fs::Error::InvalidName;
    u16 index=0;u8 e[32];if (!locate(shortN,index,e)) return fs::Error::NotFound;
    if (e[11]&0x10) return fs::Error::IsDirectory;
    if (e[11]&1) return fs::Error::Protected;
    u16 old=rd16(e+26);e[0]=0xe5;
    if (!update(index,e)) return fs::Error::IoError;
    if (old && (!loadFat() || !freeChain(old) || !flushFat())) return fs::Error::IoError;
    return fs::Error::Ok;
}
fs::Error Volume::rename(const char* name,const char* newName) {
    if (!mounted_) return fs::Error::IoError;
    u8 a[11],b[11];if (!shortName(name,a)||!shortName(newName,b)) return fs::Error::InvalidName;
    u16 index=0,other=0;u8 e[32],tmp[32];
    if (!locate(a,index,e)) return fs::Error::NotFound;
    if (e[11]&0x10) return fs::Error::IsDirectory;
    if (locate(b,other,tmp) && other!=index) return fs::Error::Exists;
    for (int j=0;j<11;++j) e[j]=b[j];
    return update(index,e)?fs::Error::Ok:fs::Error::IoError;
}
}
