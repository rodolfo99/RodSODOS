CXX := $(if $(CROSS),$(CROSS)g++,g++)
AS := $(if $(CROSS),$(CROSS)as,as)
LD := $(if $(CROSS),$(CROSS)ld,ld)
OBJCOPY := $(if $(CROSS),$(CROSS)objcopy,objcopy)
HOST_CXX ?= g++
PYTHON ?= python3
QEMU ?= qemu-system-i386
CXXFLAGS := -m32 -march=i686 -std=c++17 -Os -Wall -Wextra -Werror \
 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector \
 -fno-pie -fno-pic -fno-threadsafe-statics -fno-use-cxa-atexit \
 -fno-asynchronous-unwind-tables -fno-unwind-tables \
 -mno-sse -mno-sse2 -mno-mmx -msoft-float \
 -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2
SOURCES := $(wildcard kernel/*.cpp)
OBJECTS := $(patsubst kernel/%.cpp,build/%.o,$(SOURCES)) build/entry.o build/biosdisk.o
.PHONY: all clean run terminal test test-unit
all: usb

build:
	mkdir -p build
build/%.o: kernel/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
build/entry.o: boot/entry.S | build
	$(AS) --32 $< -o $@
build/biosdisk.o: boot/biosdisk.S | build
	$(AS) --32 $< -o $@

build/kernel.elf: $(OBJECTS) linker.ld
	$(LD) -m elf_i386 -nostdlib -T linker.ld -Map=build/kernel.map $(OBJECTS) -o $@
build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
build/rodos.img: build/kernel.bin boot/boot.S tools/build_image.py
	$(PYTHON) tools/build_image.py --as "$(AS)" --ld "$(LD)"
run: usb
	$(PYTHON) tools/run.py
terminal: usb
	$(PYTHON) tools/run.py --terminal
build/fs_test: tests/fs_test.cpp kernel/fs.cpp kernel/fs.hpp kernel/base.hpp | build
	$(HOST_CXX) -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/fs_test.cpp kernel/fs.cpp -o $@
test-unit: build/fs_test
	./build/fs_test
test: test-unit test-fat16
clean:
	rm -rf build
-include $(OBJECTS:.o=.d)

.PHONY: usb
usb: build/kernel.bin
	$(PYTHON) tools/build_usb_iso.py

.PHONY: test-fat16
test-fat16: usb
	$(HOST_CXX) -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g tests/fat16_test.cpp kernel/fat16.cpp kernel/fs.cpp -o build/fat16_test
	./build/fat16_test build/RodOS-0.1.2-USB-FAT16.iso
