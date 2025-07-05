#ifndef PAGING_H
#define PAGING_H

#include "../io.h"

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024
#define PAGE_PRESENT 0x1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4
#define PAGE_GLOBAL      (1 << 8)

typedef struct {
	u32* page_directory;
	u32* page_tables[PAGE_ENTRIES];
	u8 tables_allocated[PAGE_ENTRIES];
	u8 paging_active;
} Pager;

// Function prototypes
Pager* pager_create(void);
void pager_map_range(Pager* pager, u32 virt_start, u32 phys_start, u32 size, u32 flags);
void pager_map_page(Pager* pager, u32 virt_addr, u32 phys_addr, u32 flags);
void pager_identity_map(Pager* pager, u32 phys_start, u32 size, u32 flags);
void pager_enable(Pager* pager);
void pager_destroy(Pager* pager);

// Memory management
//void* memset(void* ptr, int value, size_t num);
//void* memalign(size_t alignment, size_t size);

#endif // PAGING_H