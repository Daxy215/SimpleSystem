#pragma once

#include "../../io.h"

// This should always be the same
// TODO; Check for other types
#define END_OF_CLUSTER_MARKER 0xFFF8

// According to: https://wiki.osdev.org/FAT
#pragma pack(push, 1)

// Things that both FAT16 and FAT32 contain
typedef struct {
    u8   jump[3];               // 0x00–0x02
    char oem[8];                // 0x03–0x0A
    u16  bytes_per_sector;      // 0x0B–0x0C
    u8   sectors_per_cluster;   // 0x0D
    u16  reserved_sector_count; // 0x0E–0x0F
    u8   fat_count;             // 0x10
    u16  root_entry_count;      // 0x11–0x12
    u16  total_sectors_16;      // 0x13–0x14
    u8   media_type;            // 0x15 -> https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#BPB20_OFS_0Ah
    u16  fat_size_16;           // 0x16–0x17
    u16  sectors_per_track;     // 0x18–0x19
    u16  num_heads;             // 0x1A–0x1B
    u32  hidden_sectors;        // 0x1C–0x1F
    u32  total_sectors_32;      // 0x20–0x23
} FAT_BPB_Common;

typedef struct {
    u8   drive_number;          // 0x24
    u8   reserved1;             // 0x25
    u8   boot_signature;        // 0x26
    u32  volume_id;             // 0x27–0x2A
    char volume_label[11];      // 0x2B–0x35
} FAT_EBPB_F16;

typedef struct {
    u32 fat_size_32;            // 0x24–0x27
    u16 ext_flags;              // 0x28–0x29
    u16 fs_version;             // 0x2A–0x2B
    u32 root_cluster;           // 0x2C–0x2F
    u16 fs_info;                // 0x30–0x31
    u16 bk_boot_sector;         // 0x32–0x33
    u8  reserved[12];           // 0x34–0x3F
    u8  drive_number;           // 0x40
    u8  reserved1;              // 0x41
    u8  boot_signature;         // 0x42
    u32 volume_id;              // 0x43–0x46
    char volume_label[11];      // 0x47–0x51
    char file_sys_type[8];      // 0x52–0x59
} FAT_EBPB_F32;

// Too lazy :}
#define FAT_EBPB_MAX_SIZE  ((sizeof(FAT_EBPB_F32) > sizeof(FAT_EBPB_F16)) ? sizeof(FAT_EBPB_F32) : sizeof(FAT_EBPB_F16))

typedef struct {
    FAT_BPB_Common common;
    
    union {
        FAT_EBPB_F16 f16;
        FAT_EBPB_F32 f32;
    } ebpb;
    
    u8 boot_code[512 - sizeof(FAT_BPB_Common) - FAT_EBPB_MAX_SIZE - sizeof(u16)];
    
    u16 signature;  // 0x1FE–0x1FF (0xAA55)
} FAT_BootSector;

typedef struct {
    char name[11];     // Offset 0
    
    /**
     * READ_ONLY=0x01 HIDDEN=0x02 SYSTEM=0x04 VOLUME_ID=0x08 DIRECTORY=0x10 ARCHIVE=0x20 LFN=READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID
     * (LFN means that this entry is a long file name entry)
     */
    u8 attr;           // Offset 11
    
    u8 reserved;       // Offset 12
    u8 create_ms;      // Offset 13
    u16 create_time;   // Offset 14
    u16 create_date;   // Offset 16
    u16 access_date;   // Offset 18
    u16 high_cluster;  // Offset 20 (always 0 in FAT16)
    u16 mod_time;      // Offset 22
    u16 mod_date;      // Offset 24
    u16 low_cluster;   // Offset 26
    u32 size;          // Offset 28
} DirEntry;

// __attribute__((packed))

#pragma pack(pop)

typedef enum {
    FAT12,
    FAT16,
    FAT32,
    ExFAT
} FatType;

typedef struct {
    /**
     * Offset in sectors
     */
    u8 partition_start;
    
    u16 bytes_per_sector; // TODO; Remove
    
    u32 root_dir_byte;
    //u16 root_dir_sectors;
    
    u32 total_sectors;
    u32 totalNumOfDataSectors;
    u32 total_clusters;
    u32 root_dir_start;
    u32 root_dir_sectors;
    u32 first_data_sector;
    u32 bytes_per_cluster;
    
    FatType type;
    
    FAT_BootSector* bs;
    DirEntry* entries;
    
    u32 entriesLength;
} FATSystem;

// 28
extern FATSystem* fs_createSystem(u8 partition_start);
extern void fs_refreshEntries(FATSystem* fs);

extern void fs_open(FATSystem* fs, DirEntry* file);
extern void fs_write(FATSystem* fs, const char* name);
