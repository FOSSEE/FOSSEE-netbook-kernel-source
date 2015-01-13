/*++
 * linux/drivers/video/wmt/wmt-vpp.c
 * WonderMedia video post processor (VPP) driver
 *
 * Copyright c 2014  WonderMedia  Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 4F, 533, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C
--*/

#define DEV_VPP_C

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/dma-mapping.h>
#include <linux/major.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <linux/sched.h>
#include <linux/wmt-mb.h>
#include <linux/wmt-se.h>
#include <linux/poll.h>

#undef DEBUG
/* #define DEBUG */
/* #define DEBUG_DETAIL */
#include "vpp.h"

/*----------------------- PRIVATE MACRO --------------------------------------*/

/*----------------------- PRIVATE CONSTANTS ----------------------------------*/
/* #define WMT_VPP_XXXX    1     *//*Example*/
#define DEVICE_NAME "wmt-vpp"

/*----------------------- PRIVATE TYPE  --------------------------------------*/
/* typedef  xxxx wmt_vpp_xxx_t; *//*Example*/

/*----------------------- INTERNAL PRIVATE VARIABLES - -----------------------*/
/* int  wmt_vpp_xxx;        *//*Example*/
static DEFINE_SEMAPHORE(wmt_vpp_sem);
static struct class *wmt_vpp_class;
static int wmt_vpp_major;

/*--------------------- INTERNAL PRIVATE FUNCTIONS ---------------------------*/
/* void wmt_vpp_xxx(void); *//*Example*/

/*----------------------- Function Body --------------------------------------*/
#ifdef CONFIG_PROC_FS
#define CONFIG_VPP_PROC
#ifdef CONFIG_VPP_PROC
unsigned int vpp_proc_value;
char vpp_proc_str[16];
static ctl_table vpp_table[];
static int vpp_do_proc(ctl_table *ctl, int write,
				void *buffer, size_t *len, loff_t *ppos)
{
	int ret;
	int ctl_name;

	ctl_name = (((int)ctl - (int)vpp_table) / sizeof(ctl_table)) + 1;
	if (!write) {
		switch (ctl_name) {
		case 1:
			vpp_proc_value = g_vpp.dbg_msg_level;
			break;
		case 2:
			vpp_proc_value = g_vpp.dbg_wait;
			break;
		case 3:
			vpp_proc_value = g_vpp.dbg_flag;
			break;
		case 8:
		case 9:
			vpp_proc_value = vpp_get_base_clock((ctl_name == 8) ?
				VPP_MOD_GOVRH : VPP_MOD_GOVRH2);
			break;
		case 10:
			vpp_proc_value = p_scl->scale_mode;
			break;
		case 11:
			vpp_proc_value = p_scl->filter_mode;
			break;
#ifdef CONFIG_WMT_HDMI
		case 12:
			vpp_proc_value = g_vpp.hdmi_cp_enable;
			break;
		case 13:
			vpp_proc_value = g_vpp.hdmi_3d_type;
			break;
		case 14:
			vpp_proc_value = g_vpp.hdmi_certify_flag;
			break;
#endif
		case 15:
			vpp_proc_value = govrh_get_brightness(p_govrh);
			break;
		case 16:
			vpp_proc_value = govrh_get_contrast(p_govrh);
			break;
		case 17:
			vpp_proc_value = govrh_get_saturation(p_govrh);
			break;
		case 18:
			vpp_proc_value = g_vpp.fb_manual;
			break;
		default:
			break;
		}
	}

	ret = proc_dointvec(ctl, write, buffer, len, ppos);
	if (write) {
		switch (ctl_name) {
		case 1:
			DPRINT("---------- VPP debug level ----------\n");
			DPRINT("0-disable,255-show all\n");
			DPRINT("1-scale,2-disp fb,3-interrupt,4-timer\n");
			DPRINT("5-ioctl,6-diag,7-stream\n");
			DPRINT("-------------------------------------\n");
			g_vpp.dbg_msg_level = vpp_proc_value;
			break;
		case 2:
			g_vpp.dbg_wait = vpp_proc_value;
			vpp_dbg_wake_up();
			break;
		case 3:
			g_vpp.dbg_flag = vpp_proc_value;
			break;
#ifdef CONFIG_WMT_EDID
		case 6:
			{
			struct vout_t *vo;

			vo = vout_get_entry_adapter(vpp_proc_value);
			if ((vo->inf) && (vo->inf->get_edid)) {
				vo->status &= ~VPP_VOUT_STS_EDID;
				if (vout_get_edid(vo->num)) {
					int i;

					vo->edid_info.option = 0;
					edid_dump(vo->edid);
					for (i = 1; i <= vo->edid[126]; i++)
						edid_dump(vo->edid + 128 * i);
					if (!edid_parse(vo->edid,
						&vo->edid_info))
						DBG_ERR("parse EDID\n");
				} else {
					DBG_ERR("read EDID\n");
				}
			}
			}
			break;
#endif
		case 8:
		case 9:
			govrh_set_clock((ctl_name == 8) ? p_govrh : p_govrh2,
				vpp_proc_value);
			DPRINT("set govr pixclk %d\n", vpp_proc_value);
			break;
		case 10:
			DPRINT("---------- scale mode ----------\n");
			DPRINT("0-recursive normal\n");
			DPRINT("1-recursive sw bilinear\n");
			DPRINT("2-recursive hw bilinear\n");
			DPRINT("3-realtime noraml (quality but x/32 limit)\n");
			DPRINT("4-realtime bilinear (fit edge w skip line)\n");
			DPRINT("-------------------------------------\n");
			p_scl->scale_mode = vpp_proc_value;
			break;
		case 11:
			p_scl->filter_mode = vpp_proc_value;
			break;
#ifdef CONFIG_WMT_HDMI
		case 12:
			g_vpp.hdmi_cp_enable = vpp_proc_value;
			hdmi_set_cp_enable(vpp_proc_value);
			break;
		case 13:
			g_vpp.hdmi_3d_type = vpp_proc_value;
			hdmi_tx_vendor_specific_infoframe_packet();
			break;
		case 14:
			g_vpp.hdmi_certify_flag = vpp_proc_value;
			break;
#endif
		case 15:
			govrh_set_brightness(p_govrh, vpp_proc_value);
			break;
		case 16:
			govrh_set_contrast(p_govrh, vpp_proc_value);
			break;
		case 17:
			govrh_set_saturation(p_govrh, vpp_proc_value);
			break;
		case 18:
			g_vpp.fb_manual = vpp_proc_value;
			break;
		default:
			break;
		}
	}
	return ret;
}

