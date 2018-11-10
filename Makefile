OBJECTS = loader.o kmain.o io_asm.o io.o serial.o framebuffer.o protection.o string.o
CC = g++
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
         -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -I/usr/lib/gcc/x86_64-linux-gnu/5/include -fno-rtti
LDFLAGS = -T link.ld -melf_i386
AS = nasm
ASFLAGS = -f elf

all: kernel.elf

kernel.elf: $(OBJECTS)
	ld $(LDFLAGS) $(OBJECTS) -o kernel.elf

os.iso: kernel.elf
	cp kernel.elf iso/boot/kernel.elf
	genisoimage -R                              \
	        -b boot/grub/stage2_eltorito    \
	        -no-emul-boot                   \
	        -boot-load-size 4               \
	        -A os                           \
	        -input-charset utf8             \
	        -quiet                          \
	        -boot-info-table                \
	        -o os.iso                       \
	        iso

bochsrun: os.iso
	bochs -f bochsrc.txt -q

run: os.iso
	qemu-system-i386 -cdrom os.iso -m 32 -nographic -serial mon:stdio \
	-device isa-debug-exit,iobase=0xf4,iosize=0x01 || true

%.o: %.cc
	$(CC) $(CFLAGS)  $< -o $@

%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf *.o kernel.elf os.iso
