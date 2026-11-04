#pragma once
#include "base.hpp"

namespace fs {
constexpr int MaxNodes = 64;
constexpr int FileCapacity = 4096; // Includes the terminating NUL.
constexpr int PathCapacity = 128;
enum class Error { Ok, NotFound, Exists, InvalidName, NotDirectory, IsDirectory,
                   Full, TooLarge, NotEmpty, Protected, InvalidPath, IoError, Unsupported };
struct Node {
    bool used;
    bool directory;
    int parent;
    char name[13];
    u32 size;
    char data[FileCapacity];
};
class RamFS {
public:
    void init();
    int resolve(const char* path) const;
    Error mkdir(const char* path);
    Error chdir(const char* path);
    Error write(const char* path, const char* text, bool append = false);
    Error remove(const char* path, bool directory);
    Error copy(const char* source, const char* destination);
    Error rename(const char* path, const char* newName);
    void path(int index, char* out, usize capacity) const;
    const Node& node(int i) const { return nodes_[i]; }
    int cwd() const { return cwd_; }
    int count() const;
    u32 bytes() const;
    static bool validName(const char* name);
private:
    Node nodes_[MaxNodes];
    int cwd_;
    int find(int parent, const char* name) const;
    Error split(const char* path, int& parent, char* name) const;
    int allocate(int parent, const char* name, bool directory);
};
const char* message(Error error);
}
