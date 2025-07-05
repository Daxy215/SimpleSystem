#include "filesystem.h"

// 28
FATSystem* fs_createSystem(u8 partition_start) {
    // Create new file system
    FATSystem* system = (FATSystem*)memalign(8, sizeof(FATSystem));
    
    system->partition_start = partition_start;
    
    u8 buffer[512];
    lba_read(partition_start, 1, buffer);
    
    system->bs = (FAT_BootSector*)buffer;
    FAT_BootSector* bs = system->bs;
    
    if (bs->signature != 0xAA55) {
        printf("Invalid boot sector signature: %x\n", system->bs->signature);
        
        return nullptr;
    }
    
    // Thank you Mr Drakeor
    // https://drakeor.com/2022/10/12/koizos-writing-a-simple-fat16-filesystem/
    
    system->total_sectors = bs->common.bytes_per_sector;
    system->root_dir_start = /*partition_start +*/ bs->common.reserved_sector_count +
                                (bs->common.fat_count * bs->common.fat_size_16);
    
    //system->root_dir_sectors = (bs->common.root_entry_count * 32 + 
    //                          bs->common.bytes_per_sector - 1) / 
    //                          bs->common.bytes_per_sector;
    
    system->root_dir_sectors = (bs->common.root_entry_count * 32) / 512;
    
    system->bytes_per_sector = bs->common.bytes_per_sector;
    
    printf("root dir start: %d\n", system->root_dir_start);
    printf("root dir sectors: %d\n", system->root_dir_sectors);
    
    system->first_data_sector = system->root_dir_start + system->root_dir_sectors;
    
    printf("first data sector: %d\n", system->first_data_sector);
    
    system->total_sectors = bs->common.total_sectors_16 ? bs->common.total_sectors_16 : bs->common.total_sectors_32;
    system->bytes_per_cluster = system->bytes_per_sector * bs->common.sectors_per_cluster;
    
    system->totalNumOfDataSectors = system->total_sectors - (bs->common.reserved_sector_count + 
                                        (bs->common.fat_count * bs->common.fat_size_16) + system->root_dir_sectors);
    
    system->total_clusters = system->totalNumOfDataSectors / bs->common.sectors_per_cluster;
    
     // TODO; Delete later
    //printf("Bytes per sector: %d\n", (unsigned int)bs->common.bytes_per_sector);
    //printf("Sectors per cluster: %d\n", (unsigned int)bs->common.sectors_per_cluster);
    
    //printf("FAT details:\n");
    //printf("  Reserved sectors: %x\n", bs->common.reserved_sector_count);
    //printf("  FAT size: %x sectors\n", bs->common.fat_size_16);
    //printf("  Bytes per sector: %x\n", system->bytes_per_sector);
    //printf("  First data sector: %x\n", system->first_data_sector);
    //printf("  Total sectors: %x\n", system->total_sectors);
    
    if (system->bytes_per_sector == 0) {
        system->type = ExFAT;
        printf("EXFAT\n");
    } else if(system->total_clusters < 4085) {
        system->type = FAT12;
        printf("FAT12\n");
    } else if(system->total_clusters < 65525) {
        system->type = FAT16;
        printf("FAT16\n");
    } else {
        system->type = FAT32;
        printf("FAT32\n");
    }
    
    fs_refreshEntries(system);
    
    return system;
}

void fs_refreshEntries(FATSystem* fs) {
    // Read data
    // TODO; Is this always '32'?
    //fs->root_dir_sectors = 32;
    
    fs->root_dir_byte = (fs->root_dir_sectors * fs->bs->common.bytes_per_sector);
    
    // TODO; Unsure how to handle this??
    u8 RootDirBuffer[fs->root_dir_byte];
    lba_read(fs->partition_start + fs->root_dir_start, fs->root_dir_sectors, RootDirBuffer);
    
    printf("Size: %d sectors: %d\n", fs->root_dir_byte, fs->root_dir_sectors);
    
    // Max possible amount of files (For FAT16)
    fs->entries[fs->root_dir_byte / fs->root_dir_sectors];
    fs->entriesLength = fs->root_dir_byte / fs->root_dir_sectors;
    
    u16 entriesCount = 0;
    
    u32 root_dir_bytes = fs->bs->common.root_entry_count * 32;
    for (u32 i = 0; i < root_dir_bytes; i += 32) {
    //for (u32 i = 0; i < fs->root_dir_byte; i += fs->root_dir_sectors) {
        DirEntry* entry = (DirEntry*)&RootDirBuffer[i];
        
        if (entry->name[0] == 0x00) {
            // a null file
            //printf("null?\n");
            continue;
        }
        
        if ((u8)entry->name[0] == 0xE5) {
            printf("Skipp1\n");
            continue;
        }
        
        char name[12];
        u8* src = entry->name;
        
        for (int i = 0; i < 11; i++) {
            name[i] = src[i];
        }
        
        name[11] = '\0';
        
        printf("Name: %s Attr: %x Size: %x Cluster: %x\n",
               name, entry->attr, entry->size, entry->low_cluster);
        
        fs->entries[entriesCount++] = *entry;
    }
}

