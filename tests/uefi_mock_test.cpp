// Host test of the exact RodOS UEFI shell with fake EFI BootServices and FileProtocol.
// Tests command-level persistence across two EFI application launches. NO real firmware.
#include <cassert>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../uefi/efi.hpp"

extern "C" EFI_STATUS EFIAPI efi_main(EFI_HANDLE,EFI_SYSTEM_TABLE*);
namespace {
std::map<std::string,std::string> files={
    {"RODOS.DAT","RODOS-DATA-V1\r\nDo not delete this marker.\r\n"},
    {"LEEME.TXT","Welcome to RodOS 0.1.3\r\n"}
};
std::string keystrokes,output;
size_t cursor=0;int flush_count=0;
struct FakeFile: EFI_FILE_PROTOCOL {
    std::string name;size_t position=0;
    bool root=false;
};
struct FakeFS: EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {bool marked=false;};
FakeFS fs_bad,fs_data;
EFI_SIMPLE_TEXT_INPUT_PROTOCOL in={};
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL out={};
EFI_BOOT_SERVICES bs={};
EFI_RUNTIME_SERVICES rt={};
EFI_SYSTEM_TABLE st={};
std::string narrow(const CHAR16* s) {
    std::string r;while(*s) {assert(*s<=127);r+=static_cast<char>(*s++);}return r;
}
FakeFile* newfile(bool root);
EFI_STATUS EFIAPI ff_open(EFI_FILE_PROTOCOL* root, EFI_FILE_PROTOCOL** dst,const CHAR16* s,uint64_t mode,uint64_t) {
    auto* f=static_cast<FakeFile*>(root);
    assert(f->root);
    std::string name=narrow(s);
    if(mode&EFI_FILE_MODE_CREATE) {if(!files.count(name))files[name]="";}
    if(!files.count(name))return 1;
    auto* n=newfile(false);n->name=name;
    *dst=n;return 0;
}
EFI_STATUS EFIAPI ff_close(EFI_FILE_PROTOCOL* f) {delete static_cast<FakeFile*>(f);return 0;}
EFI_STATUS EFIAPI ff_delete(EFI_FILE_PROTOCOL* f) {
    auto* p=static_cast<FakeFile*>(f);
    files.erase(p->name);delete p;return 0;
}
EFI_STATUS EFIAPI ff_read(EFI_FILE_PROTOCOL* f,UINTN* sz,void* outp) {
    auto* p=static_cast<FakeFile*>(f);
    if(p->root){*sz=0;return 0;}
    const std::string& s=files[p->name];
    size_t n=std::min(static_cast<size_t>(*sz),s.size()-p->position);
    std::memcpy(outp,s.data()+p->position,n);p->position+=n;*sz=n;return 0;
}
EFI_STATUS EFIAPI ff_write(EFI_FILE_PROTOCOL* f,UINTN* sz,const void* content) {
    auto* p=static_cast<FakeFile*>(f);
    if(p->name=="RODOS.DAT")return 1;
    std::string& s=files[p->name];
    if(p->position+*sz>s.size())s.resize(p->position+*sz);
    std::memcpy(&s[p->position],content,*sz);p->position+=*sz;return 0;
}
EFI_STATUS EFIAPI ff_setpos(EFI_FILE_PROTOCOL* f,uint64_t pos) {
    auto* p=static_cast<FakeFile*>(f);
    if(pos==~uint64_t(0))p->position=files[p->name].size();
    else p->position=static_cast<size_t>(pos);
    return 0;
}
EFI_STATUS EFIAPI ff_info(EFI_FILE_PROTOCOL* f,const EFI_GUID*,UINTN* size,void* outp) {
    if(*size<sizeof(EFI_FILE_INFO))return 1;
    auto* p=static_cast<FakeFile*>(f);
    auto* v=static_cast<EFI_FILE_INFO*>(outp);
    v->size=sizeof(EFI_FILE_INFO);
    v->file_size=files[p->name].size();
    v->attribute=p->root?EFI_FILE_DIRECTORY:EFI_FILE_ARCHIVE;
    *size=sizeof(EFI_FILE_INFO);return 0;
}
EFI_STATUS EFIAPI ff_setinfo(EFI_FILE_PROTOCOL* f,const EFI_GUID*,UINTN,void* info) {
    auto* p=static_cast<FakeFile*>(f);
    auto* v=static_cast<EFI_FILE_INFO*>(info);
    files[p->name].resize(static_cast<size_t>(v->file_size));return 0;
}
EFI_STATUS EFIAPI ff_flush(EFI_FILE_PROTOCOL*) {++flush_count;return 0;}
FakeFile* newfile(bool root) {
    auto* f=new FakeFile{};
    f->root=root;
    f->Open=ff_open;f->Close=ff_close;f->Delete=ff_delete;
    f->Read=ff_read;f->Write=ff_write;f->SetPosition=ff_setpos;
    f->GetInfo=ff_info;f->SetInfo=ff_setinfo;f->Flush=ff_flush;
    return f;
}
EFI_STATUS EFIAPI ff_missing(EFI_FILE_PROTOCOL*,EFI_FILE_PROTOCOL**,const CHAR16*,uint64_t,uint64_t) {return 1;}
EFI_STATUS EFIAPI fs_open(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs,EFI_FILE_PROTOCOL** r) {
    if(fs==&fs_bad) { // a different FAT volume with no RodOS marker
        *r=newfile(true);
        auto* f=static_cast<FakeFile*>(*r);
        f->Open=ff_missing;
        return 0;
    }
    *r=newfile(true);return 0;
}
EFI_STATUS EFIAPI handle(EFI_HANDLE h,const EFI_GUID*,void** p){*p=h;return 0;}
EFI_STATUS EFIAPI locate(int,const EFI_GUID*,void*,UINTN* count,EFI_HANDLE** outp){
    *count=2;auto* r=new EFI_HANDLE[2];r[0]=&fs_bad;r[1]=&fs_data;*outp=r;return 0;
}
EFI_STATUS EFIAPI free_pool(void* p){delete[] static_cast<EFI_HANDLE*>(p);return 0;}
EFI_STATUS EFIAPI wait(UINTN, EFI_EVENT*,UINTN* index){*index=0;return 0;}
EFI_STATUS EFIAPI key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL*,EFI_INPUT_KEY* k){
    assert(cursor<keystrokes.size());
    k->unicode_char=static_cast<uint16_t>(keystrokes[cursor++]);return 0;
}
EFI_STATUS EFIAPI display(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*,const CHAR16* s){
    while(*s)output+=static_cast<char>(*s++);
    return 0;
}
void setup() {
    fs_bad.OpenVolume=fs_open;fs_data.OpenVolume=fs_open;
    in.ReadKeyStroke=key;
    out.OutputString=display;
    bs.entry[6]=reinterpret_cast<void*>(free_pool);
    bs.entry[9]=reinterpret_cast<void*>(wait);
    bs.entry[16]=reinterpret_cast<void*>(handle);
    bs.entry[36]=reinterpret_cast<void*>(locate);
    st.boot_services=&bs;st.runtime_services=&rt;st.con_in=&in;st.con_out=&out;
}
void run(const std::string& cmds) {
    keystrokes=cmds;cursor=0;output.clear();
    assert(efi_main(nullptr,&st)==0);
    assert(cursor==keystrokes.size());
}
}
int main(){
    setup();
    run("PERSISTTEST\rTYPE PERSIS.TXT\rEXIT\r");
    assert(files["PERSIS.TXT"]=="1");
    assert(output.find("PERSIS.TXT guardado y releido: 1")!=std::string::npos);
    run("TYPE PERSIS.TXT\rPERSISTTEST\rEXIT\r");
    assert(files["PERSIS.TXT"]=="2");
    assert(output.find("PERSIS.TXT guardado y releido: 2")!=std::string::npos);
    assert(flush_count>=2);
    run("ECHO Hola > NOTA.TXT\rTYPE NOTA.TXT\rEXIT\r");
    assert(files["NOTA.TXT"]=="Hola");
    assert(output.find("Hola")!=std::string::npos);
    run("ECHO otro >> NOTA.TXT\rREN NOTA.TXT NUEVA.TXT\rEXIT\r");
    assert(!files.count("NOTA.TXT"));
    assert(files["NUEVA.TXT"]=="Hola\r\notro");
    run("DEL RODOS.DAT\rDEL NUEVA.TXT\rEXIT\r");
    assert(files.count("RODOS.DAT") && !files.count("NUEVA.TXT"));
    std::cout<<"OK: EFI shell mocked: marker, 2 boots, PERSISTTEST 1->2, flush, echo, append, rename, delete.\n";
    return 0;
}