struct proc_dir_entry *vpp_proc_dir;
static ctl_table vpp_table[] = {
	{ /* .ctl_name = 1, */
	.procname	= "dbg_msg",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 2, */
	.procname	= "dbg_wait",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 3, */
	.procname	= "dbg_flag",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 4, */
	.procname	= "edid_disable",
	.data		= &edid_disable,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 5, */
	.procname	= "edid_msg",
	.data		= &edid_msg_enable,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 6, */
	.procname	= "vout_edid",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 7, */
	.procname	= "vo_mode",
	.data		= vpp_proc_str,
	.maxlen		= 12,
	.mode		= 0666,
	.proc_handler = &proc_dostring,
	},
	{ /* .ctl_name = 8, */
	.procname	= "govr1_pixclk",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 9, */
	.procname	= "govr2_pixclk",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 10, */
	.procname	= "scl_scale_mode",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 11, */
	.procname	= "scl_filter",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 12 */
	.procname	= "hdmi_cp_enable",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 13 */
	.procname	= "hdmi_3d",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 14 */
	.procname	= "hdmi_certify",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 15 */
	.procname	= "brightness",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 16 */
	.procname	= "contrast",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 17 */
	.procname	= "saturation",
	.data		= &vpp_proc_value,
	.maxlen		= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* .ctl_name = 18 */
	.procname	= "fb_manual",
	.data		= &vpp_proc_value,
	.maxlen 	= sizeof(int),
	.mode		= 0666,
	.proc_handler = &vpp_do_proc,
	},
	{ /* end of table */
	}
};

static ctl_table vpp_root_table[] = {
	{
	.procname	= "vpp", /* create path ==> /proc/sys/vpp */
	.mode		= 0555,
	.child		= vpp_table
	},
	{ /* end of table */
	}
};
static struct ctl_table_header *vpp_table_header;
#endif

static int vpp_sts_read_proc(char *buf, char **start, off_t offset,
					int len, int *eof, void *data)
{
	struct govrh_regs *regs = (struct govrh_regs *) REG_GOVRH_BASE1_BEGIN;
	unsigned int yaddr, caddr;
	char *p = buf;
	unsigned int reg;

	p += sprintf(p, "--- VPP HW status ---\n");
#ifdef WMT_FTBLK_GOVRH
	p += sprintf(p, "GOVRH memory read underrun error %d,cnt %d,cnt2 %d\n",
		(regs->interrupt.val & 0x200) ? 1 : 0,
		p_govrh->underrun_cnt, p_govrh2->underrun_cnt);
	p_govrh->clr_sts(VPP_INT_ALL);
#endif

#ifdef WMT_FTBLK_SCL
	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "SCL TG error %d\n", scl_regs1->tg_sts.b.tgerr);
	p += sprintf(p, "SCLR MIF1 read error %d\n",
		scl_regs1->r_fifo_ctl.b.r1_mif_err);
	p += sprintf(p, "SCLR MIF2 read error %d\n",
		scl_regs1->r_fifo_ctl.b.r2_mif_err);
	p += sprintf(p, "SCLW RGB fifo overflow %d\n",
		scl_regs1->w_ff_ctl.b.mif_rgb_err);
	p += sprintf(p, "SCLW Y fifo overflow %d\n",
		scl_regs1->w_ff_ctl.b.mif_y_err);
	p += sprintf(p, "SCLW C fifo overflow %d\n",
		scl_regs1->w_ff_ctl.b.mif_c_err);
	p_scl->clr_sts(VPP_INT_ALL);
#endif

	p += sprintf(p, "---------------------------------------\n");
	p += sprintf(p, "(880.0)GOVRH Enable %d,(900.0)TG %d\n",
		regs->mif.b.enable, regs->tg_enable.b.enable);

	reg = inl(PM_CTRL_BASE_ADDR + 0x258);
	p += sprintf(p, "--- POWER CONTROL ---\n");
	p += sprintf(p, "0x%x = 0x%x\n", PM_CTRL_BASE_ADDR + 0x258, reg);
	p += sprintf(p, "HDCP %d,VPP %d,SCL %d,HDMI I2C %d\n",
		(reg & BIT7) ? 1 : 0, (reg & BIT18) ? 1 : 0,
		(reg & BIT21) ? 1 : 0, (reg & BIT22) ? 1 : 0);
	p += sprintf(p, "HDMI %d,GOVR %d,NA12 %d\n",
		(reg & BIT23) ? 1 : 0, (reg & BIT25) ? 1 : 0,
		(reg & BIT16) ? 1 : 0);
	p += sprintf(p, "DVO %d,HDMI OUT %d,LVDS %d\n", (reg & BIT29) ? 1 : 0,
		(reg & BIT30) ? 1 : 0, (reg & BIT14) ? 1 : 0);

	p += sprintf(p, "--- VPP fb Address ---\n");