void fs_open(FATSystem* fs, DirEntry* file) {
    char name[12];
    u8* src = file->name;
    
    for (int i = 0; i < 11; i++) {
        name[i] = src[i];
    }
    
    name[11] = '\0';
    
    printf("Reading %s\n", name);
    printf("File size: %x bytes\n", file->size);
    
    size_t sector_count = (file->size + 511) / 512;
    printf("Sectors to read from file: %x\n", sector_count);
    u8* fileBuffer = memalign(4096, sector_count * 512);
    
    printf("Reading file...\n");
    
    // Start reading file
    u16 cluster = file->low_cluster;
    u32 offset = 0;
    
    /**
     * If "table_value" is greater than or equal to (>=) 0xFFF8,
     * then there are no more clusters in the chain.
     * This means that the whole file has been read.
     * If "table_value" equals (==) 0xFFF7 then this,
     * cluster has been marked as "bad".
     * "Bad" clusters are prone to errors and should be avoided.
     * If "table_value" is not one of the above cases then,
     * it is the cluster number of the next cluster in the file.
     */
    while (cluster >= 0x0002 && cluster < 0xFFF0) {
        //u32 sector = fs->first_data_sector + (cluster - 2) * fs->sectors_per_cluster;
        u32 sector = fs->partition_start +
             fs->first_data_sector +
             (cluster - 2) * fs->bs->common.sectors_per_cluster;
        
        printf("first: %d\n", fs->first_data_sector);
        printf("cluster - 2: %d\n", cluster - 2);
        printf("sector: %d\n", fs->bs->common.sectors_per_cluster);
        printf("Reading at: %d %x\n", sector, sector);
        
        lba_read(sector, fs->bs->common.sectors_per_cluster,
            fileBuffer + offset);
        
        offset += fs->bytes_per_cluster;
        
        // Read next FAT entry
        u32 fat_offset = cluster * 2;
        //u32 fat_sector = fs->bs->common.reserved_sector_count + (fat_offset / fs->bytes_per_sector);
        u32 fat_sector = fs->partition_start +
                 fs->bs->common.reserved_sector_count + 
                 (fat_offset / fs->bytes_per_sector);
        
        u32 ent_offset = fat_offset % fs->bytes_per_sector;
        
        u8 fat_buffer[fs->bytes_per_sector];
        if (!lba_read(fs->partition_start + fat_sector, 1, fat_buffer)) {
            printf("FAT read failed at sector %x\n", fat_sector);
            
            break;
        }
        
        // Check FAT signature
        // TODO; I'm unsure if this is needed?
        // TODO; As this completely breaks reading files
        /*if (fat_buffer[0] != bs->common.media_type) {
            printf("FAT signature mismatch: expected %x, got %x\n",
                   bs->common.media_type, fat_buffer[0]);
            
            break;
        }*/
        
        //u16 next_cluster = *(unsigned short*)&fat_buffer[ent_offset];
        u16 next_cluster = (u16)fat_buffer[ent_offset] | 
                           (u16)(fat_buffer[ent_offset + 1] << 8);
        
        // Try secondary FAT if entry is zero
        // TODO; Test this..
        if (next_cluster == 0 && fs->bs->common.fat_count > 1) {
            printf("Trying secondary FAT...\n");
            
            u32 secondary_fat = fat_sector + fs->bs->common.fat_size_16;
            if (!lba_read(fs->partition_start + secondary_fat, 1, fat_buffer)) {
                printf("Secondary FAT read failed\n");
                break;
            }
            
            next_cluster = (u16)fat_buffer[ent_offset] | 
                           (u16)(fat_buffer[ent_offset + 1] << 8);
            
            printf("Secondary FAT entry: %x -> %x\n", cluster, next_cluster);
        }
        
        cluster = next_cluster;
    }
    
    printf("byte 0: %c byte 1: %c\n", fileBuffer[0], fileBuffer[1]);
    
    printf("File has been fully read!\n");
}

// TODO; GLHF :D
void fs_write(FATSystem* fs, const char* name) {
    
}
