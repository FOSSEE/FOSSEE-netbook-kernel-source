/*++
/some descriptions of this software.
Copyright ©2014 WonderMediaTechnologies, Inc.
This program is free software: you can redistribute it and/or modify it under 
the terms of the GNU General Public License as published by the Free 
Software Foundation, either version 2 of the License, or(at your option) any 
later version.

This program is distributed in the hope that it will be useful,but WITHOUT 
ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESSFOR A PARTICULAR PURPOSE. See 
the GNU General Public License for more details.You should have received 
a copy of the GNU General Public License along with this program. If not, 
see <http://www.gnu.org/licenses/>.
WonderMediaTechnologies, Inc.
4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/

/*
 * Swap block device support for MTDs
 * Turns an MTD device into a swap device with block wear leveling
 *
 * Copyright 漏 2007,2011 Nokia Corporation. All rights reserved.
 *
 * Authors: Jarkko Lavinen <jarkko.lavinen@nokia.com>
 *
 * Based on Richard Purdie's earlier implementation in 2007. Background
 * support and lock-less operation written by Adrian Hunter.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/blktrans.h>
#include <linux/kthread.h>
#include <linux/blkdev.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/swap.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/math64.h>
#include <linux/random.h>
#include <linux/suspend.h>

#define MTDSWAP_VERSION "1.0"

#define MTDSWAP_SECTOR_SIZE      4096
#define MTDSWAP_SECTOR_SHIFT     12

#define STATUS_FREE              (0xff)
#define STATUS_USED              (0x55)

#define MTDSWAP_IO_RETRIES       3

int suspend_counts __nosavedata;
int eba_tbl[1024] __nosavedata;

enum {
	MTDSWAP_SCANNED_FREE,
	MTDSWAP_SCANNED_USED,
	MTDSWAP_SCANNED_BAD,
};

struct mtdswap_oobdata {
	unsigned int magic;
	unsigned int erase_count;
	unsigned int lnum;
	unsigned int seq_number;
};

struct mtdswap_eb {

	union {
		struct rb_node rb;
		struct rb_root *root;
	} u;

	unsigned int erase_count;
	unsigned int lnum;
	unsigned int pnum;
	unsigned int seq_number;
};

struct mtdswap_dev {

	struct mtd_blktrans_dev *mbd;
	struct mtd_info *mtd;	/* mtd device */
	struct device *dev;
	struct notifier_block pm_notifier;

	struct mtdswap_eb *eb_data;
	spinlock_t mtdswap_lock;
	struct rb_root used;
	struct rb_root free;

	unsigned int pblocks;
	unsigned int badblock;
	unsigned int freeblock;
	unsigned int usedblock;

	unsigned int page_per_block;
	unsigned int sector_per_block;
	unsigned int mean_count;
	unsigned int seq_number;

	struct mutex cache_mutex;
	unsigned char *cache_data;
	unsigned long cache_offset;
	unsigned int cache_size;
	unsigned char *oob_data;
	enum { STATE_EMPTY, STATE_CLEAN, STATE_DIRTY } cache_state;

};

#define MTDSWAP_MBD_TO_MTDSWAP(dev) ((struct mtdswap_dev *)dev->priv)

unsigned char partitions[32] = "16";

extern char resume_file[64];	/*defined in hibernation.c*/
static char *parts = NULL;		/* str: mtd part number defined by resume_file */
static unsigned long part = 0;	/* mtd part number defined by resume_file */

static DEFINE_MUTEX(mtdswap_lock);

extern void print_nand_buffer(char *value, unsigned int length);

static void mtdswap_cleanup(struct mtdswap_dev *d);
static int mtdswap_check_resume(struct mtdswap_dev *d);
static int swap_tree_add(struct mtdswap_eb *eb, struct rb_root *root);
static loff_t mtdswap_eb_offset(struct mtdswap_dev *d, struct mtdswap_eb *eb);

void print_mapping_table(struct mtdswap_dev *d)
{
	int i;
	for (i = 0; i < d->pblocks; i++)
		printk("\n After checking, lnum%d pnum%d", i, eba_tbl[i]);
}

