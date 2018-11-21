KERNEL_OBJECTS = \
	loader.o \
	loader64.o \
	multiboot_header.o \
	multiboot.o \
	kmain.o \
	io.o \
	serial.o \
	framebuffer.o \
	frame_allocator.o \
	protection.o \
	string.o \
	interrupt_handlers.o \
	interrupts.o

PROGRAM_OBJECTS = \
	program.o

CC = g++
CFLAGS = -nostdlib -nostdinc -fno-builtin -fno-stack-protector -mno-red-zone -ffreestanding -mcmodel=large \
         -mno-mmx -mno-sse -mno-sse2 -nostartfiles -nodefaultlibs -fno-rtti \
         -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables \
         -Wall -Wextra -Werror -Wno-unused-parameter -c -I/usr/lib/gcc/x86_64-linux-gnu/5/include
LDFLAGS = -n -T link.ld
AS = nasm
ASFLAGS = -f elf64

all: build/kernel.elf build/program.elf

build/kernel.elf: $(addprefix build/,$(KERNEL_OBJECTS))
	ld $(LDFLAGS) $(addprefix build/,$(KERNEL_OBJECTS)) -o $@

build/program.elf: src/program.s
	nasm -f bin src/program.s -o build/program.elf

build/os.iso: build/kernel.elf grub.cfg
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
	$(CC) $(CFLAGS)  $< -o $@

build/%.o: src/%.c
	$(CC) $(CFLAGS)  $< -o $@

build/%.o: src/%.s
	@mkdir -p build
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build
