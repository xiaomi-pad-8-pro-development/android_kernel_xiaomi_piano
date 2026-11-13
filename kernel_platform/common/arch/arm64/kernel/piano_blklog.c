// SPDX-License-Identifier: GPL-2.0-only
/*
 * Bring-up only: copy the kernel log to the start of the rawdump partition
 * every few seconds and on reboot, so a boot that never reaches adb can be
 * read back later from recovery without the log being overwritten.
 *
 * Layout: 4K header ("PIANOLOG", u32 sequence, u32 length), then the log.
 */

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/kmsg_dump.h>
#include <linux/mutex.h>
#include <linux/reboot.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#define BLKLOG_PART	"rawdump"
#define BLKLOG_SIZE	SZ_1M
#define BLKLOG_HDR	SZ_4K
/*
 * The board can lock up hard about a second into boot, so write every
 * 100 ms for the first 10 s, then every 10 s, and on reboot.
 */
#define BLKLOG_FAST	(HZ / 10)
#define BLKLOG_FAST_FOR	(10 * HZ)
#define BLKLOG_PERIOD	(HZ / 10)

static struct file *blklog_file;
static char *blklog_buf;
static u32 blklog_seq;
static DEFINE_MUTEX(blklog_lock);
static struct delayed_work blklog_work;

static int blklog_match(struct device *dev, const void *name)
{
	struct block_device *bdev = dev_to_bdev(dev);

	return bdev->bd_meta_info &&
	       !strcmp((char *)bdev->bd_meta_info->volname, name);
}

/* The partition shows up only once the UFS driver has probed. */
static struct block_device *blklog_bdev(void)
{
	struct device *dev;
	struct file *f;

	if (blklog_file)
		return file_bdev(blklog_file);

	dev = class_find_device(&block_class, NULL, BLKLOG_PART, blklog_match);
	if (!dev)
		return NULL;
	f = bdev_file_open_by_dev(dev->devt, BLK_OPEN_READ | BLK_OPEN_WRITE,
				  NULL, NULL);
	put_device(dev);
	if (IS_ERR(f)) {
		pr_err("piano_blklog: cannot open %s: %ld\n", BLKLOG_PART,
		       PTR_ERR(f));
		return NULL;
	}
	blklog_file = f;
	pr_info("piano_blklog: logging to %s\n", BLKLOG_PART);
	return file_bdev(f);
}

static void blklog_flush(void)
{
	struct kmsg_dump_iter iter;
	struct block_device *bdev;
	struct bio *bio;
	size_t len = 0;
	u32 *hdr = (u32 *)blklog_buf;
	int i, n, ret;

	mutex_lock(&blklog_lock);
	bdev = blklog_bdev();
	if (!bdev)
		goto out;

	kmsg_dump_rewind(&iter);
	kmsg_dump_get_buffer(&iter, false, blklog_buf + BLKLOG_HDR,
			     BLKLOG_SIZE - BLKLOG_HDR, &len);
	memset(blklog_buf, 0, BLKLOG_HDR);
	memcpy(blklog_buf, "PIANOLOG", 8);
	hdr[2] = ++blklog_seq;
	hdr[3] = len;

	n = DIV_ROUND_UP(BLKLOG_HDR + len, PAGE_SIZE);
	bio = bio_alloc(bdev, n, REQ_OP_WRITE | REQ_SYNC, GFP_KERNEL);
	bio->bi_iter.bi_sector = 0;
	for (i = 0; i < n; i++)
		__bio_add_page(bio, vmalloc_to_page(blklog_buf + i * PAGE_SIZE),
			       PAGE_SIZE, 0);
	ret = submit_bio_wait(bio);
	bio_put(bio);
	if (ret)
		pr_err("piano_blklog: write failed: %d\n", ret);
out:
	mutex_unlock(&blklog_lock);
}

static void blklog_work_fn(struct work_struct *work)
{
	blklog_flush();
	schedule_delayed_work(&blklog_work,
			      time_before(jiffies, INITIAL_JIFFIES + BLKLOG_FAST_FOR) ?
			      BLKLOG_FAST : BLKLOG_PERIOD);
}

/* Reboot notifiers run before device_shutdown(), while UFS still works. */
static int blklog_reboot(struct notifier_block *nb, unsigned long action,
			 void *data)
{
	blklog_flush();
	return NOTIFY_DONE;
}

static struct notifier_block blklog_reboot_nb = {
	.notifier_call = blklog_reboot,
};

static int __init blklog_init(void)
{
	blklog_buf = vzalloc(BLKLOG_SIZE);
	if (!blklog_buf)
		return -ENOMEM;
	INIT_DELAYED_WORK(&blklog_work, blklog_work_fn);
	schedule_delayed_work(&blklog_work, 0);
	register_reboot_notifier(&blklog_reboot_nb);
	return 0;
}
late_initcall(blklog_init);