static void swaptree_destroy(struct rb_root *root)
{
	struct rb_node *rb;
	struct mtdswap_eb *e;

	rb = root->rb_node;
	while (rb) {
		if (rb->rb_left)
			rb = rb->rb_left;
		else if (rb->rb_right)
			rb = rb->rb_right;
		else {
			e = rb_entry(rb, struct mtdswap_eb, u.rb);

			rb = rb_parent(rb);
			if (rb) {
				if (rb->rb_left == &e->u.rb)
					rb->rb_left = NULL;
				else
					rb->rb_right = NULL;
			}
			/* kfree(e); */
		}
	}
}

static void mtdswap_cleanup(struct mtdswap_dev *d)
{
	swaptree_destroy(&d->used);
	swaptree_destroy(&d->free);
	vfree(d->eb_data);
	vfree(d->cache_data);
	vfree(d->oob_data);
}

static unsigned int get_logic_block(struct mtdswap_dev *d, unsigned int pos)
{
	return pos / d->mtd->erasesize;
}

static unsigned int get_logic_page(struct mtdswap_dev *d, unsigned int pos)
{
	return pos % d->mtd->erasesize;
}

struct mtdswap_eb *find_mtdswap_eb(struct rb_root *root, int diff)
{

	struct rb_node *p;
	struct mtdswap_eb *e;

	e = rb_entry(rb_first(root), struct mtdswap_eb, u.rb);

	p = root->rb_node;

	while (p) {
		struct mtdswap_eb *e1;

		e1 = rb_entry(p, struct mtdswap_eb, u.rb);
		if (e1->erase_count > diff)
			p = p->rb_left;
		else {
			p = p->rb_right;
			e = e1;
		}
	}
	return e;
}

static int find_new_block(struct mtdswap_dev *d, int lnum)
{
	/* first we find block from free tree */
	int key = 0;
	struct mtdswap_eb *eb;

	d->seq_number++;
	eb = find_mtdswap_eb(&d->free, key);

	if (eb == NULL) {
		eb = find_mtdswap_eb(&d->used, key);
		if (eb == NULL)
			return -1;
		rb_erase(&eb->u.rb, &d->used);
		eb->erase_count++;
		eb->lnum = lnum;
		eb->seq_number = d->seq_number;

	} else {
		rb_erase(&eb->u.rb, &d->free);
		if (eb->erase_count == 0)
			eb->erase_count = d->mean_count;
		eb->lnum = lnum;
		eb->seq_number = d->seq_number;
	}
	eba_tbl[lnum] = eb->pnum;
	return eb->pnum;
}

static int mtdswap_handle_badblock(struct mtdswap_dev *d, struct mtdswap_eb *eb)
{
	int ret;
	loff_t offset;

	if (!mtd_can_have_bb(d->mtd))
		return 1;

	offset = mtdswap_eb_offset(d, eb);
	dev_warn(d->dev, "Marking bad block at %08llx\n", offset);
	ret = mtd_block_markbad(d->mtd, offset);
	if (ret) {
		dev_warn(d->dev, "Mark block bad failed for block at %08llx "
			 "error %d\n", offset, ret);
		return ret;
	}

	return 1;

}

static int swap_erase(struct mtdswap_dev *d, struct erase_info *erase)
{
	struct mtd_info *mtd = d->mtd;
	struct mtdswap_eb *eb;
	unsigned long pos = erase->addr;
	int lnum = get_logic_block(d, pos);
	int page = get_logic_page(d, pos);
	int pnum, ret = 0, retries = 0;

	if (eba_tbl[lnum] != -1) {
		eb = d->eb_data + eba_tbl[lnum];
		spin_lock(&d->mtdswap_lock);
		swap_tree_add(eb, &d->used);
		spin_unlock(&d->mtdswap_lock);
	}

RETRY:
	spin_lock(&d->mtdswap_lock);
	pnum = find_new_block(d, lnum);
	/*printk("\n lnum %d -> %d", lnum, pnum); */
	spin_unlock(&d->mtdswap_lock);
	if (pnum == -1)
		return -EIO;

	eb = d->eb_data + pnum;
	erase->addr = pnum * mtd->erasesize + page;

	ret = mtd_erase(mtd, erase);

	if (ret) {
		mtdswap_handle_badblock(d, eb);
		retries++;
		if (retries > MTDSWAP_IO_RETRIES)
			return -EIO;
		goto RETRY;
	}
	return 0;
}

