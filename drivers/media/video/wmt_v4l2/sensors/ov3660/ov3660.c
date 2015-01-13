/*
 * Driver for ov3660 CMOS Image Sensor from Omnivision
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/errno.h>
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "ov3660.h"

#define sensor_write_array(sd, arr, sz) \
	cmos_init_16bit_addr(arr, sz, (sd)->i2c_addr)

#define sensor_read(sd, reg) \
	wmt_vid_i2c_read16addr(sd->i2c_addr, reg)

#define sensor_write(sd, reg, val) \
	wmt_vid_i2c_write16addr(sd->i2c_addr, reg, val)

struct cmos_win_size {
	char		*name;
	int		width;
	int		height;
	uint32_t 	*regs;
	size_t		size;
};

#define CMOS_WIN_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r, .size = ARRAY_SIZE(r) }


static const struct cmos_win_size cmos_supported_win_sizes[] = {
	CMOS_WIN_SIZE("VGA",  640, 480,  ov3660_640_480_regs),
	CMOS_WIN_SIZE("QXGA", 2048, 1536, ov3660_2048_1536_regs),
};

static const struct cmos_win_size *cmos_select_win(u32 *width, u32 *height)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cmos_supported_win_sizes); i++) {
		if (cmos_supported_win_sizes[i].width == *width &&
		    cmos_supported_win_sizes[i].height == *height) {
			*width = cmos_supported_win_sizes[i].width;
			*height = cmos_supported_win_sizes[i].height;
			return &cmos_supported_win_sizes[i];
		}
	}
	return NULL;
}

static int sensor_s_wb(struct cmos_subdev *sd, enum v4l2_wb_mode value)
{
	uint32_t *regs;
	size_t size;

	switch (value) {
	case WHITE_BALANCE_AUTO:
		regs = ov3660_wb_auto;
		size = ARRAY_SIZE(ov3660_wb_auto);
		break;
	case WHITE_BALANCE_INCANDESCENCE:
		regs = ov3660_wb_incandescent;
		size = ARRAY_SIZE(ov3660_wb_incandescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		regs = ov3660_wb_daylight;
		size = ARRAY_SIZE(ov3660_wb_daylight);
		break;
	case WHITE_BALANCE_CLOUDY:
		regs = ov3660_wb_cloudy;
		size = ARRAY_SIZE(ov3660_wb_cloudy);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		regs = ov3660_wb_fluorescent;
		size = ARRAY_SIZE(ov3660_wb_fluorescent);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_queryctrl(struct cmos_subdev *sd, struct v4l2_queryctrl *qc)
{
	switch (qc->id) {
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_DO_WHITE_BALANCE:
		return v4l2_ctrl_query_fill(qc, 0, 3, 1, 0);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct cmos_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
	case V4L2_CID_DO_WHITE_BALANCE:
		return sensor_s_wb(sd, ctrl->value);	
	}
	return -EINVAL;
}

static int sensor_g_mbus_fmt(struct cmos_subdev *sd,
			     struct v4l2_mbus_framefmt *mf)
{
	return -EINVAL;
}

static int sensor_s_mbus_fmt(struct cmos_subdev *sd,
			     struct v4l2_mbus_framefmt *mf)
{
	const struct cmos_win_size *win;

	win = cmos_select_win(&mf->width, &mf->height);
	if (!win)
		return -EINVAL;

	sensor_write_array(sd, win->regs, win->size);
	return 0;
}

static int sensor_try_mbus_fmt(struct cmos_subdev *sd,
			       struct v4l2_mbus_framefmt *mf)
{
	const struct cmos_win_size *win;

	/*
	 * select suitable win
	 */
	win = cmos_select_win(&mf->width, &mf->height);

	return 0;
}

static int sensor_identify(struct cmos_subdev *sd)
{
	uint32_t data;

	data = sensor_read(sd, 0x300a);
	data <<= 8;
	data |= sensor_read(sd, 0x300b);

	return (data == sd->id) ? 0 : 1;
}

static int sensor_init(struct cmos_subdev *sd)
{
	if (!sensor_identify(sd))
		return -1;

	sensor_write(sd, 0x3103, 0x11);
	sensor_write(sd, 0x3008, 0x82);
	msleep(5);
	sensor_write_array(sd, ov3660_default_regs_init,
			   ARRAY_SIZE(ov3660_default_regs_init));
	return 0;
}

static int sensor_exit(struct cmos_subdev *sd)
{
	sensor_write(sd, 0x3017, 0x80);
	sensor_write(sd, 0x3018, 0x03);
	return 0;
}

static struct cmos_subdev_ops ov3660_ops = {
	.identify	= sensor_identify,
	.init		= sensor_init,
	.exit		= sensor_exit,
	.queryctrl	= sensor_queryctrl,
	.s_ctrl		= sensor_s_ctrl,
	.s_mbus_fmt     = sensor_s_mbus_fmt,
	.g_mbus_fmt     = sensor_g_mbus_fmt,
	.try_mbus_fmt	= sensor_try_mbus_fmt,
};

struct cmos_subdev ov3660 = {
	.name		= "ov3660",
	.i2c_addr	= 0x3c,
	.id		= 0x3660,
	.max_width	= 2048,
	.max_height	= 1536,
	.ops		= &ov3660_ops,
};

static int __init ov3660_init(void)
{
	return cmos_register_sudbdev(&ov3660);
}

static void __exit ov3660_exit(void)
{
	cmos_unregister_subdev(&ov3660);
	return;
}

module_init(ov3660_init);
module_exit(ov3660_exit);

MODULE_LICENSE("GPL");
