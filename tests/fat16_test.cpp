#include "../kernel/fat16.hpp"
#include "../kernel/block.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
namespace {
std::vector<u8> storage;
u32 readCount=0,writeCount=0;
}
namespace block {
bool read(u32 lba,u8* dst) {
    if (u32(lba)>storage.size()/512-1) return false;
    std::memcpy(dst,storage.data()+u32(lba)*512,512);++readCount;return true;
}
bool write(u32 lba,const u8* src) {
    if (lba<20480 || lba>=20480u+32768u) return false; // only the data partition
    std::memcpy(storage.data()+u32(lba)*512,src,512);++writeCount;return true;
}
}
int main(int argc,char** argv) {
    if (argc!=2) return 2;
    std::ifstream in(argv[1],std::ios::binary);
    assert(in.good());
    storage.assign(std::istreambuf_iterator<char>(in),{});
    assert(storage.size()==(20480u+32768u)*512u);
    const std::vector<u8> immutable(storage.begin(),storage.begin()+20480*512);
    auto volume = new fat::Volume;
    assert(volume->init() && volume->mounted() && volume->partitionStart()==20480u);
    assert(volume->clusterCount()>4000 && volume->rootEntries()==512);
    char text[fs::FileCapacity];u32 size=0;
    assert(volume->read("LEEME.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(std::strstr(text,"RodOS")!=nullptr);
    assert(volume->write("NOTA.TXT","Hola USB",8)==fs::Error::Ok);
    assert(volume->read("nota.txt",text,sizeof(text),size)==fs::Error::Ok);
    assert(size==8 && std::strcmp(text,"Hola USB")==0);
    assert(volume->write("NOTA.TXT","+sol",4,true)==fs::Error::Ok);
    assert(volume->read("NOTA.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(size==12 && std::strcmp(text,"Hola USB+sol")==0);
    assert(volume->write("NOTA.TXT","",0)==fs::Error::Ok);
    assert(volume->read("NOTA.TXT",text,sizeof(text),size)==fs::Error::Ok && size==0);
    char large[fs::FileCapacity];
    for (int i=0;i<4095;++i) large[i]=char('A'+i%26);
    assert(volume->write("MAX.TXT",large,4095)==fs::Error::Ok);
    assert(volume->read("MAX.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(size==4095 && std::memcmp(text,large,4095)==0);
    assert(volume->write("MAX.TXT","x",1,true)==fs::Error::TooLarge);
    assert(volume->write("INVALIDNAME.TXT","abc",3)==fs::Error::InvalidName);
    assert(volume->rename("MAX.TXT","NUEVA.TXT")==fs::Error::Ok);
    assert(volume->read("MAX.TXT",text,sizeof(text),size)==fs::Error::NotFound);
    assert(volume->read("NUEVA.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(volume->remove("NUEVA.TXT")==fs::Error::Ok);
    assert(volume->read("NUEVA.TXT",text,sizeof(text),size)==fs::Error::NotFound);
    assert(volume->remove("NOTA.TXT")==fs::Error::Ok);
    delete volume;
    volume = new fat::Volume;
    assert(volume->init());
    assert(volume->read("LEEME.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(volume->write("PERSIST.TXT","Persiste tras reinicio",22)==fs::Error::Ok);
    delete volume;
    volume = new fat::Volume;
    assert(volume->init());
    assert(volume->read("PERSIST.TXT",text,sizeof(text),size)==fs::Error::Ok);
    assert(size==22 && std::strcmp(text,"Persiste tras reinicio")==0);
    assert(std::equal(immutable.begin(),immutable.end(),storage.begin()));
    std::printf("FAT16: pruebas OK; accesos: %u lecturas, %u escrituras; ISO inmutable.\n",readCount,writeCount);
    delete volume;
    return 0;
}