#ifdef WMT_FTBLK_GOVRH
	govrh_get_fb_addr(p_govrh, &yaddr, &caddr);
	p += sprintf(p, "GOVRH fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",
		REG_GOVRH_YSA, yaddr, REG_GOVRH_CSA, caddr);
	govrh_get_fb_addr(p_govrh2, &yaddr, &caddr);
	p += sprintf(p, "GOVRH2 fb addr Y(0x%x) 0x%x, C(0x%x) 0x%x\n",
		REG_GOVRH2_YSA, yaddr, REG_GOVRH2_CSA, caddr);
#endif
	p_govrh->underrun_cnt = 0;
	p_govrh2->underrun_cnt = 0;
	return p - buf;
}

static int vpp_reg_read_proc(char *buf, char **start, off_t offset,
					int len, int *eof, void *data)
{
	char *p = buf;
	struct vpp_mod_base_t *mod_p;
	int i;

	DPRINT("Product ID:0x%x\n", vpp_get_chipid());
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->dump_reg)
			mod_p->dump_reg();
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_reg_dump();
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_reg_dump();
#endif
	return p - buf;
}
#endif

irqreturn_t vpp_interrupt_routine(int irq, void *dev_id)
{
	enum vpp_int_t int_sts;

	switch (irq) {
	case VPP_IRQ_VPPM: /* VPP */
		int_sts = p_vppm->get_sts();
		p_vppm->clr_sts(int_sts);

		vpp_dbg_show_val1(VPP_DBGLVL_INT, 0, "[VPP] VPPM INT", int_sts);
		{
		int i;
		unsigned int mask;
		struct vpp_irqproc_t *irqproc;

		for (i = 0, mask = 0x1; (i < 32) && int_sts; i++, mask <<= 1) {
			if ((int_sts & mask) == 0)
				continue;

			irqproc = vpp_irqproc_get_entry(mask);
			if (irqproc) {
				if (list_empty(&irqproc->list) == 0)
					tasklet_schedule(&irqproc->tasklet);
			} else {
				irqproc = vpp_irqproc_get_entry(VPP_INT_MAX);
				if (list_empty(&irqproc->list) == 0) {
					struct vpp_proc_t *entry;
					struct list_head *ptr;

					ptr = (&irqproc->list)->next;
					entry = list_entry(ptr,
						struct vpp_proc_t, list);
					if (entry->type == mask)
						tasklet_schedule(
							&irqproc->tasklet);
				}
			}
			int_sts &= ~mask;
		}
		}
		break;
#ifdef WMT_FTBLK_SCL
	case VPP_IRQ_SCL: /* SCL */
		int_sts = p_scl->get_sts();
		p_scl->clr_sts(int_sts);
		vpp_dbg_show_val1(VPP_DBGLVL_INT, 0, "[VPP] SCL INT", int_sts);
		break;
#endif
#ifdef WMT_FTBLK_GOVRH
	case VPP_IRQ_GOVR:	/* GOVR */
	case VPP_IRQ_GOVR2:
		{
		struct govrh_mod_t *govr;

		govr = (irq == VPP_IRQ_GOVR) ? p_govrh : p_govrh2;
		int_sts = govr->get_sts();
		govr->clr_sts(int_sts);
		vpp_dbg_show_val1(VPP_DBGLVL_INT, 0, "[VPP] GOVR INT", int_sts);
		govr->underrun_cnt++;
#ifdef VPP_DBG_DIAG_NUM
		vpp_dbg_show(VPP_DBGLVL_DIAG, 3, "GOVR MIF Err");
		vpp_dbg_diag_delay = 10;
#endif
		}
		break;
#endif
	default:
		DPRINT("*E* invalid vpp isr\n");
		break;
	}
	return IRQ_HANDLED;
}

void vpp_wait_vsync(int no, int cnt)
{
	struct govrh_mod_t *govr;

	govr = vout_info_get_govr(no);
	if (govr) {
		if (govrh_get_MIF_enable(govr)) {
			vpp_irqproc_work((govr->mod == VPP_MOD_GOVRH) ?
				VPP_INT_GOVRH_VBIS : VPP_INT_GOVRH2_VBIS,
				0, 0, 100 * cnt, cnt);
			if (vpp_check_dbg_level(VPP_DBGLVL_DISPFB))
				MSG("wait vsync(%d,%d)\n", no, cnt);
		}
	}
}

/*----------------------- vpp ioctl --------------------------------------*/
int vpp_common_ioctl(unsigned int cmd, unsigned long arg)
{
	struct vpp_mod_base_t *mod_p;
	struct vpp_fb_base_t *mod_fb_p;
	int retval = 0;

	switch (cmd) {
	case VPPIO_VPPGET_INFO:
		{
		int i;
		vpp_cap_t parm;

		parm.chip_id = vpp_get_chipid();
		parm.version = 0x01;
		parm.resx_max = VPP_HD_MAX_RESX;
		parm.resy_max = VPP_HD_MAX_RESY;
		parm.pixel_clk = 400000000;
		parm.module = 0x0;
		for (i = 0; i < VPP_MOD_MAX; i++) {
			mod_p = vpp_mod_get_base(i);
			if (mod_p)
				parm.module |= (0x01 << i);
		}
		parm.option = VPP_CAP_DUAL_DISPLAY;
		copy_to_user((void *)arg, (void *) &parm, sizeof(vpp_cap_t));
		}
		break;
	case VPPIO_VPPSET_INFO:
		{
		vpp_cap_t parm;

		copy_from_user((void *)&parm, (const void *)arg,
			sizeof(vpp_cap_t));
		}
		break;
	case VPPIO_I2CSET_BYTE:
		{
		vpp_i2c_t parm;
		unsigned int id;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_i2c_t));
		id = (parm.addr & 0x0000FF00) >> 8;
		vpp_i2c_write(id, (parm.addr & 0xFF), parm.index,
			(char *)&parm.val, 1);
		}
		break;
	case VPPIO_I2CGET_BYTE:
		{
		vpp_i2c_t parm;
		unsigned int id;
		int len;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_i2c_t));
		id = (parm.addr & 0x0000FF00) >> 8;
		len = parm.val;
		{
			unsigned char buf[len];

			vpp_i2c_read(id, (parm.addr & 0xFF), parm.index,
				buf, len);
			parm.val = buf[0];
		}
		copy_to_user((void *)arg, (void *) &parm, sizeof(vpp_i2c_t));
		}
		break;
	case VPPIO_MODULE_FBINFO:
		{
		vpp_mod_fbinfo_t parm;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_mod_fbinfo_t));

		if (g_vpp.virtual_display)
			parm.mod = (hdmi_get_plugin()) ?
				VPP_MOD_GOVRH : VPP_MOD_GOVRH2;
		mod_fb_p = vpp_mod_get_fb_base(parm.mod);
		if (!mod_fb_p)
			break;
		mod_p = vpp_mod_get_base(parm.mod);
		if (parm.read) {
			parm.fb = mod_fb_p->fb;
			switch (parm.mod) {
			case VPP_MOD_GOVRH:
			case VPP_MOD_GOVRH2:
				govrh_get_framebuffer(
					(struct govrh_mod_t *)mod_p,
					&parm.fb);
				break;
			default:
				break;
			}
			copy_to_user((void *)arg, (void *) &parm,
				sizeof(vpp_mod_fbinfo_t));
		} else {
			mod_fb_p->fb = parm.fb;
			mod_fb_p->set_framebuf(&parm.fb);
		}
		}
		break;
