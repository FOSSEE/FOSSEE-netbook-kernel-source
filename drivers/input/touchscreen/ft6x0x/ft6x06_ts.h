#ifndef __LINUX_FT6X06_TS_H__
#define __LINUX_FT6X06_TS_H__

/* -- dirver configure -- */
#define CFG_MAX_TOUCH_POINTS	2
#define MT_MAX_TOUCH_POINTS	9

#define PRESS_MAX	0xFF
#define FT_PRESS		0x7F

#define Proximity_Max	32

#define FT_FACE_DETECT_ON		0xc0
#define FT_FACE_DETECT_OFF		0xe0

#define FT_FACE_DETECT_ENABLE	1
#define FT_FACE_DETECT_DISABLE	0
#define FT_FACE_DETECT_REG		0xB0

#define FT6X06_NAME 	"ft6x06_ts"

#define FT_MAX_ID	0x0F
#define FT_TOUCH_STEP	6
#define FT_FACE_DETECT_POS		1
#define FT_TOUCH_X_H_POS		3
#define FT_TOUCH_X_L_POS		4
#define FT_TOUCH_Y_H_POS		5
#define FT_TOUCH_Y_L_POS		6
#define FT_TOUCH_EVENT_POS		3
#define FT_TOUCH_ID_POS			5

#define POINT_READ_BUF	(3 + FT_TOUCH_STEP * CFG_MAX_TOUCH_POINTS)

/*register address*/
#define FT6x06_REG_FW_VER		0xA6
#define FT6x06_REG_POINT_RATE	0x88
#define FT6x06_REG_THGROUP	0x80

int ft6x06_i2c_Read(struct i2c_client *client, char *writebuf, int writelen,
		    char *readbuf, int readlen);
int ft6x06_i2c_Write(struct i2c_client *client, char *writebuf, int writelen);

/* The platform data for the Focaltech ft6x06 touchscreen driver */
struct ft6x06_platform_data {
	unsigned int x_max;
	unsigned int y_max;
	unsigned long irqflags;
	unsigned int irq;
	unsigned int reset;
};

#endif
