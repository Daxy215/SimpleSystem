import os
import struct
import datetime

# === Configuration ===
OUTPUT_IMG = "fat16.img"   # Output disk image filename
SIZE_MB = 16               # Image size in MiB
FILES_TO_ADD = {
    #"boot.bin":   "BOOT    BIN",  # Files to add: source path -> 8.3 filename (space-padded)
    #"kernel.bin": "KERNEL  BIN",
    "../assets/pics/f.bmp": "F       BMP",
}

# === FAT16 parameters ===
BYTES_PER_SECTOR    = 512
SECTORS_PER_CLUSTER = 4        # Cluster size: 4 sectors = 2048 bytes (typical for FAT16)
RESERVED_SECTORS    = 1        # Reserved sectors before FATs start (boot sector)
NUM_FATS            = 2        # Number of FAT copies
MAX_ROOT_ENTRIES    = 512      # Maximum number of root directory entries (FAT16 limit)

TOTAL_SECTORS = (SIZE_MB * 1024 * 1024) // BYTES_PER_SECTOR

ROOT_DIR_SECTORS = (MAX_ROOT_ENTRIES * 32 + BYTES_PER_SECTOR - 1) // BYTES_PER_SECTOR  # round up

FAT_SIZE_SECTORS = ((TOTAL_SECTORS - RESERVED_SECTORS - ROOT_DIR_SECTORS) + (SECTORS_PER_CLUSTER * NUM_FATS)) // (SECTORS_PER_CLUSTER * NUM_FATS + 2)
if FAT_SIZE_SECTORS < 1:
    FAT_SIZE_SECTORS = 1

FIRST_DATA_SECTOR = RESERVED_SECTORS + (NUM_FATS * FAT_SIZE_SECTORS) + ROOT_DIR_SECTORS

FAT16_EOC = 0xFFF8

def make_boot_sector():
    """Build a clean 512-byte boot sector with standard BIOS Parameter Block for FAT16."""

    bs = bytearray(512)

    # Jump instruction + OEM name
    bs[0:3] = b'\xEB\x3C\x90'               # JMP instruction
    bs[3:11] = b'MKFSFAT '                  # OEM Name

    # BIOS Parameter Block (BPB)
    struct.pack_into("<H", bs, 11, BYTES_PER_SECTOR)      # Bytes per sector
    bs[13] = SECTORS_PER_CLUSTER                           # Sectors per cluster
    struct.pack_into("<H", bs, 14, RESERVED_SECTORS)      # Reserved sectors
    bs[16] = NUM_FATS                                      # Number of FATs
    struct.pack_into("<H", bs, 17, MAX_ROOT_ENTRIES)       # Max root dir entries
    struct.pack_into("<H", bs, 19, TOTAL_SECTORS if TOTAL_SECTORS < 0x10000 else 0)  # Total sectors (small)
    bs[21] = 0xF8                                         # Media descriptor
    struct.pack_into("<H", bs, 22, FAT_SIZE_SECTORS)      # FAT size sectors
    struct.pack_into("<H", bs, 24, 32)                     # Sectors per track (dummy)
    struct.pack_into("<H", bs, 26, 2)                      # Number of heads (dummy)
    struct.pack_into("<I", bs, 28, 0)                      # Hidden sectors
    struct.pack_into("<I", bs, 32, TOTAL_SECTORS if TOTAL_SECTORS >= 0x10000 else 0)  # Total sectors (large)

    # Extended BIOS Parameter Block
    bs[36] = 0x80                                         # Drive number
    bs[38] = 0x29                                         # Extended boot signature
    struct.pack_into("<I", bs, 39, 12345678)              # Volume serial number
    bs[43:54] = b'NO NAME    '                            # Volume label
    bs[54:62] = b'FAT16   '                               # File system type

    # Boot signature bytes
    bs[510:512] = b'\x55\xAA'

    return bytes(bs)