#ifdef CONFIG_VPP_STREAM_CAPTURE
	case VPPIO_STREAM_ENABLE:
		g_vpp.stream_enable = arg;
		g_vpp.stream_mb_sync_flag = 0;
		g_vpp.stream_mb_lock = 0;
		g_vpp.stream_mb_index = 0xFF;
		MSG("VPPIO_STREAM_ENABLE %d\n", g_vpp.stream_enable);
		vpp_netlink_notify(WP_PID, DEVICE_STREAM,
				(g_vpp.stream_enable) ? 1 : 0);
		wmt_enable_mmfreq(WMT_MMFREQ_MIRACAST, g_vpp.stream_enable);
		break;
	case VPPIO_STREAM_GETFB:
		{
		vdo_framebuf_t fb;

		vpp_lock();
		if (g_vpp.stream_mb_index == 0xFF) { /* not avail */
			retval = -1;
			vpp_unlock();
			break;
		}
		fb = vout_info[g_vpp.stream_fb]->fb;
		fb.fb_h = vpp_calc_align(fb.img_h, 16);
		copy_to_user((void *)arg, (void *) &fb, sizeof(vdo_framebuf_t));
		retval = vpp_mb_get(fb.y_addr);
		vpp_unlock();
		}
		break;
	case VPPIO_STREAM_PUTFB:
		{
		vdo_framebuf_t fb;
		copy_from_user((void *) &fb, (const void *)arg,
			sizeof(vdo_framebuf_t));
		vpp_lock();
		vpp_mb_put(fb.y_addr);
		vpp_unlock();
		}
		break;
#endif
	case VPPIO_MULTIVD_ENABLE:
		wmt_enable_mmfreq(WMT_MMFREQ_MULTI_VD, arg);
		break;
	case VPPIO_MODSET_CSC:
		{
			struct vpp_mod_parm_t parm;
			copy_from_user((void *) &parm, (const void *)arg,
				sizeof(struct vpp_mod_parm_t));
			switch (parm.mod) {
			case VPP_MOD_GOVRH:
				p_govrh->csc_mode_force = parm.parm;
				break;
			case VPP_MOD_GOVRH2:
				if (p_govrh2->csc_mode_force != parm.parm)
					MSG("GOVRH2 CSC %d-->%d\n",
					p_govrh2->csc_mode_force, parm.parm);
				p_govrh2->csc_mode_force = parm.parm;
				break;
			default:
				break;
			}
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
	return retval;
}

