/****************************************************************
 *
 * sn310m-touch.c : i2c Touchscreen driver (platform data struct)
 *
 * Copyright (c) 2013 SEMISENS Co.,Ltd
 *      http://www.semisens.com
 *
 ****************************************************************/
#ifndef __SN310M_TOUCH_H
#define __SN310M_TOUCH_H

//----------------------------------------------
// register address for firmware update
//----------------------------------------------
#define REG_CMD_ISP_MODE		0x02F1	// 0x0200 : prepare eFlash, 0x0100 : finish eFalsh
#define REG_CMD_FLASH_BUS		0x04F1	// 0x0000 : set eFlash bus functions
#define REG_CMD_FLASH_ENABLE	0x08F1	// 0xFFFF : enable eFlash functions
#define REG_CMD_FLASH_AUTH		0x00F4	// 0x0100 : get eFlash approach authority
#define REG_CMD_FLASH_CON_EN	0x02F4	// 0x0000 : enable eFlash controller
#define REG_CMD_FLASH_COMMAND	0x04F4	// 0x0200 : erase eFlash, 0x0000 : write eFlash
#define REG_CMD_FLASH_BUSY		0x08F4	// [15] bit is busy flag for eflash eperating.

//----------------------------------------------
// register setting value for firmware update
//----------------------------------------------
#define REG_SET_PREPARE_FLASH_ACCESS	0x0200
#define REG_SET_FINISH_FLASH_ACCESS		0x0100
#define REG_SET_ENABLE_FLASH_ERASE		0x0200
#define REG_SET_ENABLE_FLASH_WRITE		0x0000

#define	SN310M_MAX_FW_SIZE		(10*1024)	// 10 Kbytes
#define REG_FIRMWARE_VERSION	(0x3EE0)

//----------------------------------------------
// Touch status & data register address
//----------------------------------------------
#define	REG_TS_STATUS		0x00E0

typedef struct status_reg__t {
	unsigned int ts_cnt		:4;	// lsb
	unsigned int reserved1	:4;
	unsigned int button		:5;
	unsigned int reserved2	:3;	// msb
} __attribute__ ((packed)) status_reg_t;

typedef union status_reg__u {
	unsigned short	uint;
	status_reg_t	bits;
}	__attribute__ ((packed))	status_reg_u;

#define	REG_TS_DATA_BASE	0x02E0
#define	REG_TS_DATA(x)		(((x * 6) << 8) + REG_TS_DATA_BASE)

typedef struct data_reg__t {
	unsigned short packet0;
	unsigned short packet1;
	unsigned short packet2;
} __attribute__ ((packed)) data_reg_t;

typedef union data_reg__u {
	unsigned int	uint;
	data_reg_t		bits;
} __attribute__ ((packed)) data_reg_u;


//----------------------------------------------
// i2c Control function
//----------------------------------------------
extern int 	sn310m_i2c_read(struct i2c_client *client, unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int len);
extern int 	sn310m_i2c_write(struct i2c_client *client, unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int len);

//----------------------------------------------
// Touch initialize & finalize function
//----------------------------------------------
extern int	sn310m_input_open(struct input_dev *input);
extern void	sn310m_enable(struct touch *ts);
extern void	sn310m_disable(struct touch *ts);
extern int	sn310m_early_probe(struct touch *ts);
extern int	sn310m_probe(struct touch *ts);

//----------------------------------------------
// Calibration function
//----------------------------------------------
extern int 	sn310m_calibration(struct touch *ts);

//----------------------------------------------
// Touch data processing function
//----------------------------------------------
extern void	sn310m_work(struct touch *ts);

//----------------------------------------------
// Firmware update Control function
//----------------------------------------------
extern int	sn310m_flash_firmware(struct device *dev, const char *fw_name);

#endif // __SN310M_TOUCH_H

