#include "hardware.hpp"
#include "shell.hpp"
static Shell shell;
extern "C" [[noreturn]] void kernel_main() {
    hw::init();
    console::init();
    shell.init();
    shell.run();
}