int vpp_vout_ioctl(unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	switch (cmd) {
	case VPPIO_VOGET_INFO:
		{
		vpp_vout_info_t parm;
		struct vout_info_t *info;
		int fb_no, i, idx;
		unsigned int status;

		memset(&parm, 0, sizeof(vpp_vout_info_t));
		for (fb_no = 0, idx = 0; ; fb_no++) {
			info = vout_info_get_entry(fb_no);
			if (!info)
				break;

			for (i = 0; i < VPP_VOUT_NUM; i++, idx++) {
				if (info->vout[i] == 0)
					break;
				parm.mode[idx] = vout_get_mode_adapter(
					info->vout[i]);
				status = info->vout[i]->status |
					(fb_no << 8);
				DBGMSG("GET_INFO(fb %d,mode %d,sts 0x%x)",
					fb_no, parm.mode[idx], status);
				DBGMSG("plug %d\n",
					(status & VPP_VOUT_STS_PLUGIN) ? 1 : 0);
				parm.status[idx] = status;
			}
		}
		copy_to_user((void *)arg, (const void *) &parm,
			sizeof(vpp_vout_info_t));
		}
		break;
	case VPPIO_VOUT_VMODE:
		{
		vpp_vout_vmode_t parm;
		int i;
		struct fb_videomode *vmode;
		unsigned int resx, resy, fps;
		unsigned int pre_resx, pre_resy, pre_fps;
		int index, from_index;
		int support;
		unsigned int option, pre_option;
#ifdef CONFIG_WMT_EDID
		struct edid_info_t *edid_info;
#endif
		struct fb_videomode *edid_vmode;
		vpp_vout_t mode;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_vout_vmode_t));
		from_index = parm.num;
		parm.num = 0;
		mode = parm.mode & 0xF;
#ifdef CONFIG_VPP_DEMO
		parm.parm[parm.num].resx = 1920;
		parm.parm[parm.num].resy = 1080;
		parm.parm[parm.num].fps = 60;
		parm.parm[parm.num].option = 0;
		parm.num++;
#else
#ifdef CONFIG_WMT_EDID
		{
		struct vout_t *vo;

		vo = vout_get_entry_adapter(mode);
		if (!vo)
			goto vout_vmode_end;

		if (!(vo->status & VPP_VOUT_STS_PLUGIN)) {
			DPRINT("*W* not plugin\n");
			goto vout_vmode_end;
		}

		if (vout_get_edid(vo->num) == 0) {
			DPRINT("*W* read EDID fail\n");
			goto vout_vmode_end;
		}
		if (edid_parse(vo->edid, &vo->edid_info) == 0) {
			DPRINT("*W* parse EDID fail\n");
			goto vout_vmode_end;
		}
		edid_info = &vo->edid_info;
		}
#endif
		index = 0;
		resx = resy = fps = option = 0;
		pre_resx = pre_resy = pre_fps = pre_option = 0;
		for (i = 0; ; i++) {
            int ret = 0;
			vmode = (struct fb_videomode *) &vpp_videomode[i];
			if (vmode->pixclock == 0)
				break;
			resx = vmode->xres;
			resy = vmode->yres;
			fps = vmode->refresh;
			option = fps & EDID_TMR_FREQ;
			option |= (vmode->vmode & FB_VMODE_INTERLACED) ?
				EDID_TMR_INTERLACE : 0;
			if ((pre_resx == resx) && (pre_resy == resy) &&
				(pre_fps == fps) && (pre_option == option))
				continue;
			pre_resx = resx;
			pre_resy = resy;
			pre_fps = fps;
			pre_option = option;
			support = 0;
#ifdef CONFIG_WMT_EDID
			if (ret = edid_find_support(edid_info, resx, resy,
				option, &edid_vmode))
#else
			if (1)
#endif
				support = 1;

			if (support) {
				if (index >= from_index) {
					parm.parm[parm.num].resx = resx;
					parm.parm[parm.num].resy = resy;
					parm.parm[parm.num].fps = fps;
					parm.parm[parm.num].option =
						vmode->vmode;
#ifdef CONFIG_WMT_EDID
                    if(ret == 3)
                        parm.parm[parm.num].option |= 0x80;
#endif
					parm.num++;
				}
				index++;
				if (parm.num >= VPP_VOUT_VMODE_NUM)
					break;
			}
		}
#ifdef CONFIG_WMT_EDID
vout_vmode_end:
#endif
#endif
#if 0
		if (parm.num == 0) { /* if no EDID*/
			enum vout_tvformat_t tvformat = vout_get_tvformat();
			if (g_vpp.virtual_display ||
				(g_vpp.dual_display == 0)) {
				if (mode == VPP_VOUT_DVI) {
					switch (tvformat) {
					case TV_PAL:
						parm.parm[0].resx = 720;
						parm.parm[0].resy = 576;
						parm.parm[0].fps = 50;
						parm.num = 1;
						parm.parm[0].option = 0;
						break;
					case TV_NTSC:
						parm.parm[0].resx = 720;
						parm.parm[0].resy = 480;
						parm.parm[0].fps = 60;
						parm.num = 1;
						parm.parm[0].option = 0;
						break;
					default:
						break;
					}
				}
			}
		}
#endif
		DBG_MSG("[VPP] get support vmode %d\n", parm.num);
		copy_to_user((void *)arg, (const void *) &parm,
			sizeof(vpp_vout_vmode_t));
		}
		break;
	case VPPIO_VOGET_EDID:
		{
		vpp_vout_edid_t parm;
		char *edid;
		struct vout_t *vo;
		int size;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_vout_edid_t));
		size = 0;
#ifdef CONFIG_WMT_EDID
		vo = vout_get_entry_adapter(parm.mode);
		if (!vo)
			goto vout_edid_end;

		if (!(vo->status & VPP_VOUT_STS_PLUGIN)) {
			DBG_ERR("*W* not plugin\n");
			goto vout_edid_end;
		}

		edid = vout_get_edid(vo->num);
		if (edid == 0) {
			DBG_ERR("*W* read EDID fail\n");
			goto vout_edid_end;
		}
		size = (edid[0x7E] + 1) * 128;
		if (size > parm.size)
			size = parm.size;
		copy_to_user((void *) parm.buf, (void *) edid, size);
vout_edid_end:
#endif
		parm.size = size;
		copy_to_user((void *)arg, (const void *) &parm,
			sizeof(vpp_vout_edid_t));
		}
		break;
	case VPPIO_VOGET_CP_INFO:
		{
		vpp_vout_cp_info_t parm;
		int num;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_vout_cp_info_t));
		num = parm.num;
		if (num >= VOUT_MODE_MAX) {
			retval = -ENOTTY;
			break;
		}
		memset(&parm, 0, sizeof(vpp_vout_cp_info_t));
		switch (num) {
		case VOUT_HDMI:
			if (!g_vpp.hdmi_certify_flag) {
				if (g_vpp.hdmi_bksv[0] == 0) {
					hdmi_get_bksv(&g_vpp.hdmi_bksv[0]);
					DBG_MSG("get BKSV 0x%x 0x%x\n",
						g_vpp.hdmi_bksv[0],
						g_vpp.hdmi_bksv[1]);
				}
			}
		case VOUT_DVO2HDMI:
			parm.bksv[0] = g_vpp.hdmi_bksv[0];
			parm.bksv[1] = g_vpp.hdmi_bksv[1];
			break;
		default:
			parm.bksv[0] = parm.bksv[1] = 0;
			break;
		}
		copy_to_user((void *)arg, (const void *) &parm,
			sizeof(vpp_vout_cp_info_t));
		}
		break;
	case VPPIO_VOSET_CP_KEY:
		if (g_vpp.hdmi_cp_p == 0) {
			g_vpp.hdmi_cp_p = kmalloc(sizeof(vpp_vout_cp_key_t),
				GFP_KERNEL);
		}
		if (g_vpp.hdmi_cp_p) {
			copy_from_user((void *) g_vpp.hdmi_cp_p,
				(const void *)arg, sizeof(vpp_vout_cp_key_t));
			if (hdmi_cp)
				hdmi_cp->init();
		}
		break;
