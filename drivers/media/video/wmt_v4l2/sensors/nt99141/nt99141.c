/*
 * Driver for nt99141 CMOS Image Sensor from Novatek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm/errno.h>
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "nt99141.h"

//#define sensor_write_array(sd, arr, sz) cmos_init_16bit_addr(arr, sz, (sd)->i2c_addr)

#define sensor_read(sd, reg) \
	wmt_vid_i2c_read16addr(sd->i2c_addr, reg)

#define sensor_write(sd, reg, val) \
	wmt_vid_i2c_write16addr(sd->i2c_addr, reg, val)

static int sensor_fps;
static int sensor_set_fps(struct cmos_subdev *sd, int fps);
static struct timer_list day_night_timer;
static struct cmos_subdev *day_night_sd;
static struct work_struct day_night_work;

static void sensor_write_array(struct cmos_subdev *sd, uint32_t *arr, size_t sz)
{
	uint32_t reg, data;
	int i;

	for (i = 0; i < sz; i += 2) {
		reg = arr[i];
		data = arr[i+1];

		if (reg == 0xffff)
			msleep(data);
		else
			wmt_vid_i2c_write16addr(sd->i2c_addr, reg ,data);
	}
}

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
	CMOS_WIN_SIZE("QVGA",  320,  240, nt99141_320_240_regs),
	CMOS_WIN_SIZE("VGA",   640,  480, nt99141_640_480_regs),
	CMOS_WIN_SIZE("720p", 1280,  720, nt99141_1280_720_regs),
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
		regs = nt99141_wb_auto;
		size = ARRAY_SIZE(nt99141_wb_auto);
		break;
	case WHITE_BALANCE_INCANDESCENCE:
		regs = nt99141_wb_incandescent;
		size = ARRAY_SIZE(nt99141_wb_incandescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		regs = nt99141_wb_daylight;
		size = ARRAY_SIZE(nt99141_wb_daylight);
		break;
	case WHITE_BALANCE_CLOUDY:
		regs = nt99141_wb_cloudy;
		size = ARRAY_SIZE(nt99141_wb_cloudy);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		regs = nt99141_wb_fluorescent;
		size = ARRAY_SIZE(nt99141_wb_fluorescent);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	msleep(100);
	return 0;
}

static int sensor_s_scenemode(struct cmos_subdev *sd, enum v4l2_scene_mode value)
{
	uint32_t *regs;
	size_t size;

	switch (value) {
	case SCENE_MODE_AUTO:
		regs = nt99141_scene_mode_auto;
		size = ARRAY_SIZE(nt99141_scene_mode_auto);
		break;
	case SCENE_MODE_NIGHTSHOT:
		regs = nt99141_scene_mode_night;
		size = ARRAY_SIZE(nt99141_scene_mode_night);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_exposure(struct cmos_subdev *sd, enum v4l2_exposure_mode value)
{
	uint32_t *regs;
	size_t size;

	switch (value) {
	case -2:
		regs = nt99141_exposure_neg6;
		size = ARRAY_SIZE(nt99141_exposure_neg6);
		break;
	case -1:
		regs = nt99141_exposure_neg3;
		size = ARRAY_SIZE(nt99141_exposure_neg3);
		break;
	case 0:
		regs = nt99141_exposure_zero;
		size = ARRAY_SIZE(nt99141_exposure_zero);
		break;
	case 1:
		regs = nt99141_exposure_pos3;
		size = ARRAY_SIZE(nt99141_exposure_pos3);
		break;
	case 2:
		regs = nt99141_exposure_pos6;
		size = ARRAY_SIZE(nt99141_exposure_pos6);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_hflip(struct cmos_subdev *sd, int value)
{
	uint32_t data = sensor_read(sd, 0x3022);

	switch (value) {
	case 0:
		data &= ~0x02;
		break;
	case 1:
		data |= 0x02;
		break;
	default:
		return -EINVAL;
	}

	return sensor_write(sd, 0x3022, data);
}

static int sensor_s_vflip(struct cmos_subdev *sd, int value)
{
	uint32_t data = sensor_read(sd, 0x3022);

	switch (value) {
	case 0:
		data &= ~0x01;
		break;
	case 1:
		data |= 0x01;
		break;
	default:
		return -EINVAL;
	}

	return sensor_write(sd, 0x3022, data);
}


static int sensor_s_DayNightMode(struct cmos_subdev *sd)
{
	uint32_t Data_EV = 0 ; 
  
   Data_EV = sensor_read(sd, 0x32d6);

	if(Data_EV > 0x9A) 
  {
	  sensor_write(sd, 0x302a, 0x08);
	}
	else
	{
    sensor_write(sd, 0x302a, 0x04);
  }

	return 0;

}

static int sensor_queryctrl(struct cmos_subdev *sd, struct v4l2_queryctrl *qc)
{
	switch (qc->id) {
	case V4L2_CID_VFLIP:
	case V4L2_CID_HFLIP:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_CAMERA_SCENE_MODE:
		return v4l2_ctrl_query_fill(qc, 0, 1, 1, 0);
	case V4L2_CID_DO_WHITE_BALANCE:
		return v4l2_ctrl_query_fill(qc, 0, 3, 1, 0);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, -2, 2, 1, 0);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct cmos_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_SCENE_MODE:
		return sensor_s_scenemode(sd, ctrl->value);
	case V4L2_CID_DO_WHITE_BALANCE:	
		return sensor_s_wb(sd, ctrl->value);	
	case V4L2_CID_EXPOSURE:
		return sensor_s_exposure(sd, ctrl->value);	
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->value);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->value);
	default:
	case WMT_V4L2_CID_CAMERA_FLASH:
		return -EINVAL;
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
	if (!win) {
		pr_err("%s, s_mbus_fmt failed, width %d, height %d\n", 
		       sd->name, mf->width, mf->height);
		return -EINVAL;
	}

	sensor_write_array(sd, win->regs, win->size);
	sensor_set_fps(sd,sensor_fps);
	msleep(200);
	
	return 0;
}

static int sensor_try_mbus_fmt(struct cmos_subdev *sd,
			       struct v4l2_mbus_framefmt *mf)
{
	return 0;
}

static int sensor_enum_framesizes(struct cmos_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	for (i = 0; i < ARRAY_SIZE(cmos_supported_win_sizes); i++) {
		const struct cmos_win_size *win = &cmos_supported_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int sensor_set_fps(struct cmos_subdev *sd, int fps)
{
	switch(fps){
		case 5:
			sensor_write_array(sd, nt99141_5fps_regs,
			   ARRAY_SIZE(nt99141_5fps_regs));
			break;
		case 10:
			sensor_write_array(sd, nt99141_10fps_regs,
			   ARRAY_SIZE(nt99141_10fps_regs));
			break;
		case 15:
			sensor_write_array(sd, nt99141_15fps_regs,
			   ARRAY_SIZE(nt99141_15fps_regs));
			break;
		case 20:
			sensor_write_array(sd, nt99141_20fps_regs,
			   ARRAY_SIZE(nt99141_20fps_regs));
		case 25:
			sensor_write_array(sd, nt99141_25fps_regs,
			   ARRAY_SIZE(nt99141_25fps_regs));
		case 30:
			sensor_write_array(sd, nt99141_30fps_regs,
			   ARRAY_SIZE(nt99141_30fps_regs));
		default:
			break;
	}

	return 0;
}

static int sensor_g_parm(struct cmos_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int sensor_s_parm(struct cmos_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct v4l2_fract *tpf = &cp->timeperframe;

	if (tpf->numerator == 0 || tpf->denominator == 0)	{
		return 0;
	}
	
	sensor_fps = tpf->denominator;//cause tpf->numerator == 1 in HAL

		
	return 0;
}

static int sensor_identify(struct cmos_subdev *sd)
{
	uint32_t data = 0;

	data |= (sensor_read(sd, 0x3000) & 0xff) << 8;
	data |= (sensor_read(sd, 0x3001) & 0xff);

	return (data == sd->id) ? 0 : -EINVAL;
}

static void day_night_do_work(struct work_struct *work)
{
	sensor_s_DayNightMode(day_night_sd);
}

static void day_night_timer_func(unsigned long __data)
{
	schedule_work(&day_night_work);
	mod_timer(&day_night_timer, jiffies + msecs_to_jiffies(3000));
}

static int sensor_init(struct cmos_subdev *sd)
{
	if (sensor_identify(sd))
		return -1;

	sensor_write_array(sd, nt99141_default_regs_init,
			   ARRAY_SIZE(nt99141_default_regs_init));
	
	day_night_sd = sd;
	INIT_WORK(&day_night_work, day_night_do_work);
	setup_timer(&day_night_timer, day_night_timer_func, (unsigned long)sd);
	mod_timer(&day_night_timer, jiffies + msecs_to_jiffies(3000));
	
	return 0;
}

static int sensor_exit(struct cmos_subdev *sd)
{
	sensor_write_array(sd, nt99141_default_regs_exit,
			   ARRAY_SIZE(nt99141_default_regs_exit));
	cancel_work_sync(&day_night_work);
	del_timer_sync(&day_night_timer);
	
	return 0;
}

static struct cmos_subdev_ops nt99141_ops = {
	.identify	= sensor_identify,
	.init		= sensor_init,
	.exit		= sensor_exit,
	.queryctrl	= sensor_queryctrl,
	.s_ctrl		= sensor_s_ctrl,
	.s_mbus_fmt     = sensor_s_mbus_fmt,
	.g_mbus_fmt     = sensor_g_mbus_fmt,
	.try_mbus_fmt	= sensor_try_mbus_fmt,
	.enum_framesizes = sensor_enum_framesizes,
	.g_parm			= sensor_g_parm,
	.s_parm			= sensor_s_parm,
	
};

struct cmos_subdev nt99141 = {
	.name		= "nt99141",
	.i2c_addr	= 0x2a,
	.id		= 0x1410,
	.max_width	= 1280,
	.max_height	= 720,
	.ops		= &nt99141_ops,
};

#if 0
static int __init nt99141_init(void)
{
	return cmos_register_sudbdev(&nt99141);
}

static void __exit nt99141_exit(void)
{
	cmos_unregister_subdev(&nt99141);
	return;
}

module_init(nt99141_init);
module_exit(nt99141_exit);

MODULE_LICENSE("GPL");
#endif