static int mtdswap_write_marker(struct mtdswap_dev *d, struct mtdswap_eb *eb,
				loff_t offset, size_t len, unsigned char *buf)
{
	struct mtdswap_oobdata *data;
	struct mtd_info *mtd = d->mtd;
	int ret;
	struct mtd_oob_ops ops;

	data = (struct mtdswap_oobdata *)d->oob_data;
	ops.len = ((len >= mtd->writesize) ? mtd->writesize : len);
	ops.ooblen = 16;
	ops.oobbuf = d->oob_data;
	ops.ooboffs = 0;
	ops.datbuf = buf;
	ops.mode = MTD_OPS_AUTO_OOB;

	data->magic = cpu_to_le32(STATUS_USED);
	data->erase_count = cpu_to_le32(eb->erase_count);
	data->lnum = cpu_to_le32(eb->lnum);
	data->seq_number = cpu_to_le32(eb->seq_number);

	ret = mtd_write_oob(mtd, offset, &ops);

	return ret;
}

static int swap_write(struct mtdswap_dev *d, unsigned long pos, size_t len,
		      size_t *retlen, unsigned char *buf)
{
	struct mtd_info *mtd = d->mtd;
	int lnum = get_logic_block(d, pos);
	int page = get_logic_page(d, pos);
	int pnum = eba_tbl[lnum];
	unsigned long addr = pnum * mtd->erasesize + page;
	struct mtdswap_eb *eb = d->eb_data + pnum;
	int ret;

	*retlen = len;
	/* First, write datbuf and oobbuf */
	ret = mtdswap_write_marker(d, eb, addr, len, buf);
	if (ret) {
		mtdswap_handle_badblock(d, eb);
		return ret;
	}
	/* Second, just write databuf */
	len -= mtd->writesize;
	if (len <= 0)
		return 0;
	ret =
	    mtd_write(mtd, addr + mtd->writesize, len, retlen,
		      buf + mtd->writesize);
	/*printk("\nwrite data to %d, %s", pnum, current->comm); */
	if (ret) {
		mtdswap_handle_badblock(d, eb);
		return ret;
	}
	*retlen += mtd->writesize;

	return ret;
}

static int swap_read(struct mtdswap_dev *d, unsigned long pos, size_t len,
		     size_t *retlen, unsigned char *buf)
{
	struct mtd_info *mtd = d->mtd;
	int lnum = get_logic_block(d, pos);
	int page = get_logic_page(d, pos);
	int pnum = eba_tbl[lnum];
	unsigned long addr = pnum * mtd->erasesize + page;
	/*
	   printk("\nread data from pos 0x%lx, lnum %d, pnum%d page%d",
	   pos, lnum, pnum, page);
	 */
	if (pnum == -1) {
		*retlen = len;
		return 0;
	}

	return mtd_read(mtd, addr, len, retlen, buf);
}

static int swap_read_oob(struct mtdswap_dev *d, loff_t from,
			 struct mtd_oob_ops *ops)
{
	int ret = mtd_read_oob(d->mtd, from, ops);

	return ret;
}

static void erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *) done->priv;
	wake_up(wait_q);
}

static int erase_write(struct mtdswap_dev *d, unsigned long pos,
		       int len, unsigned char *buf)
{
	struct erase_info erase;
	struct mtd_info *mtd = d->mtd;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	size_t retlen;
	int ret, retries = 0;
	/*
	 * First, let's erase the flash block.
	 */
#if 0
	if (pos == 0x0)
		printk("\n Update Swap Header!");
#endif
RETRY:
	init_waitqueue_head(&wait_q);
	erase.mtd = mtd;
	erase.callback = erase_callback;
	erase.len = len;
	erase.addr = pos;
	erase.priv = (u_long) & wait_q;

	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&wait_q, &wait);
	ret = swap_erase(d, &erase);
	if (ret) {
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&wait_q, &wait);
		return ret;
	}

	schedule();		/* Wait for erase to finish. */
	remove_wait_queue(&wait_q, &wait);
	/*
	 * Next, write the data to flash.
	 */

	ret = swap_write(d, pos, len, &retlen, buf);
	if (ret) {
		retries++;
		if (retries > MTDSWAP_IO_RETRIES)
			return -EIO;
		goto RETRY;
	}
	if (retlen != len)
		return -EIO;
	return 0;
}

