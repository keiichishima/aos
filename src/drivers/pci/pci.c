/*_
 * Copyright (c) 2013 Scyphus Solutions Co. Ltd.
 * Copyright (c) 2014 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "pci.h"
#include "../../kernel/kernel.h"
#include "../../kernel/arch/x86_64/arch.h"

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc



static struct pci *pci_head;

u16
pci_read_config(u16 bus, u16 slot, u16 func, u16 offset)
{
    u32 addr;

    addr = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8)
        | ((u32)offset & 0xfc);
    /* Set enable bit */
    addr |= (u32)0x80000000;

    outl(PCI_CONFIG_ADDR, addr);
    return (inl(0xcfc) >> ((offset & 2) * 8)) & 0xffff;
}

u64
pci_read_mmio(u8 bus, u8 slot, u8 func)
{
    u64 addr;
    u32 bar0;
    u32 bar1;
    u8 type;
#if 0
    u8 prefetchable;
#endif

    bar0 = pci_read_config(bus, slot, func, 0x10);
    bar0 |= (u32)pci_read_config(bus, slot, func, 0x12) << 16;

    type = (bar0 >> 1) & 0x3;
#if 0
    prefetchable = (bar0 >> 3) & 0x1;
#endif
    addr = bar0 & 0xfffffff0;

    if ( 0x00 == type ) {
        /* 32bit */
    } else if ( 0x02 == type ) {
        /* 64bit */
        bar1 = pci_read_config(bus, slot, func, 0x14);
        bar1 |= (u32)pci_read_config(bus, slot, func, 0x16) << 16;
        addr |= ((u64)bar1) << 32;
    } else {
        return 0;
    }

    return addr;
}


u32
pci_read_rom_bar(u8 bus, u8 slot, u8 func)
{
    u8 type;
    u32 bar;

    type = pci_get_header_type(bus, slot, func);
    if ( 0x00 == type ) {
        bar = pci_read_config(bus, slot, func, 0x30);
        bar |= (u32)pci_read_config(bus, slot, func, 0x32) << 16;
    } else if ( 0x00 == type ) {
        bar = pci_read_config(bus, slot, func, 0x38);
        bar |= (u32)pci_read_config(bus, slot, func, 0x3a) << 16;
    } else {
        bar = 0;
    }

    return bar;
}








/*
 * Get header type
 */
u8
pci_get_header_type(u16 bus, u16 slot, u16 func)
{
    return pci_read_config(bus, slot, func, 0x0e) & 0xff;
}

/*
 * Check function
 */
void
pci_check_function(u8 bus, u8 slot, u8 func)
{
    u16 vendor;
    u16 device;
    u16 reg;
    u16 class;
    u16 prog;
    struct pci_device *pci_dev;
    struct pci *pci;
    struct pci **tail;

    vendor = pci_read_config(bus, slot, func, 0);
    device = pci_read_config(bus, slot, func, 2);

    /* Allocate the memory space for a PCI device */
    pci = kmalloc(sizeof(struct pci));
    if ( NULL == pci ) {
        /* Memory error */
        return;
    }
    pci_dev = kmalloc(sizeof(struct pci_device));
    if ( NULL == pci_dev ) {
        /* Memory error */
        kfree(pci);
        return;
    }

    /* Read interrupt pin and line */
    reg = pci_read_config(bus, slot, func, 0x3c);

    /* Read class and subclass */
    class = pci_read_config(bus, slot, func, 0x0a);

    /* Read program interface and revision ID */
    prog = pci_read_config(bus, slot, func, 0x08);

    pci_dev->bus = bus;
    pci_dev->slot = slot;
    pci_dev->func = func;
    pci_dev->vendor_id = vendor;
    pci_dev->device_id = device;
    pci_dev->intr_pin = (u8)(reg >> 8);
    pci_dev->intr_line = (u8)(reg & 0xff);
    pci_dev->class = (u8)(class >> 8);
    pci_dev->subclass = (u8)(class & 0xff);
    pci_dev->progif = (u8)(prog >> 8);
    pci_dev->revision = (u8)(prog & 0xff);
    pci->device = pci_dev;
    pci->next = NULL;

#if 0
    if ( vendor == 0x8086 ) {
        kprintf("%x.%x.%x %x %x %x / %x %x %x %x %x %x %x\r\n",
                bus, slot, func, vendor, device, reg,
                pci_read_config(bus, slot, func, 0x0f),
                pci_read_config(bus, slot, func, 0x06),
                pci_read_config(bus, slot, func, 0x04),
                pci_read_config(bus, slot, func, 0xf2),
                pci_read_config(bus, slot, func, 0xf0),
                pci_read_config(bus, slot, func, 0xf6),
                pci_read_config(bus, slot, func, 0xf4));
    }
#endif

    /* Search the tail */
    tail = &pci_head;
    while ( NULL != *tail ) {
        tail = &(*tail)->next;
    }
    /* Update the tail */
    *tail = pci;
}

