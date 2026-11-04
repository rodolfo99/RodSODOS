#include "fs.hpp"

namespace fs {
void RamFS::init() {
    for (auto& n : nodes_) { n.used = false; n.size = 0; n.data[0] = 0; }
    nodes_[0].used = nodes_[0].directory = true;
    nodes_[0].parent = 0; nodes_[0].name[0] = 0; cwd_ = 0;
}
int RamFS::find(int parent, const char* name) const {
    for (int i=1; i<MaxNodes; ++i)
        if (nodes_[i].used && nodes_[i].parent == parent && str::ieq(nodes_[i].name, name)) return i;
    return -1;
}
bool RamFS::validName(const char* name) {
    int base = 0, ext = 0; bool dot = false;
    for (; *name; ++name) {
        char c = str::upper(*name);
        if (c == '.') { if (dot || base == 0) return false; dot = true; continue; }
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
        if (dot) { if (++ext > 3) return false; }
        else if (++base > 8) return false;
    }
    return base > 0 && (!dot || ext > 0);
}
int RamFS::resolve(const char* p) const {
    if (!p || str::len(p) >= PathCapacity) return -1;
    usize length = str::len(p);
    bool mustBeDirectory = length && str::separator(p[length-1]);
    int current = cwd_;
    if (p[0] && p[1] == ':') {
        if (str::upper(*p) != 'C') return -1;
        p += 2;
    }
    if (str::separator(*p)) current = 0;
    while (*p) {
        while (str::separator(*p)) ++p;
        if (!*p) break;
        if (!nodes_[current].directory) return -1;
        char part[13]; int n = 0;
        while (*p && !str::separator(*p)) { if (n >= 12) return -1; part[n++] = *p++; }
        part[n] = 0;
        if (str::eq(part, ".")) continue;
        if (str::eq(part, "..")) { current = nodes_[current].parent; continue; }
        if (!validName(part)) return -1;
        current = find(current, part);
        if (current < 0) return -1;
    }
    // A trailing slash denotes a directory, never a regular file.
    return mustBeDirectory && !nodes_[current].directory ? -1 : current;
}
Error RamFS::split(const char* p, int& parent, char* name) const {
    usize n = str::len(p);
    if (!n || n >= PathCapacity || str::separator(p[n-1])) return Error::InvalidPath;
    int start = 0;
    for (int i=0; i<int(n); ++i) if (str::separator(p[i]) || p[i] == ':') start = i+1;
    if (!validName(p+start)) return Error::InvalidName;
    str::copy(name, p+start, 13);
    for (char* q = name; *q; ++q) *q = str::upper(*q);
    if (!start) parent = cwd_;
    else {
        char prefix[PathCapacity]; str::copy(prefix, p, usize(start)+1);
        parent = resolve(prefix);
    }
    if (parent < 0) return Error::NotFound;
    if (!nodes_[parent].directory) return Error::NotDirectory;
    char parentPath[PathCapacity]; path(parent, parentPath, sizeof(parentPath));
    if (str::len(parentPath) + str::len(name) + 1 >= PathCapacity) return Error::InvalidPath;
    return Error::Ok;
}
int RamFS::allocate(int parent, const char* name, bool directory) {
    for (int i=1; i<MaxNodes; ++i) if (!nodes_[i].used) {
        auto& n = nodes_[i]; n.used = true; n.directory = directory;
        n.parent = parent; str::copy(n.name, name, sizeof(n.name));
        n.size = 0; n.data[0] = 0; return i;
    }
    return -1;
}
Error RamFS::mkdir(const char* p) {
    int parent; char name[13]; Error e = split(p, parent, name);
    if (e != Error::Ok) return e;
    if (find(parent, name) >= 0) return Error::Exists;
    return allocate(parent, name, true) < 0 ? Error::Full : Error::Ok;
}
Error RamFS::chdir(const char* p) {
    int i = resolve(p);
    if (i < 0) return Error::NotFound;
    if (!nodes_[i].directory) return Error::NotDirectory;
    cwd_ = i; return Error::Ok;
}
Error RamFS::write(const char* p, const char* text, bool append) {
    usize size = str::len(text);
    if (size >= FileCapacity) return Error::TooLarge;
    int parent; char name[13]; Error e = split(p, parent, name);
    if (e != Error::Ok) return e;
    int i = find(parent, name);
    if (i >= 0 && nodes_[i].directory) return Error::IsDirectory;
    usize old = append && i >= 0 ? nodes_[i].size : 0;
    if (old + size >= FileCapacity) return Error::TooLarge;
    if (i < 0) i = allocate(parent, name, false);
    if (i < 0) return Error::Full;
    str::copy(nodes_[i].data + old, text, FileCapacity-old);
    nodes_[i].size = u32(old+size); return Error::Ok;
}
Error RamFS::remove(const char* p, bool directory) {
    int i = resolve(p);
    if (i < 0) return Error::NotFound;
    if (nodes_[i].directory != directory) return nodes_[i].directory ? Error::IsDirectory : Error::NotDirectory;
    if (directory) {
        if (!i) return Error::Protected;
        for (int c=cwd_; c; c=nodes_[c].parent) if (c == i) return Error::Protected;
        for (int c=1; c<MaxNodes; ++c) if (nodes_[c].used && nodes_[c].parent == i) return Error::NotEmpty;
    }
    nodes_[i].used = false; return Error::Ok;
}
Error RamFS::copy(const char* source, const char* destination) {
    int src = resolve(source);
    if (src < 0) return Error::NotFound;
    if (nodes_[src].directory) return Error::IsDirectory;
    int dst = resolve(destination);
    char finalPath[PathCapacity];
    if (dst >= 0 && nodes_[dst].directory) {
        path(dst, finalPath, sizeof(finalPath));
        usize n = str::len(finalPath);
        if (n + 1 + str::len(nodes_[src].name) >= sizeof(finalPath)) return Error::InvalidPath;
        if (!str::separator(finalPath[n-1])) finalPath[n++] = '\\';
        str::copy(finalPath+n, nodes_[src].name, sizeof(finalPath)-n);
        destination = finalPath;
    }
    if (resolve(destination) == src) return Error::Protected;
    return write(destination, nodes_[src].data);
}
Error RamFS::rename(const char* p, const char* newName) {
    int i = resolve(p);
    if (i < 0) return Error::NotFound;
    if (!i) return Error::Protected;
    if (!validName(newName)) return Error::InvalidName;
    int duplicate = find(nodes_[i].parent, newName);
    if (duplicate >= 0 && duplicate != i) return Error::Exists;
    // Keep every descendant's printable path within PathCapacity.
    int delta = int(str::len(newName)) - int(str::len(nodes_[i].name));
    for (int j=1; j<MaxNodes; ++j) if (nodes_[j].used) {
        for (int c=j; c; c=nodes_[c].parent) if (c == i) {
            char full[PathCapacity]; path(j, full, sizeof(full));
            if (int(str::len(full)) + delta >= PathCapacity-1) return Error::InvalidPath;
            break;
        }
    }
    str::copy(nodes_[i].name, newName, sizeof(nodes_[i].name));
    for (char* q=nodes_[i].name; *q; ++q) *q=str::upper(*q);
    return Error::Ok;
}
void RamFS::path(int i, char* out, usize capacity) const {
    int chain[MaxNodes]; int depth=0;
    while (i && depth < MaxNodes) { chain[depth++] = i; i = nodes_[i].parent; }
    if (capacity < 4) { if (capacity) *out=0; return; }
    str::copy(out, "C:\\", capacity); usize n=3;
    while (depth) {
        const char* name = nodes_[chain[--depth]].name;
        for (; *name && n+1<capacity; ++name) out[n++]=*name;
        if (depth && n+1<capacity) out[n++]='\\';
    }
    out[n]=0;
}
int RamFS::count() const { int n=0; for (const auto& node : nodes_) if (node.used) ++n; return n; }
u32 RamFS::bytes() const { u32 n=0; for (const auto& node : nodes_) if (node.used && !node.directory) n+=node.size; return n; }
const char* message(Error e) {
    switch (e) {
    case Error::Ok: return "OK";
    case Error::NotFound: return "Archivo o ruta no encontrados.";
    case Error::Exists: return "Ya existe ese nombre.";
    case Error::InvalidName: return "Nombre invalido: use 8.3, letras, numeros, _ o -.";
    case Error::NotDirectory: return "La ruta no es un directorio.";
    case Error::IsDirectory: return "La ruta es un directorio.";
    case Error::Full: return "RAM llena: limite de 64 entradas, incluida la raiz.";
    case Error::TooLarge: return "Archivo demasiado grande (maximo 4095 bytes).";
    case Error::NotEmpty: return "El directorio no esta vacio.";
    case Error::Protected: return "Operacion no permitida sobre esa ruta.";
    case Error::InvalidPath: return "Ruta invalida o demasiado larga.";
    case Error::IoError: return "Fallo al leer o escribir el disco BIOS.";
    case Error::Unsupported: return "Operacion no disponible en D: (solo raiz FAT16).";
    }
    return "Error.";
}
}
