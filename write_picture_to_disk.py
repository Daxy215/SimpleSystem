import sys
import os

SECTOR_SIZE = 512
LBA_OFFSET = 150
FILENAME_DISK = "os-image.bin"

def main():
    if len(sys.argv) != 2:
        print("Usage: python write_picture_to_disk.py <image_file>")
        sys.exit(1)
    
    picture_file_name = sys.argv[1]
    
    if not os.path.exists(picture_file_name):
        print(f"Error: File '{picture_file_name}' not found.")
        sys.exit(1)
    
    if not os.path.exists(FILENAME_DISK):
        print(f"Error: Disk image '{FILENAME_DISK}' not found.")
        sys.exit(1)
    
    with open(FILENAME_DISK, "r+b") as disk_file, open(picture_file_name, "rb") as picture_file:
        picture_data = picture_file.read()
        
        # Pad the picture data to fill sectors
        if len(picture_data) % SECTOR_SIZE != 0:
            padding = SECTOR_SIZE - (len(picture_data) % SECTOR_SIZE)
            picture_data += b'\x00' * padding
        
        sectors_needed = len(picture_data) // SECTOR_SIZE
        
        # Seek to the right sector
        disk_file.seek(LBA_OFFSET * SECTOR_SIZE)
        disk_file.write(picture_data)
        
        print(f"Wrote {sectors_needed} sectors from '{picture_file_name}' to '{FILENAME_DISK}' at LBA {LBA_OFFSET}")

if __name__ == "__main__":
    main()
    
    print("Nice");