/*
 * Check a specified device
 */
void
pci_check_device(u8 bus, u8 device)
{
    u16 vendor;
    u8 func;
    u8 hdr_type;

    func = 0;
    vendor = pci_read_config(bus, device, func, 0);
    if ( 0xffff == vendor ) {
        return;
    }

    pci_check_function(bus, device, func);
    hdr_type = pci_get_header_type(bus, device, func);

    if ( hdr_type & 0x80 ) {
        /* Multiple functions */
        for ( func = 1; func < 8; func++ ) {
            vendor = pci_read_config(bus, device, func, 0);
            if ( 0xffff != vendor ) {
                pci_check_function(bus, device, func);
            }
         }
    }
}

/*
 * Check a specified bus
 */
void
pci_check_bus(u8 bus)
{
    u8 device;

    for ( device = 0; device < 32; device++ ) {
        pci_check_device(bus, device);
    }
}

/*
 * Check all PCI buses
 */
void
pci_check_all_buses(void)
{
    u16 bus;
    u8 func;
    u8 hdr_type;
    u16 vendor;

    hdr_type = pci_get_header_type(0, 0, 0);
    if ( !(hdr_type & 0x80) ) {
        /* Single PCI host controller */
        for ( bus = 0; bus < 256; bus++ ) {
            pci_check_bus(bus);
        }
    } else {
        /* Multiple PCI host controllers */
        for ( func = 0; func < 8; func++ ) {
            vendor = pci_read_config(0, 0, func, 0);
            if ( 0xffff != vendor ) {
                break;
            }
            bus = func;
            pci_check_bus(bus);
        }
    }
}

/*
 * Initialize PCI driver
 */
void
pci_init(void)
{
    /* Reset the head of the list */
    pci_head = NULL;

    /* Search all PCI devices */
    pci_check_all_buses();

#if 0
    struct pci *pci;
    pci = pci_head;
    while ( pci ) {
        if ( pci->device->vendor_id == 0x1425 /* Chelsio */
             || (pci->device->vendor_id == 0x8086
                 && pci->device->device_id == 0x2829 /* Intel AHCI */)
             || (pci->device->vendor_id == 0x8086
                 && pci->device->device_id == 0x10fb /* Intel X520 */) ) {
            u32 x = 0;
            int j;
            for ( j = 0; j <= 5; j++ ) {
                x = pci_read_config(pci->device->bus, pci->device->slot, pci->device->func, 0x10 + j * 4);
                x |= (u32)pci_read_config(pci->device->bus, pci->device->slot, pci->device->func, 0x12 + j * 4) << 16;
                arch_dbg_printf("%d.%d.%d %.4x:%.4x BAR%d %.8x\r\n",
                                pci->device->bus, pci->device->slot,
                                pci->device->func, pci->device->vendor_id,
                                pci->device->device_id, j, x);
            }
        }
        pci = pci->next;
    }
#endif
}

struct pci *
pci_list(void)
{
    return pci_head;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
