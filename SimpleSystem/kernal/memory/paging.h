#ifndef PAGING_H
#define PAGING_H

#include "../io.h"

#define PAGE_SIZE 4096

// This needs to be 512,
// once long mode is ready
#define PAGE_ENTRIES 1024 // 512

#define PAGE_PRESENT    0x001	// Present
#define PAGE_WRITE      0x002	// Read/Write
#define PAGE_USER       0x004	// User/Supervisor
#define PAGE_PWT        0x008	// Write-Through
#define PAGE_PCD        0x010	// Cache Disable
#define PAGE_ACCESSED   0x020	// Accessed..
#define PAGE_DIRTY      0x040	// Dirty
#define PAGE_HUGE       0x080	// Page Size >= 2MB or 1GB page?
#define PAGE_GLOBAL     0x100	//
#define PAGE_NO_EXECUTE (1ULL << 63)

typedef struct {
	u32* page_directory;
	u32* page_tables[PAGE_ENTRIES];
	u8 tables_allocated[PAGE_ENTRIES];
	u8 paging_active;
} Pager;

Pager* pager_create(void);

void pager_map_range(Pager* pager, u32 virt_start, u32 phys_start, u32 size, u32 flags);
void pager_map_page(Pager* pager, u32 virt_addr, u32 phys_addr, u32 flags);
void pager_identity_map(Pager* pager, u32 phys_start, u32 size, u32 flags);
void pager_enable(Pager* pager);
void pager_destroy(Pager* pager);

#endif // PAGING_H