#ifdef WMT_FTBLK_HDMI
	case VPPIO_VOSET_AUDIO_PASSTHRU:
		hdmi_regs1->aud_mode.b.sub_packet = (arg) ? 0xF : 0x0;
		break;
#endif
#ifdef CONFIG_VPP_VIRTUAL_DISPLAY
	case VPPIO_VOSET_VIRTUAL_FBDEV:
		{
		struct vout_info_t *info;
		int i;

		g_vpp.fb0_bitblit = (arg) ? 0 : 1;
		for (i = 1; ; i++) {
			info = vout_info_get_entry(i);
			if (!info)
				break;
			if (info->alloc_mode != VOUT_ALLOC_GE_OVERSCAN)
				continue;
			if (info->mb == 0)
				continue;
			MSG("fb%d mb free 0x%x\n", i, info->mb);
			mb_free(info->mb);
			info->mb = 0;
		}
		MSG("[VPP] virtual display %d\n", (int)arg);
		}
		break;
#endif
	case VPPIO_VOGET_HDMI_3D:
		{
		struct vpp_vout_parm_s parm;
		struct vout_t *vo;

		vo = vout_get_entry(VPP_VOUT_NUM_HDMI);
		parm.arg = edid_get_hdmi_3d_mask(&vo->edid_info, hdmi_info.vic);
		copy_to_user((void *)arg, (const void *) &parm,
			sizeof(struct vpp_vout_parm_s));
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
	return retval;
}

int vpp_scl_ioctl(unsigned int cmd, unsigned long arg)
{
	int retval = 0;

	switch (cmd) {
	case VPPIO_SCL_SCALE_OVERLAP:
		{
		vpp_scale_overlap_t parm;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_scale_overlap_t));
		if (vpp_check_dbg_level(VPP_DBGLVL_FPS))
			vpp_dbg_timer(&p_scl->overlap_timer, 0, 1);

		p_scl->scale_sync = 1;
		retval = scl_set_scale_overlap(&parm.src_fb,
			&parm.src2_fb, &parm.dst_fb);

		if (vpp_check_dbg_level(VPP_DBGLVL_FPS))
			vpp_dbg_timer(&p_scl->overlap_timer, "overlap", 2);
		}
		break;
	case VPPIO_SCL_SCALE_ASYNC:
	case VPPIO_SCL_SCALE:
		{
		vpp_scale_t parm;

		p_scl->scale_sync = (cmd == VPPIO_SCL_SCALE) ? 1 : 0;
		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_scale_t));
		if (vpp_check_dbg_level(VPP_DBGLVL_FPS))
			vpp_dbg_timer(&p_scl->scale_timer, 0, 1);

		vpp_set_NA12_hiprio(1);
		retval = vpp_set_recursive_scale(&parm.src_fb, &parm.dst_fb);
		vpp_set_NA12_hiprio(0);

		if (vpp_check_dbg_level(VPP_DBGLVL_FPS))
			vpp_dbg_timer(&p_scl->scale_timer, "scale", 2);
		copy_to_user((void *)arg, (void *)&parm, sizeof(vpp_scale_t));
		}
		break;
	case VPPIO_SCL_SCALE_FINISH:
		retval = p_scl->scale_finish();
		break;
	case VPPIO_SCLSET_OVERLAP:
		{
		vpp_overlap_t parm;

		copy_from_user((void *) &parm, (const void *)arg,
			sizeof(vpp_overlap_t));
		vpp_mod_set_clock(VPP_MOD_SCL, VPP_FLAG_ENABLE, 0);
		scl_set_overlap(&parm);
		vpp_mod_set_clock(VPP_MOD_SCL, VPP_FLAG_DISABLE, 0);
		}
		break;
	default:
		retval = -ENOTTY;
		break;
	}
	return retval;
}

static int wmt_vpp_open(struct inode *inode, struct file *filp)
{
	DBG_MSG("\n");
	return 0;
}

static int wmt_vpp_release(struct inode *inode, struct file *filp)
{
	DBG_MSG("\n");
	return 0;
}

