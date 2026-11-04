#include "../kernel/fs.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int checks=0;
void check(bool condition, const char* text, int line) {
    ++checks;
    if (!condition) { std::cerr << "FAIL line " << line << ": " << text << '\n'; std::exit(1); }
}
#define CHECK(x) check((x), #x, __LINE__)
using E=fs::Error;
fs::RamFS disk;
void invariants() {
    CHECK(disk.node(0).used && disk.node(0).directory);
    CHECK(disk.node(disk.cwd()).used && disk.node(disk.cwd()).directory);
    int count=0; unsigned bytes=0;
    for (int i=0;i<fs::MaxNodes;++i) {
        const auto& n=disk.node(i); if (!n.used) continue;
        ++count;
        CHECK(n.size<fs::FileCapacity);
        CHECK(str::len(n.data)==n.size);
        if (!n.directory) bytes+=n.size;
        if (i) {
            CHECK(fs::RamFS::validName(n.name));
            CHECK(n.parent>=0 && n.parent<fs::MaxNodes);
            CHECK(disk.node(n.parent).used && disk.node(n.parent).directory);
            int hops=0;
            for (int p=i;p;p=disk.node(p).parent) CHECK(++hops<fs::MaxNodes);
            char path[fs::PathCapacity]; disk.path(i,path,sizeof(path));
            CHECK(disk.resolve(path)==i);
            for (int j=i+1;j<fs::MaxNodes;++j) {
                const auto& m=disk.node(j);
                CHECK(!m.used || m.parent!=n.parent || !str::ieq(m.name,n.name));
            }
        }
    }
    CHECK(disk.count()==count); CHECK(disk.bytes()==bytes);
}
}
int main() {
    disk.init();
    CHECK(disk.resolve("C:\\")==0);
    CHECK(disk.resolve("..\\..\\")==0);
    CHECK(disk.resolve("D:\\")==-1);
    CHECK(disk.mkdir("work")==E::Ok);
    CHECK(disk.mkdir("WORK")==E::Exists);
    CHECK(disk.chdir("C:/WoRk")==E::Ok);
    CHECK(disk.mkdir("SUB")==E::Ok);
    CHECK(disk.write("file.txt","uno\n")==E::Ok);
    CHECK(disk.write("FILE.TXT","dos\n",true)==E::Ok);
    CHECK(str::eq(disk.node(disk.resolve("file.txt")).data,"uno\ndos\n"));
    CHECK(disk.resolve("FILE.TXT/NO.TXT")==-1);
    CHECK(disk.resolve("FILE.TXT/")==-1);
    CHECK(disk.write("FILE.TXT\\X.TXT","no")==E::NotFound);
    CHECK(disk.write("SUB","no")==E::IsDirectory);
    CHECK(disk.chdir("FILE.TXT")==E::NotDirectory);
    CHECK(disk.copy("FILE.TXT","SUB")==E::Ok);
    CHECK(str::eq(disk.node(disk.resolve("SUB\\FILE.TXT")).data,"uno\ndos\n"));
    CHECK(disk.copy("FILE.TXT","file.txt")==E::Protected);
    CHECK(disk.rename("FILE.TXT","NEXT.TXT")==E::Ok);
    CHECK(disk.resolve("FILE.TXT")==-1);
    CHECK(disk.rename("NEXT.TXT","sub")==E::Exists);
    CHECK(disk.remove("NEXT.TXT",true)==E::NotDirectory);
    CHECK(disk.remove("SUB",false)==E::IsDirectory);
    CHECK(disk.remove("SUB",true)==E::NotEmpty);
    CHECK(disk.remove(".",true)==E::Protected);
    CHECK(disk.remove("C:\\",true)==E::Protected);
    CHECK(disk.chdir("SUB")==E::Ok);
    CHECK(disk.remove("..",true)==E::Protected);
    CHECK(disk.chdir("..\\..")==E::Ok);
    char path[128]; disk.path(disk.resolve("WORK/SUB"),path,sizeof(path));
    CHECK(str::eq(path,"C:\\WORK\\SUB"));
    CHECK(disk.rename("WORK","PROJECT")==E::Ok);
    CHECK(disk.resolve("PROJECT/SUB/FILE.TXT")>=0);
    CHECK(disk.mkdir("NAMEISTOOLONG")==E::InvalidName);
    CHECK(disk.mkdir("A.BBBB")==E::InvalidName);
    CHECK(disk.mkdir("BAD NAME")==E::InvalidName);
    CHECK(disk.mkdir("A.")==E::InvalidName);
    CHECK(disk.mkdir("A/B")==E::NotFound);
    CHECK(disk.mkdir("X/")==E::InvalidPath);
    CHECK(disk.mkdir("..")==E::InvalidName);
    CHECK(disk.mkdir("A*B")==E::InvalidName);
    CHECK(disk.mkdir(".TXT")==E::InvalidName);
    CHECK(disk.mkdir("A.B.C")==E::InvalidName);
    CHECK(disk.write("EMPTY.TXT","")==E::Ok);
    std::string max(4095,'x'), oversized(4096,'y');
    CHECK(disk.write("BIG.TXT",max.c_str())==E::Ok);
    CHECK(disk.write("BIG.TXT","y",true)==E::TooLarge);
    CHECK(disk.node(disk.resolve("BIG.TXT")).size==4095);
    CHECK(disk.write("BIG.TXT",oversized.c_str())==E::TooLarge);
    CHECK(str::eq(disk.node(disk.resolve("BIG.TXT")).data,max.c_str()));
    int before=disk.count();
    CHECK(disk.write("NEW.TXT",oversized.c_str())==E::TooLarge);
    CHECK(disk.count()==before);
    invariants();

    // Capacity failures must not corrupt existing entries; freed slots are reusable.
    disk.init();
    for (int i=0;i<63;++i) CHECK(disk.write(("F"+std::to_string(i)).c_str(),"x")==E::Ok);
    CHECK(disk.count()==64);
    CHECK(disk.mkdir("NO")==E::Full);
    CHECK(disk.write("NO","x")==E::Full);
    CHECK(disk.write("F0","replacement")==E::Ok);
    CHECK(disk.remove("F1",false)==E::Ok);
    CHECK(disk.mkdir("NEW")==E::Ok);
    invariants();

    // Deep paths and renames may not create unresolvable entries.
    disk.init();
    for (int i=0;i<12;++i) { CHECK(disk.mkdir("ABCDEFGH")==E::Ok); CHECK(disk.chdir("ABCDEFGH")==E::Ok); }
    CHECK(disk.mkdir("LAST")==E::Ok);
    CHECK(disk.chdir("LAST")==E::Ok);
    CHECK(disk.write("X","ok")==E::Ok);
    CHECK(disk.rename("C:\\ABCDEFGH","ABCDEFGH.XYZ")==E::Ok);
    invariants();
    for (int i=0;i<8;++i) {
        auto e=disk.mkdir("ABCDEFGH");
        if (e!=E::Ok) { CHECK(e==E::InvalidPath); break; }
        CHECK(disk.chdir("ABCDEFGH")==E::Ok);
    }
    invariants();

    // Deterministic mutation stress checks state after successful and failed calls.
    disk.init(); unsigned seed=0x524f444f;
    for (int step=0;step<2000;++step) {
        seed=seed*1664525u+1013904223u;
        std::string name="F"+std::to_string((seed>>8)%40);
        switch (seed%5) {
            case 0: disk.write(name.c_str(),"texto\n"); break;
            case 1: disk.remove(name.c_str(),false); break;
            case 2: disk.mkdir(name.c_str()); break;
            case 3: disk.remove(name.c_str(),true); break;
            case 4: disk.copy(name.c_str(),"COPY.TXT"); break;
        }
        if (step%40==0) invariants();
    }
    invariants();
    std::cout << "PASS: RAMFS, " << checks << " assertions; 2000 mutation operations.\n";
}
