#pragma once
#include "fs.hpp"
#include "fat16.hpp"
class Shell {
public:
    void init();
    [[noreturn]] void run();
private:
    fs::RamFS disk_;
    bool batchEcho_;
    bool onUsb_ = false;
    fat::Volume usb_;
    bool isUsb(const char* path) const;
    const char* usbName(const char* path) const;
    fs::Error readText(const char* path, char* out, u32& size);
    fs::Error writeText(const char* path, const char* text, bool append=false);
    void listUsb();
    void batchText(const char* content, int depth);
    void execute(char* line, int depth=0);
    void batch(int index, int depth);
    void edit(const char* path);
    void list(const char* path);
    void tree(int index, int depth=0);
    void prompt();
};
