/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */

/* $Id$ */

#include <aos/const.h>
#include "kernel.h"

static int lock;
static int routing_processor;

/*
 * Temporary: Keyboard drivers
 */
void kbd_init(void);
void kbd_event(void);
int kbd_read(void);

/* arch.c */
void arch_bsp_init(void);

/*
 * Print a panic message and hlt processor
 */
void
panic(const char *s)
{
    kprintf("%s\r\n", s);
    arch_crash();
}

int ixgbe_forwarding_test1(struct netdev *, struct netdev *);
int ixgbe_forwarding_test2(struct netdev *, struct netdev *);
extern struct netdev_list *netdev_head;

#define PERFEVTSELx_MSR_BASE 0x186      /* 3B. 18.2.2.2 */
#define PMCx_MSR_BASE 0x0c1             /* 3B. 18.2.2.2 */
#define PERFEVTSELx_EN (1<<22)
#define PERFEVTSELx_OS (1<<17)
#define PERFEVTSELx_USR (1<<16)

/*
 * Entry point to C function for BSP called from asm.s
 */
void
kmain(void)
{
    /* Initialize the lock varialbe */
    lock = 0;

    /* Routing processor */
    routing_processor = -1;

    /* Initialize kmem */
    kmem_init();

    /* Initialize keyboard */
    kbd_init();

    /* Initialize architecture-related devices */
    arch_bsp_init();

    /* Wait for 10ms */
    arch_busy_usleep(10000);

    /* Initialize random number generator */
    rng_init();
    rng_stir();

#if 0
    int i;
    for ( i = 0; i < 16; i++ ) {
        kprintf("RNG: %.8x\r\n", rng_random());
    }
#endif

    /* Print out a message */
    kprintf("\r\nStarting a shell.  Press Esc to power off the machine:\r\n");

#if 0
    u64 val;
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PERFEVTSELx_MSR_BASE));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PERFEVTSELx_MSR_BASE+1));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PMCx_MSR_BASE));
    __asm__ __volatile__ ("wrmsr" :: "a"(0), "d"(0), "c"(PMCx_MSR_BASE+1));
    val = 0x2e | (0x4f << 8) |PERFEVTSELx_EN |PERFEVTSELx_OS|PERFEVTSELx_USR;
    __asm__ __volatile__ ("wrmsr" :: "a"((u32)val), "d"(val>>32), "c"(PERFEVTSELx_MSR_BASE));
    val = 0x2e | (0x41 << 8) |PERFEVTSELx_EN |PERFEVTSELx_OS|PERFEVTSELx_USR;
    __asm__ __volatile__ ("wrmsr" :: "a"((u32)val), "d"(val>>32), "c"(PERFEVTSELx_MSR_BASE+1));

    u32 *data = kmalloc(10000*10000*sizeof(u32));
    int i,j;
    int x;
    for ( i = 0; i < 10000*10000; i++ ) {
        data[i] = i;
    }
    for ( i = 0; i < 10000*10000; i++ ) {
        x = data[i];
    }
    kfree(data);

    u32 a,d;
    __asm__ __volatile__ ("rdpmc" : "=a"(a), "=d"(d) : "c"(1));
    kprintf("RDPMC %x %x\r\n", a, d);
#endif


    struct ktask *t;

    t = ktask_alloc();
    t->main = &proc_shell;
    t->argc = 0;
    t->argv = NULL;

    arch_set_next_task(t);
    task_restart();

    halt();
}


void
ktask_entry(struct ktask *t)
{
    int ret;

    /* Get the arguments from the stack pointer */
    ret = t->main(t->argc, t->argv);

    /* ToDo: Handle return value */

    /* Avoid returning to wrong point (no returning point in the stack) */
    halt();
}

/*
 * Allocate a task
 */
struct ktask *
ktask_alloc(void)
{
    struct ktask *t;

    t = kmalloc(sizeof(struct ktask));
    if ( NULL == t ) {
        return NULL;
    }
    t->id = 0;
    t->name = NULL;
    t->pri = 0;
    t->cred = 0;
    t->state = 0;
    t->arch = arch_alloc_task(t, &ktask_entry, TASK_POLICY_KERNEL);

    t->main = NULL;
    t->argc = 0;
    t->argv = NULL;

    return t;
}


/*
 * Scheduler
 */
void
sched(void)
{
}

#if 0
void
ctxtest(void)
{

    xxx = 0;

    task1 = kmalloc(4096);
    task1->kstack = kmalloc(4096);
    task1->ustack = kmalloc(4096*0x10);
    task1->sp0 = (u64)task1->kstack + 4096 - 16; /* 16=GUARD */
    task1->rp = (struct stackframe64 *)(task1->sp0 - sizeof(struct stackframe64));
    task1->rp->gs = 0;
    task1->rp->fs = 0;
    task1->rp->ip = (u64)&proc1;
    task1->rp->cs = GDT_RING1_CODE_SEL + 1;
    task1->rp->flags = 0x1200;  /* IOPL */
    task1->rp->sp = (u64)task1->ustack + 4096 * 0x10 - 16;
    task1->rp->ss = GDT_RING1_DATA_SEL + 1;

    task2 = kmalloc(4096);
    task2->kstack = kmalloc(4096);
    task2->ustack = kmalloc(4096*0x10);
    task2->sp0 = (u64)task2->kstack + 4096 - 16; /* 16=GUARD */
    task2->rp = (struct stackframe64 *)(task2->sp0 - sizeof(struct stackframe64));
    task2->rp->gs = 0;
    task2->rp->fs = 0;
    task2->rp->ip = (u64)&proc2;
    task2->rp->cs = GDT_RING1_CODE_SEL + 1;
    task2->rp->flags = 0x1200;  /* IOPL */
    task2->rp->sp = (u64)task2->ustack + 4096 * 0x10 - 16;
    task2->rp->ss = GDT_RING1_DATA_SEL + 1;

    pdata = (struct p_data *)(P_DATA_BASE);
    pdata->cur_task = 0;
    pdata->next_task = task1;

    task_restart();
    halt();
}
#endif


