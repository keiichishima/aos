/* -*- Mode: asm -*- */
/*_
 * Copyright 2013-2014 Scyphus Solutions Co. Ltd.  All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@scyphus.co.jp>
 */
	.set	BOOTINFO_BASE,0x8000	/* Boot information base address */
	.set	BOOTINFO_SIZE,0x100	/* Size of boot info structure */
	.set	MME_SIZE,24		/* Memory map entry size */
	.set	MME_SIGN,0x534d4150	/* MME signature (ascii "SMAP")  */

	.set	VGA_TEXT_COLOR_80x25,0x03

	.set	IVT_IRQ0,0x08		/* IRQ0=0x08 (BIOS default) */
	.set	IVT_IRQ8,0x70		/* IRQ8=0x70 (BIOS default) */

	.set	KERNEL_SEG,0x1000	/* Memory where to load kernel */


/* Temporary GDT for bootstrap processors */
	.set	GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	GDT_DATA_SEL,0x20	/* Data selector */

/* Temporary GDT for application processors */
	.set	AP_GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	AP_GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	AP_GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	AP_GDT_DATA_SEL,0x20	/* Data selector */


	.set	KERNEL_PGT,0x00079000	/* Page table */
	.set	P_DATA_SIZE,0x10000	/* Data size for each processor */
	.set	P_DATA_BASE,0x1000000	/* Data base for each processor */
	.set	P_STACK_GUARD,0x10