static int write_cached_data(struct mtdswap_dev *d)
{
	int ret;
	if (d->cache_state != STATE_DIRTY)
		return 0;

	ret = erase_write(d, d->cache_offset, d->cache_size, d->cache_data);
	if (ret)
		return ret;
	d->cache_state = STATE_EMPTY;
	return 0;
}

static int do_cached_write(struct mtdswap_dev *d, unsigned long pos,
			   unsigned int len, unsigned char *buf)
{
	unsigned int sect_size = d->cache_size;
	size_t retlen;
	int ret;
	/* print_nand_buffer(buf, len); */
	while (len > 0) {
		unsigned long sect_start = (pos / sect_size) * sect_size;
		unsigned int offset = pos - sect_start;
		unsigned int size = sect_size - offset;
		if (size > len)
			size = len;
		if (size == sect_size) {
			ret = erase_write(d, pos, size, buf);
			if (ret)
				return ret;
		} else {
			if (d->cache_state == STATE_DIRTY &&
			    d->cache_offset != sect_start) {
				mutex_lock(&d->cache_mutex);
				ret = write_cached_data(d);
				mutex_unlock(&d->cache_mutex);
				if (ret)
					return ret;
			}

			if (d->cache_state == STATE_EMPTY ||
			    d->cache_offset != sect_start) {
				d->cache_state = STATE_EMPTY;
				ret = swap_read(d, sect_start, sect_size,
						&retlen, d->cache_data);
				if (ret)
					return ret;

				if (retlen != sect_size)
					return -EIO;

				d->cache_offset = sect_start;
				d->cache_state = STATE_CLEAN;
			}
			memcpy(d->cache_data + offset, buf, size);
			d->cache_state = STATE_DIRTY;
		}
		buf += size;
		pos += size;
		len -= size;
	}
	return 0;
}

static int do_cached_read(struct mtdswap_dev *d, unsigned long pos,
			  int len, char *buf)
{
	unsigned int sect_size = d->cache_size;
	size_t retlen;
	int ret;
	/* printk("\n Read data from pos 0x%lx, len 0x%x", pos, len); */
	mutex_lock(&d->cache_mutex);
	while (len > 0) {

		unsigned long sect_start = (pos / sect_size) * sect_size;
		unsigned int offset = pos - sect_start;
		unsigned int size = sect_size - offset;

		if (size > len)
			size = len;
		if (d->cache_state != STATE_EMPTY &&
		    d->cache_offset == sect_start) {
			memcpy(buf, d->cache_data + offset, size);
		} else {
			ret = swap_read(d, pos, size, &retlen, buf);
			if (ret)
				return ret;
			if (retlen != size)
				return -EIO;
		}
		/* print_nand_buffer(buf, len); */
		buf += size;
		pos += size;
		len -= size;
	}

	mutex_unlock(&d->cache_mutex);
	return 0;
}

static int mtdswap_flush(struct mtd_blktrans_dev *dev)
{
	struct mtdswap_dev *d = MTDSWAP_MBD_TO_MTDSWAP(dev);
	mutex_lock(&d->cache_mutex);
	write_cached_data(d);
	mutex_unlock(&d->cache_mutex);
	mtd_sync(d->mtd);
	return 0;
}

static int mtdswap_readsect(struct mtd_blktrans_dev *dev, unsigned long block,
			    char *buf)
{
	struct mtdswap_dev *d = MTDSWAP_MBD_TO_MTDSWAP(dev);

	if (likely(dev->mtd->writesize >= MTDSWAP_SECTOR_SIZE))
		return do_cached_read(d, block << MTDSWAP_SECTOR_SHIFT,
				      MTDSWAP_SECTOR_SIZE, buf);

	return do_cached_read(d, block << 9, 512, buf);
}

static int mtdswap_writesect(struct mtd_blktrans_dev *dev, unsigned long block,
			     char *buf)
{

	struct mtdswap_dev *d = MTDSWAP_MBD_TO_MTDSWAP(dev);
	if (likely(dev->mtd->writesize >= MTDSWAP_SECTOR_SIZE))
		return do_cached_write(d, block << MTDSWAP_SECTOR_SHIFT,
				       MTDSWAP_SECTOR_SIZE, buf);

	return do_cached_write(d, block << 9, 512, buf);
}