/*
 * Entry point to C function for AP called from asm.s
 */
void
apmain(void)
{
    /* Initialize this AP */
    arch_ap_init();

#if 0
    arch_spin_lock(&lock);
    if ( routing_processor < 0 ) {
        proc_router();
        routing_processor = 1;
    }
    arch_spin_unlock(&lock);
#endif

#if 0
    struct netdev_list *list;

    list = netdev2_head;
    arch_spin_lock(&lock);
    if ( routing_processor < 0 ) {
        ixgbe_forwarding_test2(list->netdev, list->next->netdev);
        routing_processor = 1;
    }
    arch_spin_unlock(&lock);
#endif
}

/*
 * PIT interrupt
 */
void
kintr_int32(void)
{
    arch_clock_update();
}

/*
 * Keyboard interrupt
 */
void
kintr_int33(void)
{
    kbd_event();
}

/*
 * Local timer interrupt
 */
void
kintr_loc_tmr(void)
{
    arch_clock_update();
}

/*
 * Interrupt service routine for all vectors
 */
void
kintr_isr(u64 vec)
{
    switch ( vec ) {
    case IV_TMR:
        kintr_int32();
        break;
    case IV_KBD:
        kintr_int33();
        break;
    case IV_LOC_TMR:
        kintr_loc_tmr();
        /* Run task scheduler */
        sched();
        break;
    default:
        ;
    }
}



void set_cr3(u64);
u32 bswap32(u32);
u64 bswap64(u64);
u64 popcnt(u64);
#if 0
u64
setup_routing_page_table(void)
{
    u64 base;
    int nr0;
    int nr1;
    int nr2;
    int nr3;
    int i;

    /* 4GB + 32GB = 36GB */
    nr0 = 1;
    nr1 = 1;
    nr2 = 0x24;
    nr3 = 0x4000;

    /* Initialize page table */
    base = kmalloc((nr0 + nr1 + nr2 + nr3) * 4096);
    for ( i = 0; i < (nr0 + nr1 + nr2 + nr3) * 4096; i++ ) {
        *(u8 *)(base+i) = 0;
    }

    /* PML4E */
    *(u64 *)base = base + 0x1007;

    /* PDPTE */
    for ( i = 0; i < 0x24; i++ ) {
        *(u64 *)(base+0x1000+i*8) = base + (u64)0x1000 * i + 0x2007;
    }

    /* PDE */
    for ( i = 0; i < 0x4 * 0x200; i++ ) {
        *(u64 *)(base+0x2000+i*8) = (u64)0x00200000 * i + 0x183;
    }
    for ( i = 0; i < 0x20 * 0x200; i++ ) {
        *(u64 *)(base+0x6000+i*8) = base + (u64)0x1000 * i + 0x26007;
    }

    rng_init();
    rng_stir();
    u64 rt = kmalloc(4096 * 0x1000);
    for ( i = 0; i < 4096 * 0x1000 / 8; i++ ) {
        *(u64 *)(rt+i*8) = ((u64)rng_random()<<32) | (u64)rng_random();
    }

    /* PTE */
#if 0
    for ( i = 0; i < 0x4000 * 0x200; i++ ) {
        *(u64 *)(base+0x26000+i*8) = (u64)0x3 + (rt + (i * 8) % (4096 * 0x1000));
    }
#else
    for ( i = 0; i < 0x20 * 0x200; i++ ) {
        *(u64 *)(base+0x6000+i*8) = (u64)0x83 + (rt & 0xffffffc00000ULL);
    }
#endif
    kprintf("ROUTING TABLE: %llx %llx\r\n", base, rt);
    set_cr3(base);

    arch_dbg_printf("Looking up routing table\r\n");
    u64 x;
    u64 tmp = 0;
    for ( x = 0; x < 0x100000000ULL; x++ ) {
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)bswap32(x));
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)rng_random());
        //bswap32(x);
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + (u64)(x^tmp)&0xffffffff);
        //tmp = *(u64 *)(u64)(0x100000000ULL + ((u64)(tmp&0xffffffffULL) << 3));
        //tmp ^= *(u64 *)(u64)(0x100000000ULL + ((u64)x << 3));
        //tmp ^= *(u64 *)(u64)(0x100000000);
        //kprintf("ROUTING TABLE: %llx\r\n", *(u64 *)(u64)(0x100000000 + x));

        popcnt(x);
#if 0
        u64 m;
        __asm__ __volatile__ ("popcntq %1,%0" : "=r"(m) : "r"(x) );
        if ( m != popcnt(x) ) {
            kprintf("%.16llx %.16llx %.16llx\r\n", x, popcnt(x), m);
        }
#endif
    }
    arch_dbg_printf("done : %llx\r\n", tmp);


    return 0;
}
#endif

/*
 * Idle process
 */
int
kproc_idle_main(int argc, const char *const argv[])
{
    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
