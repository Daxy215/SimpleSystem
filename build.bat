@echo off
setlocal

:: Set toolchain path
set TOOLCHAIN=C:\Users\smsmk\Documents\elf\bin
set PATH=%TOOLCHAIN%;%PATH%

:: Create build directory
mkdir build 2> nul
pushd build

:: Build bootloader
echo Assembling bootloader...
nasm ..\boot.asm -g -f bin -o boot.bin || exit /b 1
nasm ..\isr_stubs.asm -g -f elf -o isr_stubs.o || exit /b 1

:: Build kernel with debug symbols
echo Compiling kernel...
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\kernel.c -o kernel.o              || exit /b 1

i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\decoding\pictures\bmp.c -o bmp.o  || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\idt\idt.c -o idt.o                || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\serial\serial.c -o serial.o       || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\keyboard\keyboard.c -o keyboard.o || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\memory\paging.c -o paging.o       || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\mouse\mouse.c -o mouse.o          || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\pic\pic.c -o pic.o                || exit /b 1

:: Link kernel as ELF
echo Linking kernel as ELF...
i686-elf-ld -g -T ..\linker.ld -o kernel.elf kernel.o bmp.o idt.o serial.o keyboard.o paging.o mouse.o pic.o isr_stubs.o || exit /b 1

:: Convert ELF to binary for booting
echo Converting ELF to binary...
i686-elf-objcopy -O binary kernel.elf kernel.bin || exit /b 1

echo Creating a FAT16 image through python..
python ..\assets\py_scripts\make_fat_image.py

:: Pad kernel to multiple of 512 bytes (1 sector)
:: Combine boot and kernel using Python
echo Creating OS image using Python...
python ..\make_image.py || exit /b 1

:: TODO; Move this to the filesystem
:: echo Parsing image into OS image using Python..
:: python ..\write_picture_to_disk.py ..\assets\f.bmp
:: python ..\write_picture_to_disk.py ..\mouse.bmp
:: python ..\write_picture_to_disk.py ..\all_gray.bmp

:: echo Padding kernel...
:: dd if=/dev/zero of=os-image.bin bs=512 count=2880 || exit /b 1
:: dd if=boot.bin of=os-image.bin conv=notrunc || exit /b 1
:: dd if=kernel.bin of=os-image.bin bs=512 seek=1 conv=notrunc || exit /b 1

:: Create disk image
:: echo Creating OS image...
:: dd if=boot.bin of=os-image.bin conv=notrunc || exit /b 1
:: dd if=kernel.bin of=os-image.bin bs=512 seek=1 conv=notrunc || exit /b 1

:: Verification steps
echo Verifying build...
::hexdump -C boot.bin -n 2 -s 510 || (
::    echo Missing boot signature! && exit /b 1
::)

:: Launch QEMU
qemu-system-i386 ^
  -vga std ^
  -drive format=raw,file=os-image.bin,index=0,if=ide ^
  -serial stdio ^
  -d int,cpu_reset,guest_errors ^
  -D qemu.log

popd
pause
exit /b 0

:error
echo Failed during build process.
popd
pause
exit /b 1
