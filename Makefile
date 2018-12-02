KERNEL_OBJECTS = \
	assertions.o \
	elf.o \
	framebuffer.o \
	frame_allocator.o \
	interrupt_handlers.o \
	interrupts.o \
	io.o \
	kmain.o \
	loader.o \
	loader64.o \
	multiboot_header.o \
	multiboot.o \
	page_tables.o \
	protection.o \
	serial.o \
	builtins/string.o \
	syscall.o \
	thread.o

PROGRAM_OBJECTS = \
	program.o

build/tests/linked_list_test: TEST_SRCS =
build/tests/allocator_test: TEST_SRCS = frame_allocator.cc

CC = g++
CFLAGS = -nostdlib -nostdinc -fno-builtin -fno-stack-protector -mno-red-zone -ffreestanding -mcmodel=large \
         -mno-mmx -mno-sse -mno-sse2 -nostartfiles -nodefaultlibs -fno-rtti \
         -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables \
         -Wall -Wextra -Werror -Wno-unused-parameter -isystem /usr/lib/gcc/x86_64-linux-gnu/5/include \
         -isystem src/builtins
LDFLAGS = -n -T link.ld
AS = nasm
ASFLAGS = -f elf64

GTEST = googletest/googletest

all: build/kernel.elf build/program.elf

build/kernel.elf: $(addprefix build/,$(KERNEL_OBJECTS))
	ld $(LDFLAGS) $(addprefix build/,$(KERNEL_OBJECTS)) -o $@

build/program.elf: src/program/program.cc
	$(CC) $(CFLAGS) $< -o $@

build/os.iso: build/kernel.elf build/program.elf grub.cfg
	@mkdir -p build/iso/boot/grub
	@mkdir -p build/iso/modules
	@cp build/kernel.elf build/iso/boot
	@cp build/program.elf build/iso/modules
	@cp grub.cfg build/iso/boot/grub
	grub-mkrescue /usr/lib/grub/i386-pc -o build/os.iso build/iso
	@rm -r build/iso

bochsrun: build/os.iso
	bochs -f bochsrc.txt -q

run: build/os.iso
	qemu-system-x86_64 -cdrom build/os.iso -serial mon:stdio \
	-device isa-debug-exit,iobase=0xf4,iosize=0x01 || true

build/%.o: src/%.cc
	@mkdir -p build
	@mkdir -p build/builtins
	$(CC) $(CFLAGS)  -c $< -o $@

build/%.o: src/%.s
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

build/libgtest.a: $(GTEST)/src/gtest-all.cc
	g++ -std=c++11 -isystem $(GTEST)/include -I$(GTEST) -pthread -c $(GTEST)/src/gtest-all.cc -o build/gtest-all.o
	ar -rv build/libgtest.a build/gtest-all.o

build/tests/%: src/tests/%.cc $(addprefix src/,$(TEST_SRCS)) build/libgtest.a
	@mkdir -p build/tests
	g++ -g -DTESTING -std=c++11 -o $@ -Isrc -I$(GTEST)/include $< $(addprefix src/,$(TEST_SRCS)) -pthread build/libgtest.a

clean:
	rm -rf build
