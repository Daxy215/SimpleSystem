#include "paging.h"

#include "../serial/serial.h"

Pager* pager_create(void) {
    Pager* pager = (Pager*)memalign(4, sizeof(Pager)); // 4-byte aligned
    
    if (!pager) return 0;
    
    // Allocate page directory (must be page-aligned)
    pager->page_directory = (u32*)memalign(PAGE_SIZE, PAGE_ENTRIES * sizeof(u32));
    
    if (!pager->page_directory) return 0;
    
    memset(pager->page_directory, 0, PAGE_ENTRIES * sizeof(u32));
    
    // Initialize table tracking
    memset(pager->tables_allocated, 0, sizeof(pager->tables_allocated));
    pager->paging_active = 0;
    
    return pager;
}

void pager_map_range(Pager* pager, u32 virt_start, u32 phys_start, u32 size, u32 flags) {
    u32 virt_end = virt_start + size;
    u32 aligned_start = virt_start & ~(PAGE_SIZE-1);
    u32 aligned_end = (virt_end + PAGE_SIZE-1) & ~(PAGE_SIZE-1);
    
    for (u32 virt = aligned_start; virt < aligned_end; virt += PAGE_SIZE) {
        u32 phys = phys_start + (virt - virt_start);
        
        pager_map_page(pager, virt, phys, flags);
    }
}

void pager_map_page(Pager* pager, u32 virt_addr, u32 phys_addr, u32 flags) {
    u32 pdi = virt_addr >> 22;           // Page directory index
    u32 pti = (virt_addr >> 12) & 0x3FF; // Page table index
    
    // Create page table if needed
    if (!pager->tables_allocated[pdi]) {
        // Allocate new page table (must be page-aligned)
        u32* pt = (u32*)memalign(PAGE_SIZE, PAGE_ENTRIES * sizeof(u32));
        if (!pt) return;
        memset(pt, 0, PAGE_ENTRIES * sizeof(u32));
        pager->page_tables[pdi] = pt;
        pager->tables_allocated[pdi] = 1;
        
        // Set directory entry
        pager->page_directory[pdi] = (u32)pt | PAGE_PRESENT | PAGE_WRITE;
    }
    
    // Set page table entry
    u32* pt = pager->page_tables[pdi];
    pt[pti] = (phys_addr & ~0xFFF) | (flags & 0xFFF) | PAGE_PRESENT;
    
    // Flush TLB if paging active
    if (pager->paging_active) {
        asm volatile("invlpg (%0)" ::"r"(virt_addr) : "memory");
    }
}

void pager_identity_map(Pager* pager, u32 phys_start, u32 size, u32 flags) {
    pager_map_range(pager, phys_start, phys_start, size, flags);
}

void pager_enable(Pager* pager) {
    // Load CR3
    asm volatile("mov %0, %%cr3" :: "r"(pager->page_directory));
    
    // Enable paging
    u32 cr0;
    asm volatile(
        "mov %%cr0, %0\n\t"
        "or $0x80000001, %0\n\t"
        "mov %0, %%cr0"
        : "=r"(cr0)
        :
        : "memory"
    );
    
    pager->paging_active = 1;
}

void pager_destroy(Pager* pager) {
    // TODO; free memory here
    if (pager) pager->paging_active = 0;
}