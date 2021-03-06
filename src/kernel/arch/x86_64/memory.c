/*_
 * Copyright 2013 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "../../kernel.h"
#include "memory.h"
#include "arch.h"
#include "bootinfo.h"

/* Flags */
#define PHYS_MEM_USED           (u64)1
#define PHYS_MEM_WIRED          (u64)(1<<1)
#define PHYS_MEM_HEAD           (u64)(1<<2)
#define PHYS_MEM_UNAVAIL        (u64)(1<<16)

#define PHYS_MEM_IS_FREE(x)     (0 == (x)->flags ? 1 : 0)
#define PHYS_MEM_PAGE_POS(p)    (((u64)(p) - (u64)phys_mem->pages)  \
                                 / (sizeof(struct phys_mem_page)))

#define FLOOR(val, base)        ((val) / (base)) * (base)
#define CEIL(val, base)         (((val) - 1) / (base) + 1) * (base)


static volatile int memory_lock;
static volatile struct phys_mem *phys_mem;




/*
 * Initialize physical memory
 *    This is not thread safe.  Call this from BSP.
 */
int
phys_mem_init(struct bootinfo *bi)
{
    struct bootinfo_sysaddrmap_entry *bse;
    u64 nr;
    u64 addr;
    u64 sz;
    u64 a;
    u64 b;
    u64 i;
    u64 j;
    int k;
    int flag;

    /* Clear lock variable */
    memory_lock = 0;

    /* Clear physical memory space */
    phys_mem = NULL;

    /* Check the number of address map entries */
    if ( bi->sysaddrmap.n <= 0 ) {
        return -1;
    }

    /* Obtain usable memory size */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            if ( bse->base + bse->len > addr ) {
                addr = bse->base + bse->len;
            }
        }
    }

    /* Calculate required memory size for pages */
    nr = CEIL(addr, PAGESIZE) / PAGESIZE;
    sz = nr * sizeof(struct phys_mem_page) + sizeof(struct phys_mem);

    /* Search free space system address map obitaned from BIOS */
    addr = 0;
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE);
            b = FLOOR(bse->base + bse->len, PAGESIZE);

            if ( b < PHYS_MEM_FREE_ADDR ) {
                /* Skip */
                continue;
            } else if ( a < PHYS_MEM_FREE_ADDR ) {
                if ( b - PHYS_MEM_FREE_ADDR >= sz ) {
                    addr = PHYS_MEM_FREE_ADDR;
                    break;
                } else {
                    continue;
                }
            } else {
                if ( b - a >= sz ) {
                    addr = a;
                    break;
                } else {
                    continue;
                }
            }
        }
    }

    /* Could not find */
    if ( 0 == addr ) {
        return -1;
    }

    /* Setup */
    phys_mem = (struct phys_mem *)(addr + nr * sizeof(struct phys_mem_page));
    phys_mem->nr = nr;
    phys_mem->pages = (struct phys_mem_page *)addr;

    /* Reset flags */
    for ( i = 0; i < phys_mem->nr; i++ ) {
        /* Mark as unavailable */
        phys_mem->pages[i].flags = PHYS_MEM_UNAVAIL;
        phys_mem->pages[i].lru = NULL;
        phys_mem->pages[i].next = NULL;
        phys_mem->pages[i].order = -1;
    }

    /* Check system address map obitaned from BIOS */
    for ( i = 0; i < bi->sysaddrmap.n; i++ ) {
        bse = &bi->sysaddrmap.entries[i];
        if ( 1 == bse->type ) {
            /* Available */
            a = CEIL(bse->base, PAGESIZE) / PAGESIZE;
            b = FLOOR(bse->base + bse->len, PAGESIZE) / PAGESIZE;

            /* Mark as unallocated */
            for ( j = a; j < b; j++ ) {
                if ( j >= phys_mem->nr ) {
                    /* Error */
                    return -1;
                }
                /* Unmark unavailable */
                phys_mem->pages[j].flags &= ~PHYS_MEM_UNAVAIL;
                if ( j * PAGESIZE <= PHYS_MEM_FREE_ADDR ) {
                    /* Wired by kernel */
                    phys_mem->pages[j].flags |= PHYS_MEM_WIRED;
                }
            }
        }
    }

    /* Mark self */
    for ( i = addr / PAGESIZE; i < CEIL(addr + sz, PAGESIZE) / PAGESIZE; i++ ) {
        phys_mem->pages[i].flags |= PHYS_MEM_WIRED;
    }

    /* Initialize buddy system */
    for ( k = 0; k < PHYS_MEM_BUDDY_ORDER; k++ ) {
        phys_mem->buddy.o[k].head = NULL;
    }
    for ( k = PHYS_MEM_BUDDY_ORDER-1; k >= 0; k-- ) {
        for ( i = 0; i < phys_mem->nr; i += (1<<k) ) {
            flag = 0;
            if ( phys_mem->pages[i].order >= 0 ) {
                /* Already used in a higher order buddy list */
                flag = 1;
                break;
            }
            if ( !flag ) {
                /* Not used in a higher order buddy list */
                flag = 1;
                for ( j = 0; i < (1<<k); j++ ) {
                    if ( !PHYS_MEM_IS_FREE(&phys_mem->pages[i+j]) ) {
                        /* Not free */
                        flag = 0;
                        break;
                    }
                }
                if ( flag ) {
                    /* All pages are free */
                    phys_mem->pages[i].next = phys_mem->buddy.o[k].head;
                    phys_mem->buddy.o[k].head = &phys_mem->pages[i];
                    for ( j = 0; i < (1<<k); j++ ) {
                        phys_mem->pages[k].order = k;
                    }
                }
            }
        }
    }

    return 0;
}

