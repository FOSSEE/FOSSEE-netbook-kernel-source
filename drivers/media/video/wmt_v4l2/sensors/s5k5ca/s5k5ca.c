#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "s5k5ca.h"

#define sensor_write_array(sd, arr, sz) \
	cmos_init_16bit_addr_16bit_data(arr, sz, (sd)->i2c_addr)

#define sensor_read(sd, reg) \
	wmt_vid_i2c_read16data(sd->i2c_addr, reg)

#define sensor_write(sd, reg, val) \
	wmt_vid_i2c_write16data(sd->i2c_addr, reg, val)

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
	CMOS_WIN_SIZE("VGA",   640,  480, s5k5ca_640_480_regs),
	CMOS_WIN_SIZE("VGA",   2048,  1536, s5k5ca_2048_1536_regs),	
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
		regs = s5k5ca_wb_auto;
		size = ARRAY_SIZE(s5k5ca_wb_auto);
		break;
	case WHITE_BALANCE_INCANDESCENCE:
		regs = s5k5ca_wb_incandescent;
		size = ARRAY_SIZE(s5k5ca_wb_incandescent);
		break;
	case WHITE_BALANCE_DAYLIGHT:
		regs = s5k5ca_wb_daylight;
		size = ARRAY_SIZE(s5k5ca_wb_daylight);
		break;
	case WHITE_BALANCE_CLOUDY:
		regs = s5k5ca_wb_cloudy;
		size = ARRAY_SIZE(s5k5ca_wb_cloudy);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		regs = s5k5ca_wb_fluorescent;
		size = ARRAY_SIZE(s5k5ca_wb_fluorescent);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_scenemode(struct cmos_subdev *sd, enum v4l2_scene_mode value)
{
	uint32_t *regs;
	size_t size;

	switch (value) {
	case SCENE_MODE_AUTO:
		regs = s5k5ca_scene_mode_auto;
		size = ARRAY_SIZE(s5k5ca_scene_mode_auto);
		break;
	case SCENE_MODE_NIGHTSHOT:
		regs = s5k5ca_scene_mode_night;
		size = ARRAY_SIZE(s5k5ca_scene_mode_night);
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
		regs = s5k5ca_exposure_neg6;
		size = ARRAY_SIZE(s5k5ca_exposure_neg6);
		break;
	case -1:
		regs = s5k5ca_exposure_neg3;
		size = ARRAY_SIZE(s5k5ca_exposure_neg3);
		break;
	case 0:
		regs = s5k5ca_exposure_zero;
		size = ARRAY_SIZE(s5k5ca_exposure_zero);
		break;
	case 1:
		regs = s5k5ca_exposure_pos3;
		size = ARRAY_SIZE(s5k5ca_exposure_pos3);
		break;
	case 2:
		regs = s5k5ca_exposure_pos6;
		size = ARRAY_SIZE(s5k5ca_exposure_pos6);
		break;
	default:
		return -EINVAL;
	}

	sensor_write_array(sd, regs, size);
	return 0;
}

static int sensor_s_hflip(struct cmos_subdev *sd, int value)
{

	uint32_t data;

	sensor_write(sd,0xfcfc,0xd000);
	sensor_write(sd,0x0028,0x7000);
	sensor_write(sd,0x002a,0x0296);

	data = 0;

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
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x02c6);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x02f6);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x0326);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);	
	msleep(100);

	return 0;
}

static int sensor_s_vflip(struct cmos_subdev *sd, int value)
{

	uint32_t data;

	sensor_write(sd,0xfcfc,0xd000);
	sensor_write(sd,0x0028,0x7000);
	sensor_write(sd,0x002a,0x0296);

	data = 0;

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
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x02c6);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x02f6);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x002a,0x0326);
	sensor_write(sd,0x0f12,data);
	sensor_write(sd,0x0f12,data);	
	msleep(100);

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
	case WMT_V4L2_CID_CAMERA_ANTIBANDING:
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
	return 0;
}

static int sensor_try_mbus_fmt(struct cmos_subdev *sd,
			       struct v4l2_mbus_framefmt *mf)
{
	const struct cmos_win_size *win;

	win = cmos_select_win(&mf->width, &mf->height);

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

static int sensor_identify(struct cmos_subdev *sd)
{
	uint32_t data = 0;
	
	sensor_write(sd,0xfcfc,0x0000);
	data = sensor_read(sd,0x0040);
	
	return (data == sd->id) ? 0 : -EINVAL;
}

static int sensor_init(struct cmos_subdev *sd)
{
	if (sensor_identify(sd)) {
		return -1;
	}

	sensor_write_array(sd, s5k5ca_default_regs_init0,
			   ARRAY_SIZE(s5k5ca_default_regs_init0));
	msleep(100);
	sensor_write_array(sd, s5k5ca_default_regs_init1,
			   ARRAY_SIZE(s5k5ca_default_regs_init1));
	msleep(100);
	sensor_write_array(sd, s5k5ca_default_regs_init2,
			   ARRAY_SIZE(s5k5ca_default_regs_init2));

	msleep(100);
	sensor_write_array(sd, s5k5ca_default_regs_init3,
			   ARRAY_SIZE(s5k5ca_default_regs_init3));

	msleep(100);
	sensor_write_array(sd, s5k5ca_default_regs_init4,
			   ARRAY_SIZE(s5k5ca_default_regs_init3));
	
	return 0;
}

static int sensor_exit(struct cmos_subdev *sd)
{
	return 0;
}

static struct cmos_subdev_ops s5k5ca_ops = {
	.identify	= sensor_identify,
	.init		= sensor_init,
	.exit		= sensor_exit,
	.queryctrl	= sensor_queryctrl,
	.s_ctrl		= sensor_s_ctrl,
	.s_mbus_fmt     = sensor_s_mbus_fmt,
	.g_mbus_fmt     = sensor_g_mbus_fmt,
	.try_mbus_fmt	= sensor_try_mbus_fmt,
	.enum_framesizes = sensor_enum_framesizes,
};

struct cmos_subdev s5k5ca = {
	.name		= "s5k5ca",
	.i2c_addr	= 0x3c,
	.id		= 0x5ca,
	.max_width	= 2048,
	.max_height	= 1536,
	.ops		= &s5k5ca_ops,
};

#if 0
static int __init s5k5ca_init(void)
{
	return cmos_register_sudbdev(&s5k5ca);
}

static void __exit s5k5ca_exit(void)
{
	return cmos_unregister_subdev(&s5k5ca);
}

module_init(s5k5ca_init);
module_exit(s5k5ca_exit);

MODULE_LICENSE("GPL");
#endif

