KERNEL_OBJECTS = \
	assert.o \
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
	string.o \
	syscall.o \
	thread.o

PROGRAM_OBJECTS = \
	program.o

CC = g++
CFLAGS = -nostdlib -nostdinc -fno-builtin -fno-stack-protector -mno-red-zone -ffreestanding -mcmodel=large \
         -mno-mmx -mno-sse -mno-sse2 -nostartfiles -nodefaultlibs -fno-rtti \
         -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables \
         -Wall -Wextra -Werror -Wno-unused-parameter -I/usr/lib/gcc/x86_64-linux-gnu/5/include
LDFLAGS = -n -T link.ld
AS = nasm
ASFLAGS = -f elf64

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
	$(CC) $(CFLAGS)  -c $< -o $@

build/%.o: src/%.s
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build