static void mtdswap_remove_dev(struct mtd_blktrans_dev *dev)
{
	struct mtdswap_dev *d = MTDSWAP_MBD_TO_MTDSWAP(dev);
	del_mtd_blktrans_dev(dev);
	mtdswap_cleanup(d);
	kfree(d);
}

static loff_t mtdswap_eb_offset(struct mtdswap_dev *d, struct mtdswap_eb *eb)
{
	return (loff_t) (eb - d->eb_data) * d->mtd->erasesize;
}

static int mtdswap_read_markers(struct mtdswap_dev *d, struct mtdswap_eb *eb)
{
	struct mtdswap_oobdata *data;
	int ret;
	loff_t offset;
	struct mtd_oob_ops ops;

	offset = mtdswap_eb_offset(d, eb);
	if (mtd_can_have_bb(d->mtd) && mtd_block_isbad(d->mtd, offset)) {
		d->badblock++;
		return MTDSWAP_SCANNED_BAD;
	}

	ops.ooblen = 16;
	ops.oobbuf = d->oob_data;
	ops.ooboffs = 0;
	ops.datbuf = NULL;
	ops.mode = MTD_OPS_AUTO_OOB;
	ret = swap_read_oob(d, offset, &ops);
	data = (struct mtdswap_oobdata *)d->oob_data;

	if (le32_to_cpu(data->magic) == STATUS_USED) {
		eb->erase_count = le32_to_cpu(data->erase_count);
		eb->lnum = le32_to_cpu(data->lnum);
		eb->seq_number = le32_to_cpu(data->seq_number);
		d->usedblock++;
		d->mean_count += eb->erase_count;

		if (eb->seq_number > d->seq_number)
			d->seq_number = eb->seq_number;
		ret = MTDSWAP_SCANNED_USED;
	} else {
		eb->erase_count = 0;
		d->freeblock++;
		ret = MTDSWAP_SCANNED_FREE;
	}
	eb->pnum = (unsigned int)(eb - d->eb_data);
	return ret;

}

static int swap_tree_add(struct mtdswap_eb *eb, struct rb_root *root)
{
	struct rb_node **p, *parent = NULL;

	p = &root->rb_node;
	while (*p) {
		struct mtdswap_eb *eb1;
		parent = *p;
		eb1 = rb_entry(parent, struct mtdswap_eb, u.rb);

		if (eb->erase_count < eb1->erase_count)
			p = &(*p)->rb_left;
		else if (eb->erase_count > eb1->erase_count)
			p = &(*p)->rb_right;
		else {
			if (eb->pnum == eb1->pnum)
				return 0;

			if (eb->pnum < eb1->pnum)
				p = &(*p)->rb_left;
			else
				p = &(*p)->rb_right;
		}

	}

	rb_link_node(&eb->u.rb, parent, p);
	rb_insert_color(&eb->u.rb, root);

	return 0;
}

static int build_mapping_table(struct mtdswap_dev *d, struct mtdswap_eb *eb)
{

	int pnum;
	struct mtdswap_eb *eb1;
	pnum = eba_tbl[eb->lnum];

	if (pnum >= 0) {
		eb1 = d->eb_data + pnum;
		if (eb1->seq_number > eb->seq_number)
			return 0;
	}

	eba_tbl[eb->lnum] = eb->pnum;
	return 0;
}

static int mtdswap_check_counts(struct mtdswap_dev *d)
{
	return (d->pblocks - d->usedblock - d->freeblock - d->badblock) ? 1 : 0;
}

static int mtdswap_scan_eblks(struct mtdswap_dev *d, unsigned int need_build)
{
	int status, i;
	struct mtdswap_eb *eb;

	for (i = 0; i < d->pblocks; i++) {
		eb = d->eb_data + i;
		eb->pnum = i;
		status = mtdswap_read_markers(d, eb);
		if (status == MTDSWAP_SCANNED_BAD)
			continue;
		switch (status) {
		case MTDSWAP_SCANNED_FREE:
			spin_lock(&d->mtdswap_lock);
			swap_tree_add(eb, &d->free);
			spin_unlock(&d->mtdswap_lock);
			break;
		case MTDSWAP_SCANNED_USED:
			spin_lock(&d->mtdswap_lock);
			swap_tree_add(eb, &d->used);
			spin_unlock(&d->mtdswap_lock);
			if(need_build)
				build_mapping_table(d, eb);
			break;
		}
	}

	if (mtdswap_check_counts(d))
		printk(KERN_CRIT "\n NOTICE: MTDSWAP counts are illegal");

	return 0;
}

