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
nasm ..\boot.asm -g -f bin -o boot.bin         || exit /b 1
nasm ..\isr_stubs.asm -g -f elf -o isr_stubs.o || exit /b 1

:: Build kernel with debug symbols
echo Compiling kernel...
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\kernel.c -o kernel.o                                 || exit /b 1

i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\decoding\pictures\bmp.c -o bmp.o                     || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\idt\idt.c -o idt.o                                   || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\serial\serial.c -o serial.o                          || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\keyboard\keyboard.c -o keyboard.o                    || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\memory\filesystem\filesystem.c -o filesystem.o       || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\memory\paging.c -o paging.o                          || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\mouse\mouse.c -o mouse.o                             || exit /b 1
i686-elf-gcc -m32 -ffreestanding -nostdlib -g -c ..\SimpleSystem\kernal\pic\pic.c -o pic.o                                   || exit /b 1

:: Link kernel as ELF
echo Linking kernel as ELF...
i686-elf-ld -g -T ..\linker.ld -o kernel.elf kernel.o bmp.o idt.o serial.o keyboard.o filesystem.o paging.o mouse.o pic.o isr_stubs.o || exit /b 1

:: Convert ELF to binary for booting
echo Converting ELF to binary...
i686-elf-objcopy -O binary kernel.elf kernel.bin || exit /b 1

echo Creating a FAT16 image through python..
python ..\assets\py_scripts\make_fat_image.py || exit /b 1

:: Pad kernel to multiple of 512 bytes (1 sector)
echo Creating OS image using Python...
python ..\make_image.py || exit /b 1

:: Launch QEMU

:: For Debugging
:: -s -S
:: gdb kernal.elf
:: target remote localhost:1234
:: break *0x1000 -> Entry point

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
