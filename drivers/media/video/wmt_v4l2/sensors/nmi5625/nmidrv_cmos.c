
#include "../cmos-subdev.h"
#include "../../wmt-vid.h"
#include "nmidrv_cmos.h"

#define sensor_write_array(sd, arr, sz)
#define sensor_read(sd, reg)
#define sensor_write(sd, reg, val)

struct cmos_win_size {
	char		*name;
	int		width;
	int		height;
	uint8_t 	*regs;
	size_t		size;
};

#define CMOS_WIN_SIZE(n, w, h, r) \
	{.name = n, .width = w , .height = h, .regs = r, .size = ARRAY_SIZE(r) }

#define FILP_REG_INIT_VALUE	  0x10
#define DELAY_INTERVAL		50
#define RETRY_TIMES              10

static const struct cmos_win_size cmos_supported_win_sizes[] = {
	CMOS_WIN_SIZE("QVGA", 320, 240, nmi_320_240_regs),
//	CMOS_WIN_SIZE("VGA",  640, 480, nmi_640_480_regs),
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

static int sensor_queryctrl(struct cmos_subdev *sd, struct v4l2_queryctrl *qc)
{
	return -EINVAL;
}

static int sensor_s_ctrl(struct cmos_subdev *sd, struct v4l2_control *ctrl)
{
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
	extern int nmi_running(void);
	return nmi_running() ? 0 : -EINVAL;
}

static int sensor_init(struct cmos_subdev *sd)
{
	if (sensor_identify(sd)) {
		return -1;
	}

	sensor_write_array(sd, nmi_default_regs_init,
			   ARRAY_SIZE(nmi_default_regs_init));
	return 0;
}

static int sensor_exit(struct cmos_subdev *sd)
{
	sensor_write_array(sd, nmi_default_regs_exit,
			   ARRAY_SIZE(nmi_default_regs_exit));
	return 0;
}

static struct cmos_subdev_ops nmi_ops = {
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

struct cmos_subdev nmi = {
	.name		= "nmi",
	.i2c_addr	= 0xff,
	.id		= 0xff,
	.max_width	= 320,
	.max_height	= 240,
	.ops		= &nmi_ops,
};

#if 0
int nmi_cmos_init(void)
{
	return cmos_register_sudbdev(&nmi);
}

void nmi_cmos_exit(void)
{
	return cmos_unregister_subdev(&nmi);
}
#endif