#if 0
static void test_swap(struct mtdswap_dev *d)
{
	unsigned long start_sector = 0x0;
	unsigned long sector_count = 0;
	unsigned long rand_seed = 544;
	unsigned char write_data = 0;
	unsigned int i;
	int ret;

	for (i = 0; i < 10000; i++) {
		/* seed the randome: no seed to freeze the test case */
		srandom32(random32() + i + rand_seed);

/*      start_sector = (unsigned long)(random32()%(d->sector_per_block * 64)) & (~(32-1));
		rand_seed = (unsigned long)(random32()%(d->sector_per_block * 64-start_sector));
*/
		write_data = (unsigned char)(random32() % ((unsigned char)-1));
		sector_count = 1;

		/* set data */
		memset(sector_buffer, (unsigned char)write_data, 2097152);

		/* write */
/*      ret = ONFM_Write(c, start_sector, sector_count, sector_buffer); */
		ret = do_cached_write(d, start_sector, 2097152, sector_buffer);
/*      ret = erase_write(d, start_sector, 512, sector_buffer); */
		if (ret == 0) {
			/* read and check */
			ret =
			    do_cached_read(d, start_sector, 2097152,
					   read_sector_buffer);
			if (ret == 0) {
				ret =
				    memcmp(sector_buffer, read_sector_buffer,
					   2097152);
			}
		}

		/* print */
		if (ret != 0) {
			printk
			    ("\n%d:*FAIL* start address: %d, sector count: %d, data: %d",
			     i, start_sector, sector_count, write_data);
			break;
		} else {
			printk
			    ("\n%d-PASS. start address: %d, sector count: %d, data: %d.",
			     i, start_sector, sector_count, write_data);
			start_sector += 0x200;
		}
	}
}
#endif

static int mtdswap_check_resume(struct mtdswap_dev *d)
{
	struct mtd_info *mtd = d->mtd;
	struct mtdswap_eb *eb;

	spin_lock(&d->mtdswap_lock);
	swaptree_destroy(&d->used);
	swaptree_destroy(&d->free);
	spin_unlock(&d->mtdswap_lock);
	d->mean_count = 1;
	d->used = d->free = RB_ROOT;
	d->badblock = d->freeblock = d->usedblock = 0;
	memset(d->eb_data, 0x00, sizeof(struct mtdswap_eb) * d->pblocks);

	mutex_lock(&d->cache_mutex);
	d->cache_size = mtd->erasesize;
	d->cache_state = STATE_EMPTY;
	d->cache_offset = -1;
	memset(d->cache_data, 0xFF, mtd->erasesize);
	mutex_unlock(&d->cache_mutex);

	memset(d->oob_data, 0xFF, mtd->oobsize);
	mtdswap_scan_eblks(d, 0);
	eb = d->eb_data + eba_tbl[0];
	spin_lock(&d->mtdswap_lock);
	rb_erase(&eb->u.rb, &d->used);
	spin_unlock(&d->mtdswap_lock);
#if 0
	for (i = 0; i < d->pblocks; i++) {
		if (eba_tbl[i] != -1) {
			eb = d->eb_data + eba_tbl[i];
			printk("\n Remove %d from used tree", eb->pnum);
			rb_erase(&eb->u.rb, &d->used);
		}
	}
#endif
	if (d->usedblock)
		d->mean_count = d->mean_count / d->usedblock;
	return 0;
}