static long wmt_vpp_ioctl(struct file *filp, unsigned int cmd,
						unsigned long arg)
{
	int ret = -EINVAL;
	int skip_mutex = 0;

/*	DBG_MSG("0x%x,0x%x\n", cmd, (int)arg); */

	if (_IOC_TYPE(cmd) != VPPIO_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > VPPIO_MAX)
		return -ENOTTY;

	/* check argument area */
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE,
				(void __user *) arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ,
				(void __user *) arg, _IOC_SIZE(cmd));
	else
		ret = 0;

	if (ret)
		return -EFAULT;

	if (vpp_check_dbg_level(VPP_DBGLVL_IOCTL)) {
		switch (cmd) {
		default:
			DPRINT("[VPP] ioctl cmd 0x%x,arg 0x%x\n",
				_IOC_NR(cmd), (int)arg);
			break;
		}
	}

	switch (cmd) {
	case VPPIO_STREAM_GETFB: /* block mode should wait vsync */
		skip_mutex = 1;
		break;
	default:
		/* scale should wait complete */
		if ((_IOC_NR(cmd) >= VPPIO_SCL_BASE) &&
			(_IOC_NR(cmd) < VPPIO_MAX))
			skip_mutex = 1;
		break;
	}

	if (!skip_mutex)
		down(&wmt_vpp_sem);

	switch (_IOC_NR(cmd)) {
	case VPPIO_VPP_BASE ... (VPPIO_VOUT_BASE-1):
		/* DBGMSG("VPP command ioctl\n"); */
		ret = vpp_common_ioctl(cmd, arg);
		break;
	case VPPIO_VOUT_BASE ... (VPPIO_SCL_BASE - 1):
		ret = vpp_vout_ioctl(cmd, arg);
		break;
	case VPPIO_SCL_BASE ... (VPPIO_MAX-1):
		/* DBGMSG("SCL ioctl\n"); */
		ret = vpp_scl_ioctl(cmd, arg);
		break;
	default:
		DBG_ERR("*W* cmd 0x%x\n", cmd);
		break;
	}

	if (!skip_mutex)
		up(&wmt_vpp_sem);

	if (vpp_check_dbg_level(VPP_DBGLVL_IOCTL)) {
		switch (cmd) {
		default:
			DPRINT("[VPP] ioctl cmd 0x%x,ret 0x%x\n",
				_IOC_NR(cmd), (int)ret);
			break;
		}
	}
	return ret;
}

const struct file_operations wmt_vpp_fops = {
	.owner          = THIS_MODULE,
	.open           = wmt_vpp_open,
	.release        = wmt_vpp_release,
	.unlocked_ioctl = wmt_vpp_ioctl,
};

static int wmt_vpp_probe(struct platform_device *dev)
{
	DBG_MSG("\n");

	vpp_irqproc_init();
	vpp_netlink_init();
	vpp_init();
#ifdef CONFIG_VPP_PROC
	/* init system proc */
	if (vpp_proc_dir == 0) {
		struct proc_dir_entry *res;

		vpp_proc_dir = proc_mkdir("driver/vpp", NULL);
		res = create_proc_entry("sts", 0, vpp_proc_dir);
		if (res)
			res->read_proc = vpp_sts_read_proc;
		res = create_proc_entry("reg", 0, vpp_proc_dir);
		if (res)
			res->read_proc = vpp_reg_read_proc;
		vpp_table_header = register_sysctl_table(vpp_root_table);
	}
#endif

	/* init interrupt service routine */
#ifdef WMT_FTBLK_SCL
	if (vpp_request_irq(VPP_IRQ_SCL, vpp_interrupt_routine,
		SA_INTERRUPT, "scl", (void *)&g_vpp)) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}
#endif
	if (vpp_request_irq(VPP_IRQ_VPPM, vpp_interrupt_routine,
		SA_INTERRUPT, "vpp", (void *)&g_vpp)) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}
#ifdef WMT_FTBLK_GOVRH
	if (vpp_request_irq(VPP_IRQ_GOVR, vpp_interrupt_routine,
		SA_INTERRUPT, "govr", (void *)&g_vpp)) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}

	if (vpp_request_irq(VPP_IRQ_GOVR2, vpp_interrupt_routine,
		SA_INTERRUPT, "govr2", (void *)&g_vpp)) {
		DPRINT("*E* request VPP ISR fail\n");
		return -1;
	}
#endif
	vpp_switch_state_init();
	return 0;
}

static int wmt_vpp_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_PM
unsigned int wmt_vpp_vout_blank_mask;
static int wmt_vpp_suspend(struct device *dev)
{

	struct vpp_mod_base_t *mod_p;
	struct vout_t *vo;
	int i;

	DPRINT("Enter wmt_vpp_suspend\n");

	wmt_vpp_vout_blank_mask = 0;
	for (i = 0; i <= VPP_VOUT_NUM; i++) {
		vo = vout_get_entry(i);
		if (vo && !(vo->status & VPP_VOUT_STS_BLANK))
			wmt_vpp_vout_blank_mask |= (0x1 << i);
		vout_set_blank(i, VOUT_BLANK_POWERDOWN);
		if (vo && vo->dev && vo->dev->suspend)
			vo->dev->suspend();
	}

	if (vout_check_plugin(1))
		vpp_netlink_notify_plug(VPP_VOUT_ALL, 0);
	else
		wmt_set_mmfreq(0);

	/* disable module */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->suspend)
			mod_p->suspend(0);
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(0);
#endif
	wmt_suspend_mmfreq();
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(0);
#endif
	/* disable tg */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->suspend)
			mod_p->suspend(1);
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(1);
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(1);
#endif
	/* backup registers */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->suspend)
			mod_p->suspend(2);
	}
#ifdef WMT_FTBLK_HDMI
	hdmi_suspend(2);
#endif
#ifdef WMT_FTBLK_LVDS
	lvds_suspend(2);
#endif
#if 0
	if (lcd_get_lvds_id() == LCD_LVDS_1024x600) {
		mdelay(5);
		/* GPIO10 off  8ms -> clock -> off */
		outl(inl(GPIO_BASE_ADDR + 0xC0) & ~BIT10,
			GPIO_BASE_ADDR + 0xC0);
	}
#endif
	return 0;
}

static int wmt_vpp_freeze(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_VPP_SHENZHEN
extern int lcd_spi_resume(void);
#endif

static int wmt_vpp_resume(struct device *dev)
{

	struct vpp_mod_base_t *mod_p;
	int i;

#if 0
	if (lcd_get_lvds_id() == LCD_LVDS_1024x600) {
		/* GPIO10 6ms -> clock r0.02.04 */
		outl(inl(GPIO_BASE_ADDR + 0x80) | BIT10,
			GPIO_BASE_ADDR + 0x80);
		outl(inl(GPIO_BASE_ADDR + 0xC0) | BIT10,
			GPIO_BASE_ADDR + 0xC0);
	}
#endif

	DPRINT("Enter wmt_vpp_resume\n");

#ifdef CONFIG_VPP_SHENZHEN
	lcd_spi_resume();
#endif

	/* restore registers */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->resume)
			mod_p->resume(0);
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(0);
#endif
#ifdef WMT_FTBLK_HDMI
	hdmi_check_plugin(0);
	hdmi_resume(0);
#endif
	/* enable tg */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->resume)
			mod_p->resume(1);
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(1);
#endif
#ifdef WMT_FTBLK_HDMI
	hdmi_resume(1);
