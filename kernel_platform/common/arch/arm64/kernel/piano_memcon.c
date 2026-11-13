// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bring-up only: copy the kernel log into the unused ramoops dmesg zone from
 * right after paging_init(), before ramoops itself is up. Each write is
 * cleaned to PoC so it survives a warm reset.
 */

#include <linux/console.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/cacheflush.h>
#include <asm/early_ioremap.h>

/*
 * Two rings, used in turn on each boot, so a boot that falls back to
 * recovery does not overwrite the log of the boot that failed. The boot
 * counter is stored 32 times and read by a per-bit majority vote, because
 * the memory gets bit flips over a warm reset.
 */
#define MEMCON_RING0	0xa3600000
#define MEMCON_RING1	0xa3780000
#define MEMCON_COUNT	0xa35c0000
#define MEMCON_COUNT_COPIES	32
#define MEMCON_SIZE	0x99000
/* The RAM keeps ~20% bad bytes over a warm reset: write 5 copies, vote per bit. */
#define MEMCON_COPIES	5
#define MEMCON_MAGIC	0x4d454d43	/* "MEMC" */
/* early_memremap() maps at most 256K per slot. */
#define MEMCON_EARLY_SIZE	0x3f000

static u8 *memcon;
static phys_addr_t memcon_base;
static u32 memcon_size = MEMCON_EARLY_SIZE;
/* piano_memcon_copies=1..5 on the command line; parsed before paging_init(). */
static int memcon_copies = MEMCON_COPIES;

static int __init memcon_copies_setup(char *arg)
{
	int v;

	if (!kstrtoint(arg, 0, &v) && v >= 1 && v <= MEMCON_COPIES)
		memcon_copies = v;
	return 0;
}
early_param("piano_memcon_copies", memcon_copies_setup);

static void memcon_write(struct console *con, const char *s, unsigned int n)
{
	/* Bring-up: keep userspace (init) lines and kernel errors only. */
	if (con && !strnstr(s, "init", n) && !strnstr(s, "rror", n) &&
	    !strnstr(s, "ail", n) && !strnstr(s, "anic", n) && !strnstr(s, "memcon", n) &&
	    !strnstr(s, "remoteproc", n) && !strnstr(s, "q6v5", n) &&
	    !strnstr(s, "irmware", n) && !strnstr(s, "adsp", n))
		return;
	/* The old 6.6 dlkm modules flood the log with this; skip it. */
	if (con && (strnstr(s, "linkonce", n) || strnstr(s, "Loaded kernel module", n) ||
		    strnstr(s, "init: Loading module", n)))
		return;
	u32 *hdr = (u32 *)memcon;
	u32 pos = hdr[1];

	u32 ring = memcon_size - 16, start = pos % ring, len = n;

	int c, copies = memcon_size == MEMCON_SIZE ? memcon_copies : 1;

	for (c = 0; c < copies; c++) {
		u8 *m = memcon + c * MEMCON_SIZE;
		const char *t = s;
		u32 p = pos, k = len;

		while (k--) {
			m[16 + p % ring] = *t++;
			p++;
		}
		((u32 *)m)[0] = MEMCON_MAGIC;
		((u32 *)m)[1] = p;
		((u32 *)m)[2] = hdr[2];
		/* Clean only the header and the bytes just written (whole ring on wrap). */
		dcache_clean_poc((unsigned long)m, (unsigned long)m + 16);
		if (start + len <= ring)
			dcache_clean_poc((unsigned long)m + 16 + start,
					 (unsigned long)m + 16 + start + len);
		else
			dcache_clean_poc((unsigned long)m + 16,
					 (unsigned long)m + memcon_size);
	}
}

static struct console memcon_console = {
	.name	= "memcon",
	.write	= memcon_write,
	.flags	= CON_PRINTBUFFER | CON_ANYTIME | CON_ENABLED,
	.index	= -1,
};

static u32 __init memcon_next_boot(void)
{
	u32 *cnt, boot = 0;
	int i, bit;

	cnt = early_memremap(MEMCON_COUNT, PAGE_SIZE);
	if (!cnt)
		return 0;
	for (bit = 0; bit < 32; bit++) {
		int votes = 0;

		for (i = 0; i < MEMCON_COUNT_COPIES; i++)
			votes += (cnt[i] >> bit) & 1;
		if (votes * 2 > MEMCON_COUNT_COPIES)
			boot |= 1U << bit;
	}
	boot++;
	for (i = 0; i < MEMCON_COUNT_COPIES; i++)
		cnt[i] = boot;
	dcache_clean_poc((unsigned long)cnt,
			 (unsigned long)(cnt + MEMCON_COUNT_COPIES));
	early_memunmap(cnt, PAGE_SIZE);
	return boot;
}

/* Right after early_ioremap_init(), through a temporary fixmap mapping. */
void __init piano_memcon_early_init(void)
{
	static const char hello[] = "memcon: early\n";
	u32 boot = memcon_next_boot();

	memcon_base = MEMCON_RING0;
	memcon = early_memremap(memcon_base, MEMCON_EARLY_SIZE);
	if (!memcon)
		return;
	memset(memcon, 0, 16);
	((u32 *)memcon)[2] = boot;
	memcon_write(NULL, hello, sizeof(hello) - 1);
	register_console(&memcon_console);
}

/* After paging_init(): move to the linear map, keeping what was written. */
void __init piano_memcon_init(void)
{
	static const char hello[] = "memcon: linear map\n";
	u8 *early = memcon;

	if (!early)
		return;
	console_lock();
	memcon = phys_to_virt(memcon_base);
	/* Before this point the ring was the early fixmap size and had not wrapped. */
	memcon_size = MEMCON_SIZE;
	{
		int c;

		for (c = 1; c < memcon_copies; c++)
			memcpy(memcon + c * MEMCON_SIZE, memcon, MEMCON_SIZE);
		dcache_clean_poc((unsigned long)memcon,
				 (unsigned long)memcon + MEMCON_COPIES * MEMCON_SIZE);
	}
	console_unlock();
	early_memunmap(early, MEMCON_EARLY_SIZE);
	memcon_write(NULL, hello, sizeof(hello) - 1);
}

/*
 * Bring-up: piano_memcon.reboot_after=<seconds> warm-resets into the
 * bootloader that long after boot, so a 6.12 boot that never reaches adb
 * still returns without a long power-key reset (which lets the RAM log
 * decay). 0 (default) disables it.
 */
#include <linux/moduleparam.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>

static unsigned int reboot_after;
module_param(reboot_after, uint, 0644);

static void memcon_reboot_fn(struct work_struct *work)
{
	pr_emerg("memcon: %u s up, restarting into the bootloader\n", reboot_after);
	kernel_restart("bootloader");
}
static DECLARE_DELAYED_WORK(memcon_reboot_work, memcon_reboot_fn);

static int __init memcon_reboot_init(void)
{
	if (reboot_after)
		schedule_delayed_work(&memcon_reboot_work, reboot_after * HZ);
	return 0;
}
late_initcall(memcon_reboot_init);