static int mtdswap_check_suspend(struct mtdswap_dev *d)
{
	struct mtd_info *mtd = d->mtd;
	struct mtdswap_eb *eb;
	int i;

	spin_lock(&d->mtdswap_lock);
	swaptree_destroy(&d->used);
	swaptree_destroy(&d->free);
	spin_unlock(&d->mtdswap_lock);

	d->mean_count = 1;
	d->used = d->free = RB_ROOT;
	d->badblock = d->freeblock = d->usedblock = 0;
	memset(d->eb_data, 0x00, sizeof(struct mtdswap_eb) * d->pblocks);
	mutex_lock(&d->cache_mutex);
	d->cache_size = mtd->erasesize;
	d->cache_state = STATE_EMPTY;
	d->cache_offset = -1;
	memset(d->cache_data, 0xFF, mtd->erasesize);
	mutex_unlock(&d->cache_mutex);
	memset(d->oob_data, 0xFF, mtd->oobsize);

	if(!suspend_counts) {
	for (i = 1; i < d->pblocks; i++)
		eba_tbl[i] = -1;
	}
	mtdswap_scan_eblks(d, 0);
	eb = d->eb_data + eba_tbl[0];
	spin_lock(&d->mtdswap_lock);
	rb_erase(&eb->u.rb, &d->used);
	spin_unlock(&d->mtdswap_lock);
	suspend_counts = 1;
#if 0
	for (i = 0; i < d->pblocks; i++) {
		if (eba_tbl[i] != -1) {
			eb = d->eb_data + eba_tbl[i];
			rb_erase(&eb->u.rb, &d->used);
		}
	}
#endif
	if (d->usedblock)
		d->mean_count = d->mean_count / d->usedblock;
	return 0;
}

static int mtdswap_resume(struct mtdswap_dev *d)
{
	mtdswap_check_resume(d);
	return 0;
}

static int mtdswap_suspend(struct mtdswap_dev *d)
{
	mtdswap_check_suspend(d);
	return 0;
}

