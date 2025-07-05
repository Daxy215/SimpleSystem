# import os
# 
# SECTOR_SIZE = 512
# FLOPPY_SECTORS = 2880
# IMAGE_SIZE = SECTOR_SIZE * FLOPPY_SECTORS
# 
# with open("boot.bin", "rb") as f:
#     boot = f.read()
# 
# with open("kernel.bin", "rb") as f:
#     kernel = f.read()
# 
# # Create blank image
# image = bytearray(IMAGE_SIZE)
# 
# # Insert boot sector at offset 0
# image[:len(boot)] = boot
# 
# # Insert kernel at sector 1 (offset 512)
# image[SECTOR_SIZE:SECTOR_SIZE + len(kernel)] = kernel
# 
# # Write to output file
# with open("os-image.bin", "wb") as f:
#     f.write(image)
# 
# print("os-image.bin created successfully.")

import os

SECTOR_SIZE = 512
FLOPPY_SECTORS = 2880
IMAGE_SIZE = SECTOR_SIZE * FLOPPY_SECTORS

BOOT_SECTORS = 1
KERNEL_SECTORS = (len(open("kernel.bin", "rb").read()) + SECTOR_SIZE - 1) // SECTOR_SIZE
FAT16_START_SECTOR = BOOT_SECTORS + KERNEL_SECTORS  # after boot + kernel

with open("boot.bin", "rb") as f:
    boot = f.read()

with open("kernel.bin", "rb") as f:
    kernel = f.read()

KERNEL_SECTORS = (len(kernel) + SECTOR_SIZE - 1) // SECTOR_SIZE
if KERNEL_SECTORS > 128:
    print(f"WARNING: Kernel too big! {KERNEL_SECTORS} sectors")

with open("test.img", "rb") as f:
    fat16 = f.read()

# Create blank image
image = bytearray(IMAGE_SIZE)

# Write bootloader at sector 0
image[0:len(boot)] = boot

# Write kernel at sector 1
kernel_offset = SECTOR_SIZE * BOOT_SECTORS
image[kernel_offset:kernel_offset + len(kernel)] = kernel

# Write FAT16 image starting at FAT16_START_SECTOR
fat16_offset = SECTOR_SIZE * FAT16_START_SECTOR
image[fat16_offset:fat16_offset + len(fat16)] = fat16

# Write to output file
with open("os-image.bin", "wb") as f:
    f.write(image)

print(f"os-image.bin created successfully.\nKernel at sectors 1-{FAT16_START_SECTOR - 1}\nFAT16 at sector {FAT16_START_SECTOR}")