#if 0
/*
 * Wire
 */
int
phys_mem_wire(void *page, u64 size)
{
    u64 i;
    u64 a;
    u64 b;

    spin_lock(&memory_lock);

    a = FLOOR((u64)page, PAGESIZE) / PAGESIZE;
    b = CEIL((u64)page + size, PAGESIZE) / PAGESIZE;

    /* Check first */
    for ( i = a; i < b; i++ ) {
        if ( !PHYS_MEM_IS_FREE(&phys_mem->pages[i]) ) {
            return -1;
        }
    }

    /* Wired */
    for ( i = a; i < b; i++ ) {
        phys_mem->pages[i].flags |= PHYS_MEM_WIRED;

        /* FIXME: Buddy system support */
    }

    spin_unlock(&memory_lock);

    return 0;
}
#endif


/*
 * Allocate n pages for kernel
 */
void *
phys_mem_alloc_pages(u64 n)
{
    u64 i;
    u64 f;
    u64 cnt;

    /* Check address first */
    if ( n > phys_mem->nr ) {
        return NULL;
    }

    spin_lock(&memory_lock);

#if 1
    cnt = 0;
    f = 0;
    for ( i = 0; i < phys_mem->nr; i++ ) {
        if ( PHYS_MEM_IS_FREE(&phys_mem->pages[i]) ) {
            if ( !cnt ) {
                f = i;
            }
            cnt++;
            if ( cnt == n ) {
                break;
            }
        } else {
            f = 0;
            cnt = 0;
        }
    }

    if ( cnt == n ) {
        phys_mem->pages[i].flags = PHYS_MEM_HEAD;
        for ( i = f; i < f + n; i++ ) {
            phys_mem->pages[i].flags |= PHYS_MEM_USED;
        }
        spin_unlock(&memory_lock);
        return (void *)(f * PAGESIZE);
    } else {
        spin_unlock(&memory_lock);
        return NULL;
    }
#endif

#if 0
    u64 j;
    int o;
    u64 tmp;

    /* Calculate order */
    tmp = n - 1;
    o = 0;
    while ( tmp ) {
        tmp >>= 1;
        o++;
    }

    for ( i = o; i < PHYS_MEM_BUDDY_ORDER; i++ ) {
        if ( NULL != phys_mem->buddy.o[i].head ) {
            /* Found */
            f = PHYS_MEM_PAGE_POS(phys_mem->buddy.o[i].head);
            for ( j = 0; j < n; j++ ) {
                PHYS_MEM_PAGE_POS(phys_mem->buddy.o[i].head);
                phys_mem->pages[f + j].flags | PHYS_MEM_USED;
            }

            spin_unlock(&memory_lock);
            return (void *)(PHYS_MEM_PAGE_POS(phys_mem->buddy.o[i].head)
                            * PAGESIZE);
        }
    }

    spin_unlock(&memory_lock);
    return NULL;
#endif
}

/*
 * Free allocated pages
 */
void
phys_mem_free_pages(void *page)
{
    u64 i;
    u64 p;

    p = (u64)page / PAGESIZE;
    if ( p >= phys_mem->nr ) {
        /* Invalid page number */
        return;
    }

    spin_lock(&memory_lock);

    if ( !(phys_mem->pages[p].flags & PHYS_MEM_HEAD) ) {
        /* Invalid page type */
        spin_unlock(&memory_lock);
        return;
    }
    phys_mem->pages[p].flags &= ~PHYS_MEM_HEAD;

    /* Free */
    for ( i = p; i < phys_mem->nr; i++ ) {
        if ( phys_mem->pages[i].flags & PHYS_MEM_HEAD ) {
            /* Another allocation begins */
            break;
        } else if ( phys_mem->pages[i].flags & PHYS_MEM_USED ) {
            /* Free */
            phys_mem->pages[i].flags &= ~PHYS_MEM_USED;
        } else {
            /* This allocation ends */
            break;
        }
    }

    spin_unlock(&memory_lock);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