static int swap_power_event(struct notifier_block *this,
			    unsigned long event, void *ptr)
{
	struct mtdswap_dev *d =
	    container_of(this, struct mtdswap_dev, pm_notifier);
	switch (event) {
	case PM_POST_RESTORE:	/* in case hibernation restore fail */
	case PM_POST_HIBERNATION:	/* normal case for hibernation finished */
		mtdswap_resume(d);
		break;
	case PM_HIBERNATION_PREPARE:
		mtdswap_suspend(d);
		break;
	case PM_HIBERNATION_FINISH:
		mutex_lock(&d->cache_mutex);
		write_cached_data(d);
		mutex_unlock(&d->cache_mutex);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int mtdswap_init(struct mtdswap_dev *d, unsigned int eblocks)
{
	struct mtd_info *mtd = d->mbd->mtd;
	struct mtdswap_eb *eb;
	int i;

	d->mtd = mtd;
	d->pblocks = eblocks;
	d->pm_notifier.notifier_call = swap_power_event;
	register_pm_notifier(&d->pm_notifier);

	d->page_per_block = mtd->erasesize / mtd->writesize;
	d->sector_per_block = mtd->erasesize >> MTDSWAP_SECTOR_SHIFT;
	d->mean_count = 1;
	d->used = d->free = RB_ROOT;
	spin_lock_init(&d->mtdswap_lock);
	mutex_init(&d->cache_mutex);

	d->badblock = d->freeblock = d->usedblock = 0;

	d->cache_data = vmalloc(mtd->erasesize);
	d->cache_size = mtd->erasesize;
	d->cache_state = STATE_EMPTY;
	d->cache_offset = -1;
	d->oob_data = vmalloc(mtd->oobsize);
	d->eb_data = vmalloc(sizeof(struct mtdswap_eb) * d->pblocks);

	memset(d->eb_data, 0x00, sizeof(struct mtdswap_eb) * d->pblocks);
	memset(d->cache_data, 0xFF, mtd->erasesize);
	memset(d->oob_data, 0xFF, mtd->oobsize);

	for (i = 0; i < d->pblocks; i++)
		eba_tbl[i] = -1;

	mtdswap_scan_eblks(d, 1);

	for (i = 0; i < d->pblocks; i++) {
		if (eba_tbl[i] != -1) {
			eb = d->eb_data + eba_tbl[i];
			rb_erase(&eb->u.rb, &d->used);
		}
	}
#if 0
	for (i = 0; i < d->pblocks; i++)
		printk("\n lnum%d pnum%d", i, eba_tbl[i]);
#endif
	if (d->usedblock)
		d->mean_count = d->mean_count / d->usedblock;
	/* test_swap(d); */

	return 0;
}

static int mtdswap_find_mtd(unsigned char *target, unsigned char *source)
{
	/*extract partition number from string */
	unsigned char *temp;
	unsigned int slen = strlen(source);
	unsigned int tlen=0;

	temp = strstr(target, source);

	if (temp) {
		tlen = strlen(temp);
		strncpy(partitions, temp + slen, tlen-slen+1);
		/*find mtd = true*/
		return 1;	
	}

	/*find mtd = false*/
	return 0;	
}


static void mtdswap_add_mtd(struct mtd_blktrans_ops *tr, struct mtd_info *mtd)
{
	struct mtdswap_dev *d;
	struct mtd_blktrans_dev *mbd_dev;
	struct nand_ecclayout *info;
	unsigned long use_size;
	int eblocks;

	if (memcmp(mtd->name, "swap", sizeof("swap"))!=0)
		return;
	if (mtd->index != part){
		printk(KERN_WARNING"\n Find swap partition mtdswap%d != mtdswap%lu\n", mtd->index, part);
		/*replace original resume_file with what we actaully find.*/
		memset(resume_file, 0, sizeof(resume_file));
		strncat(resume_file, "/dev/mtdswap", sizeof("/dev/mtdswap"));
		snprintf(partitions, sizeof(partitions), "%d", mtd->index);
		strncat(resume_file, partitions, sizeof(partitions));
		printk(KERN_WARNING"Replace resume_file As : %s\n", resume_file);
	}

	printk(KERN_INFO "Enabling MTD swap on device %d, size %lldMB, ",
	       mtd->index, mtd->size / 1024 / 1024);

	info = mtd->ecclayout;

	use_size = mtd->size;
	eblocks = mtd_div_by_eb(use_size, mtd);

	d = kzalloc(sizeof(struct mtdswap_dev), GFP_KERNEL);

	if (!d)
		return;
	mbd_dev = kzalloc(sizeof(struct mtd_blktrans_dev), GFP_KERNEL);
	if (!mbd_dev) {
		kfree(d);
		return;
	}

	d->mbd = mbd_dev;
	mbd_dev->priv = d;

	mbd_dev->mtd = mtd;
	mbd_dev->devnum = mtd->index;
	mbd_dev->size = use_size >> 9;
	mbd_dev->tr = tr;

	if (!(mtd->flags & MTD_WRITEABLE))
		mbd_dev->readonly = 1;

	if (mtdswap_init(d, eblocks) < 0)
		goto init_failed;
	if (add_mtd_blktrans_dev(mbd_dev) < 0)
		goto cleanup;
	d->dev = disk_to_dev(mbd_dev->disk);
	return;

cleanup:
	mtdswap_cleanup(d);

init_failed:
	kfree(mbd_dev);
	kfree(d);
}

static int mtdswap_open(struct mtd_blktrans_dev *dev)
{
	return 0;
}

static int mtdswap_release(struct mtd_blktrans_dev *dev)
{
	struct mtdswap_dev *d = MTDSWAP_MBD_TO_MTDSWAP(dev);
	mutex_lock(&d->cache_mutex);
	write_cached_data(d);
	mutex_unlock(&d->cache_mutex);
	return 0;
}

static struct mtd_blktrans_ops mtdswap_ops = {
	.name = "mtdswap",
	.major = 0,
	.part_bits = 0,
	.blksize = 512,
	.open = mtdswap_open,
	.flush = mtdswap_flush,
	.release = mtdswap_release,
	.readsect = mtdswap_readsect,
	.writesect = mtdswap_writesect,
	.add_mtd = mtdswap_add_mtd,
	.remove_dev = mtdswap_remove_dev,
	.owner = THIS_MODULE,
};

static int __init mtdswap_modinit(void)
{
	/* find if resume_file name contains "mtdswap" */
	int ret = mtdswap_find_mtd(resume_file, "mtdswap");
	if (!ret){
		printk(KERN_WARNING"\n[mtdswap] Resume Partition Is Not mtdswap !!!\n");
		return 0;
	}
	parts = &partitions[0];
	printk(KERN_WARNING"[mtdswap] resume_file:%s, parts=%s\n", resume_file, parts);
	if(kstrtoul(parts, 0, &part) < 0){
		printk(KERN_WARNING"[mtdswap] Invalid MTDSWAP Partition Number!!!\n");
	}
	return register_mtd_blktrans(&mtdswap_ops);
}

static void __exit mtdswap_modexit(void)
{
	deregister_mtd_blktrans(&mtdswap_ops);
}

module_init(mtdswap_modinit);
module_exit(mtdswap_modexit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johnny Liu <johnnyliu@viatech.com.cn>");
MODULE_DESCRIPTION("Block device access to an MTD suitable for using as "
		   "swap space");