#endif
	/* wait */
	msleep(150);

	/* enable module */
	for (i = 0; i < VPP_MOD_MAX; i++) {
		mod_p = vpp_mod_get_base(i);
		if (mod_p && mod_p->resume)
			mod_p->resume(2);
	}
#ifdef WMT_FTBLK_LVDS
	lvds_resume(2);
#endif
	if (lcd_get_lvds_id() != LCD_LVDS_1024x600) {
		for (i = 0; i < VPP_VOUT_NUM; i++) {
                        struct vout_t *vo;

                        vo = vout_get_entry(i);
                        if (vo && vo->dev) {
				if (lcd_get_type() == 0) /* DVI type */
                                govrh_set_dvo_enable(vo->govr, 1);
                                vo->dev->init(vo);
                                vo->dev->set_mode(&vo->option[0]);
                                vo->inf->init(vo, 0);
                        }

			if (wmt_vpp_vout_blank_mask & (0x1 << i))
				vout_set_blank(i, VOUT_BLANK_UNBLANK);
			if (vo && vo->dev && vo->dev->resume)
				vo->dev->resume();
		}
	}
	wmt_resume_mmfreq();
	if (vout_check_plugin(0))
		vpp_netlink_notify_plug(VPP_VOUT_ALL, 1);
	else
		wmt_set_mmfreq(0);
#ifdef WMT_FTBLK_HDMI
	hdmi_resume(2);
#endif
	return 0;
}

static int wmt_vpp_restore(struct device *dev)
{
	return 0;
}

static void wmt_vpp_shutdown
(
	struct platform_device *pDev	/*!<; // a pointer struct device */
)
{
	DPRINT("wmt_vpp_shutdown\n");
	hdmi_set_power_down(1);
	lvds_set_power_down(1);
}

#else
#define wmt_vpp_suspend NULL
#define wmt_vpp_resume NULL
#define wmt_vpp_shutdown NULL
#endif

/********************************************************************
	device driver struct define
**********************************************************************/
static struct dev_pm_ops wmt_vpp_pm_ops = {
	.suspend	= wmt_vpp_suspend,
	.resume		= wmt_vpp_resume,
	.freeze		= wmt_vpp_freeze,
	.thaw		= wmt_vpp_restore,
	.restore	= wmt_vpp_restore,
};

static struct platform_driver wmt_vpp_driver = {
	.driver.name    = DEVICE_NAME, /* equal to platform device name. */
/*	.bus            = &platform_bus_type, */
	.probe          = wmt_vpp_probe,
	.remove         = wmt_vpp_remove,
	/* .suspend	= wmt_vpp_suspend, */
	/* .resume	= wmt_vpp_resume, */
	.shutdown	= wmt_vpp_shutdown,
	.driver.pm	= &wmt_vpp_pm_ops
};

static void wmt_vpp_platform_release(struct device *device)
{
}

/********************************************************************
	platform device struct define
*********************************************************************/
/* static u64 wmt_vpp_dma_mask = 0xffffffffUL; */
static struct platform_device wmt_vpp_device = {
	.name   = DEVICE_NAME,
	.id     = 0,
	.dev    = {
		.release = wmt_vpp_platform_release,
#if 0
		.dma_mask = &wmt_vpp_dma_mask,
		.coherent_dma_mask = ~0,
#endif
	},
	.num_resources  = 0,	/* ARRAY_SIZE(cipher_resources), */
	.resource       = NULL,	/* cipher_resources, */
};

static int __init wmt_vpp_init(void)
{
	int ret;
	dev_t dev_no;

	wmt_vpp_major = register_chrdev(0, DEVICE_NAME, &wmt_vpp_fops);
	if (wmt_vpp_major < 0) {
		DBG_ERR("get major failed\n");
		return -EFAULT;
	}

	wmt_vpp_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(wmt_vpp_class)) {
		ret = PTR_ERR(wmt_vpp_class);
		DBG_ERR("Can't create class : %s !!\n", DEVICE_NAME);
		return ret;
	}

	dev_no = MKDEV(wmt_vpp_major, 0);
	device_create(wmt_vpp_class, NULL, dev_no, NULL, DEVICE_NAME);
	ret = platform_device_register(&wmt_vpp_device);
	if (ret != 0)
		return -ENODEV;

	ret = platform_driver_register(&wmt_vpp_driver);
	if (ret != 0) {
		platform_device_unregister(&wmt_vpp_device);
		return -ENODEV;
	}
	return 0;
}

static void __exit wmt_vpp_exit(void)
{
	dev_t dev_no;

	DBG_MSG("\n");

	vout_exit();
#ifdef CONFIG_VPP_PROC
	unregister_sysctl_table(vpp_table_header);
#endif
	platform_driver_unregister(&wmt_vpp_driver);
	platform_device_unregister(&wmt_vpp_device);
	dev_no = MKDEV(wmt_vpp_major, 0);
	device_destroy(wmt_vpp_class, dev_no);
	class_destroy(wmt_vpp_class);
	unregister_chrdev(wmt_vpp_major, DEVICE_NAME);
	return;
}

module_init(wmt_vpp_init);
module_exit(wmt_vpp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WMT VPP driver");
MODULE_AUTHOR("WMT TECH");
MODULE_VERSION("1.0.0");