def make_empty_fat():
    """Create a blank FAT table with reserved entries."""

    entries_count = (FAT_SIZE_SECTORS * BYTES_PER_SECTOR) // 2

    entries = [0xFFF8, 0xFFFF] + [0] * (entries_count - 2)

    return struct.pack("<" + "H" * entries_count, *entries)

def make_image():
    """Create the FAT16 disk image with boot sector, FATs, root directory, and files."""

    with open(OUTPUT_IMG, "wb") as img:
        img.truncate(TOTAL_SECTORS * BYTES_PER_SECTOR)

    with open(OUTPUT_IMG, "r+b") as img:
        # Write boot sector
        img.seek(0)
        img.write(make_boot_sector())

        # Write FAT tables
        fat_data = make_empty_fat()
        for fat_idx in range(NUM_FATS):
            fat_offset = (RESERVED_SECTORS + fat_idx * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR
            img.seek(fat_offset)
            img.write(fat_data)

        # Root directory offset
        root_dir_offset = (RESERVED_SECTORS + NUM_FATS * FAT_SIZE_SECTORS) * BYTES_PER_SECTOR

        next_free_cluster = 2
        root_dir_entries = []

        for src_path, fat_name in FILES_TO_ADD.items():
            with open(src_path, "rb") as f:
                data = f.read()

            cluster_size_bytes = SECTORS_PER_CLUSTER * BYTES_PER_SECTOR
            clusters_needed = (len(data) + cluster_size_bytes - 1) // cluster_size_bytes

            clusters = list(range(next_free_cluster, next_free_cluster + clusters_needed))
            next_free_cluster += clusters_needed

            # Write file data cluster by cluster
            for i, cluster_num in enumerate(clusters):
                sector_num = FIRST_DATA_SECTOR + (cluster_num - 2) * SECTORS_PER_CLUSTER
                img.seek(sector_num * BYTES_PER_SECTOR)

                start = i * cluster_size_bytes
                chunk = data[start:start + cluster_size_bytes]
                if len(chunk) < cluster_size_bytes:
                    chunk += b'\x00' * (cluster_size_bytes - len(chunk))

                img.write(chunk)

            # Update FAT chain
            for i in range(len(clusters) - 1):
                fat_entry_pos = (RESERVED_SECTORS * BYTES_PER_SECTOR) + clusters[i] * 2
                img.seek(fat_entry_pos)
                img.write(struct.pack("<H", clusters[i + 1]))

            # Mark end of cluster chain
            fat_entry_pos = (RESERVED_SECTORS * BYTES_PER_SECTOR) + clusters[-1] * 2
            img.seek(fat_entry_pos)
            img.write(struct.pack("<H", FAT16_EOC))

            # Prepare root directory entry
            now = datetime.datetime.now()
            time_val = ((now.hour & 0x1F) << 11) | ((now.minute & 0x3F) << 5) | ((now.second // 2) & 0x1F)
            date_val = (((now.year - 1980) & 0x7F) << 9) | ((now.month & 0x0F) << 5) | (now.day & 0x1F)

            root_dir_entries.append((fat_name.encode("ascii"), clusters[0], len(data), time_val, date_val))
        
        # Write root directory entries
        img.seek(root_dir_offset)
        
        for name_bytes, first_cluster, size_bytes, time_val, date_val in root_dir_entries:
            entry = bytearray(32)
            entry[0:11] = name_bytes
            entry[11] = 0x20  # Archive attribute
            struct.pack_into("<H", entry, 22, time_val)
            struct.pack_into("<H", entry, 24, date_val)
            struct.pack_into("<H", entry, 26, first_cluster)
            struct.pack_into("<I", entry, 28, size_bytes)

            img.write(entry)

    print(f"Created {OUTPUT_IMG} ({SIZE_MB} MiB) with {len(root_dir_entries)} files.")


if __name__ == "__main__":
    make_image()
