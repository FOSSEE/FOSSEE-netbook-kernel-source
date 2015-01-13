/*++
	WM8880 eFuse char device driver

	Copyright (c) 2013 - 2014  WonderMedia Technologies, Inc.

	This program is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software Foundation,
	either version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <http://www.gnu.org/licenses/>.

	WonderMedia Technologies, Inc.
	2013-11-11, HowayHuo, ShenZhen
--*/
/*--- History -------------------------------------------------------------------
*     DATE          |         AUTHORS         |        DESCRIPTION
*   2013/11/11               Howay Huo             v1.0, First Release
*
*
*------------------------------------------------------------------------------*/

/*---------------------------- WM8880 eFuse Layout -------------------------------------------------------

     Type                  StartAddr   DataBytes   HammingECCBytes    OccupyBytes
Hardware Reserved              0          8              0                8
Bounding                       8          6              2                8
CPUID                          16         8              3                12
UUID                           28         24             8                32
Unused Space                   60         51             17               68

Explain:
OccupyBytes = 3aligned(DataBytes) + HammingECCBytes
eFuse Total 128 Bytes = 8 HardwareReservedBytes + 90 DataBytes + 30 ECCBytes
1 Block = 4 Bytes = 3 DataBytes + 1 ECCByte
The minimum unit of eFuse_ECC_Read/Write is 1 Block (4 Bytes)
eFuse Total 32 Block = 2 HardwareReservedBlocks + 30 AvailableBlock

---------------------------------------------------------------------------------------------------------*/

//Control files: /sys/class/efuse/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <mach/wmt_iomux.h>
#include <asm/uaccess.h>

#include "wmt_efuse.h"

/*********************************** Constant Macro **********************************/
#define EFUSE_NAME   "efuse"
#define EFUSE_AVAILABLE_START_BYTE       8
#define EFUSE_AVAILABLE_BYTES_NUM        ((128 - EFUSE_AVAILABLE_START_BYTE) / 4 * 3) // (120 / 4) * 3

/*
* Note: Max bytes number should be 3 aligned
*/
#define EFUSE_BOUND_MAX_BYTES_NUM       6
#define EFUSE_CPUID_MAX_BYTES_NUM       8
#define EFUSE_UUID_MAX_BYTES_NUM        24

//#define EFUSE_WRITE_THROUGH_SYSFS

/*
* For example: setenv wmt.efuse.gpio 14:1
* It means pull high gpio14 to enable efuse write
*/
#define ENV_EFUSE_GPIO "wmt.efuse.gpio"

/**************************** Data type and local variable ***************************/
/*
*  WM8880 VDD25EFUSE pin is controlled by WM8880 GPIO via a MOS.
*  Pull High VDD25EFUSE pin to program eFuse. Pull Low VDD25EFUSE pin to read eFuse.
*/
static VDD25_GPIO vdd25_control_pin = {WMT_PIN_GP1_GPIO14, 1};
static int vdd25_control;

static const char *otp_type_str[] = {
	"bound",
	"cpuid",
	"uuid",
};

static int efuse_major;
static struct device *p_efuse_device;

/********************************** Function declare *********************************/
#undef DEBUG
//#define DEBUG  //if you need see the debug info, please define it.

#undef DBG

#ifdef DEBUG
#define DBG(fmt, args...) printk(KERN_INFO "[" EFUSE_NAME "] " fmt , ## args)
#else
#define DBG(fmt, args...)
#endif

#define INFO(fmt, args...) printk(KERN_INFO "[" EFUSE_NAME "] " fmt , ## args)
#define ERROR(fmt,args...) printk(KERN_ERR "[" EFUSE_NAME "]Error: " fmt , ## args)
#define WARNING(fmt,args...) printk(KERN_WARNING "[" EFUSE_NAME "]Warning: " fmt , ## args)

extern int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen);

/******************************** Function implement *********************************/
static void int_to_bin(unsigned int data, unsigned char *binary, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		binary[i] = data % 2;
		data = (data >> 1);
	}
}

static int bin_to_int(unsigned char *binary, unsigned int *p_data, int len)
{
	int i;
	unsigned char data = 0, *p;

	p = (unsigned char *)p_data;

	for(i = 0; i < len; i++) {
		if(binary[i] != 0 && binary[i] != 1) {
			ERROR("wrong binary[%d] = 0x%02X\n", i, binary[i]);
			return -1;
		}

		data += (binary[i] << (i % 8));
		//printk("binary[%d] = %d, data = %d\n", i, binary[i], data);

		if((i + 1) % 8 == 0) {
			*p = data;
			p++;
			data = 0;
		}
	}

	return 0;
}

/* matrix Inner product */
static unsigned char hamming_check(unsigned int data, unsigned int parity, int len)
{
	unsigned char *dst;
	unsigned char *src;
	int i, count = 0;

	dst = kzalloc(len, GFP_KERNEL);
	src = kzalloc(len, GFP_KERNEL);

	int_to_bin(data, dst, len);
	int_to_bin(parity, src, len);

	for (i = 0; i < len; i++) {
		if (dst[i] & src[i])
			count++;
	}

	kfree(dst);
	kfree(src);

	if (count % 2)
		return 1;
	else
		return 0;
}

static int hamming_encode_31_26(unsigned int data, unsigned int *p_hamming_data)
{
	int i, ret;
	unsigned int parity_num[5] = {0x2AAAD5B, 0x333366D, 0x3C3C78E, 0x3FC07F0, 0x3FFF800};
	unsigned int tmp;
	unsigned char encode_data[32];
	int len;
	char hamming_binary_buf[32];

	tmp = data;
	for (i = 0; i < 31; i++) {
		if (i == 0)
			encode_data[0] = hamming_check(data, parity_num[0], 26);
		else if (i == 1)
			encode_data[1] = hamming_check(data, parity_num[1], 26);
		else if (i == 3)
			encode_data[3] = hamming_check(data, parity_num[2], 26);
		else if (i == 7)
			encode_data[7] = hamming_check(data, parity_num[3], 26);
		else if (i == 15)
			encode_data[15] = hamming_check(data, parity_num[4], 26);
		else {
			if ((tmp << 31) >> 31)
				encode_data[i] = 1;
			else
				encode_data[i] = 0;
			tmp >>= 1;
		}
	}

	encode_data[31] = 0;

	len = 0;
	for(i = 30; i >= 0; i--) {
		if(encode_data[i] != 0 && encode_data[i] != 1) {
			ERROR("wrong hamming_encode_31_26, encode_data[%d] = 0x%02X\n", i, encode_data[i]);
			return -1;
		}
		len += sprintf(hamming_binary_buf + len, "%d", encode_data[i]);
	}
	hamming_binary_buf[31] = 0;

	ret = bin_to_int(encode_data, p_hamming_data, 32);
	if(ret)
		return -1;

	INFO("hamming_encode_31_26: 0x%08X ==> 0x%08X (%s)\n",
		data, *p_hamming_data, hamming_binary_buf);

	return 0;
}

static void efuse_vdd25_active(int active)
{
	if(vdd25_control == 0)
		return;

	if(active)
		gpio_direction_output(vdd25_control_pin.gpiono, vdd25_control_pin.active ? 1 : 0);
	else
		gpio_direction_output(vdd25_control_pin.gpiono, vdd25_control_pin.active ? 0 : 1);
}

static void __efuse_read_ready(void)
{
	unsigned int mode;
	unsigned int val = 0;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	/* CSB set high */
	val |= EFUSE_DIR_CSB;

	/* VDDQ set high */
	val |= EFUSE_DIR_VDDQ;

	/* PGENB set low */
	val &= ~EFUSE_DIR_PGENB;

	/* STROBE set low */
	val &= ~EFUSE_DIR_STROBE;

	/* LOAD set low */
	val &= ~EFUSE_DIR_LOAD;

	EFUSE_DIR_CMD_VAL = val;

	DBG("__efuse_read_ready: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);

	udelay(1);
}

static void __efuse_read_init(void)
{
	unsigned int mode;

	__efuse_read_ready();

	/* set DirectAccess */
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_DA;
	EFUSE_MODE_VAL = mode;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;
	udelay(1);

	/* CSB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_CSB;
	udelay(1);

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;
	udelay(1);

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;
	udelay(1);

	/* STROBE set low */
	//EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;

	DBG("__efuse_read_init: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);
}

static void __efuse_read_exit(void)
{
	unsigned int mode;

	/* LOAD set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_LOAD;
	udelay(1);

	/* PGENB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_PGENB;
	udelay(1);

	/* CSB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_CSB;
	udelay(1);

	/* VDDQ set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_VDDQ;
	udelay(1);

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	udelay(1);

	/* Standby mode */
	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;


	DBG("__efuse_read_exit\n");
}

static void print_write_read_bytes(int have_title, unsigned char *p_wbuf, unsigned char *p_rbuf, int count)
{
	int i;
	int len;
	char *pbuf;
	unsigned char *p;

	/* print Write bytes */
	if(p_wbuf != NULL) {
		len = 0;
		p = p_wbuf;

		pbuf = (unsigned char *)kzalloc(count * 6, GFP_KERNEL);
		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 16 == 0)
					len += sprintf(pbuf + len, "\n");
			} else {
				if(i != 0 && i % 16 == 0)
					len += sprintf(pbuf + len, "\n");
			}

			if(i != count - 1)
				len += sprintf(pbuf + len, "0x%02x,", *(p + (count - 1 - i)));
			else
				len += sprintf(pbuf + len, "0x%02x\n", *(p + (count - 1 - i)));
		}

		if(have_title)
			INFO("write bytes: %s", pbuf);
		else
			INFO("%s", pbuf);

		kfree(pbuf);
	}

	/* print Read bytes */
	if(p_rbuf != NULL) {
		len = 0;
		p = p_rbuf;

		pbuf = (unsigned char *)kzalloc(count * 6, GFP_KERNEL);
		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 16 == 0)
					len += sprintf(pbuf + len, "\n");
			} else {
				if(i != 0 && i % 16 == 0)
					len += sprintf(pbuf + len, "\n");
			}


			if(i != count - 1)
				len += sprintf(pbuf + len, "0x%02x,", *(p + (count - 1 - i)));
			else
				len += sprintf(pbuf + len, "0x%02x\n", *(p + (count - 1 - i)));
		}

		if(have_title)
			INFO("read bytes: %s", pbuf);
		else
			INFO("%s", pbuf);

		kfree(pbuf);
	}
}

static void print_write_read_integers(int have_title, unsigned int *p_wbuf, unsigned int *p_rbuf, int count)
{
	int i;
	int len;
	char *pbuf;
	unsigned int *p;

	/* print Write integers */
	if(p_wbuf != NULL) {
		len = 0;
		p = p_wbuf;

		pbuf = (unsigned char *)kzalloc(count * 12, GFP_KERNEL);
		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 4 == 0)
					len += sprintf(pbuf + len, "\n");
			} else {
				if(i != 0 && i % 4 == 0)
					len += sprintf(pbuf + len, "\n");
			}

			if(i != count - 1)
				len += sprintf(pbuf + len, "0x%08x,", *(p + (count - 1 - i)));
			else
				len += sprintf(pbuf + len, "0x%08x\n", *(p + (count - 1 - i)));
		}

		if(have_title)
			INFO("write integers: %s", pbuf);
		else
			INFO("%s", pbuf);

		kfree(pbuf);
	}

	/* print Read integers */
	if(p_rbuf != NULL) {
		len = 0;
		p = p_rbuf;

		pbuf = (unsigned char *)kzalloc(count * 12, GFP_KERNEL);
		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 4 == 0)
					len += sprintf(pbuf + len, "\n");
			} else {
				if(i != 0 && i % 4 == 0)
					len += sprintf(pbuf + len, "\n");
			}


			if(i != count - 1)
				len += sprintf(pbuf + len, "0x%08x,", *(p + (count - 1 - i)));
			else
				len += sprintf(pbuf + len, "0x%08x\n", *(p + (count - 1 - i)));
		}

		if(have_title)
			INFO("read integers: %s", pbuf);
		else
			INFO("%s", pbuf);

		kfree(pbuf);
	}
}

static int efuse_read_bytes(int start, unsigned char *p_read_bytes, int count)
{
	int i, read_count;
	unsigned char *p;

	memset(p_read_bytes, 0, count);

	if(start > 127 || start < 0) {
		ERROR("efuse_read_bytes: start = %d is invalid, it should be 0 ~ 127\n", start);
		return -1;
	}

	if(start + count > 128)
		read_count = 128 - start;
	else
		read_count = count;

	__efuse_read_init();

	p  = p_read_bytes;

	for(i = start; i < start + read_count; i++) {
		/* set read address */
		EFUSE_ADR_VAL = i;

		/* STROBE set high to read data */
		EFUSE_DIR_CMD_VAL |= EFUSE_DIR_STROBE;
		udelay(1);

		/* STROBE set low */
		EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;
		udelay(1);

		/* read data */
		*p = EFUSE_RD_DATA_VAL & 0xFF;

		p++;
	}

	__efuse_read_exit();

#ifdef DEBUG
	print_write_read_bytes(1, NULL, p_read_bytes, count);
#endif

	if(read_count != count) {
		ERROR("Need read %d bytes. In fact read %d bytes\n", count, read_count);
		print_write_read_bytes(1, NULL, p_read_bytes, count);
	}

	return read_count;
}

static void __efuse_write_ready(void)
{
	unsigned int mode;
	unsigned int val = 0;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	/* CSB set high */
	val |= EFUSE_DIR_CSB;

	/* VDDQ set low */
	val &= ~EFUSE_DIR_VDDQ;

	/* PGENB set high */
	val |= EFUSE_DIR_PGENB;

	/* STROBE set low */
	val &= ~EFUSE_DIR_STROBE;

	/* LOAD set high */
	val |= EFUSE_DIR_LOAD;

	EFUSE_DIR_CMD_VAL = val;

	DBG("__efuse_write_ready: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);

	udelay(1);
}

static void __efuse_write_init(void)
{
	unsigned int mode;

	__efuse_write_ready();

	/* set DirectAccess */
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_DA;
	EFUSE_MODE_VAL = mode;

	/* PGENB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_PGENB;
	udelay(1);

	/* VDD25 set high */
	efuse_vdd25_active(1);

	/* VDDQ set high*/
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_VDDQ;
	udelay(1);

	/* LOAD set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_LOAD;
	udelay(1);

	/* CSB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_CSB;
	udelay(1);

	DBG("__efuse_write_init: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);
}

static void __efuse_write_exit(void)
{
	int mode;

	/* CSB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_CSB;
	udelay(1);

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;
	udelay(1);

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;
	udelay(1);

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;

	udelay(1);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	DBG("__efuse_write_exit\n");
}

static int efuse_write_bytes(int start, unsigned char *p_write_bytes, int count)
{
	int i, j;
	int write_addr;
	unsigned char binary[8];
	unsigned char *p, *pbuf;

	if(start < 8 || start > 127) {
		ERROR("efuse_write_bytes: start = %d is invalid, it should be 8 ~ 127\n", start);
		return -1;
	}

	if(start + count > 128) {
		ERROR("efuse_write_bytes: byte_no range[%d - %d], it should be in range[0 - 127]\n",
			start, start + count -1);
		return -1;
	}

	pbuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
	if(pbuf == NULL) {
		ERROR("efuse_write_bytes: kzalloc buf fail\n");
		return -2;
	}

	DBG("========== read byte before write ==========\n");

	efuse_read_bytes(start, pbuf, count);

	for(i = 0; i < count; i++) {
		if(*(pbuf + i) != 0) {
			ERROR("eFuse has been programmed. Byte%d = 0x%02X. Exit.\n",
				start + i, *(pbuf + i));
			print_write_read_bytes(1, NULL, pbuf, count);
			kfree(pbuf);
			return -3;
		}
	}

	DBG("========== start write efuse ==========\n");

	p = p_write_bytes;

	__efuse_write_init();

	for(i = start; i < start + count; i++) {
		int_to_bin(*p, binary, 8);
		DBG("binary: %d%d%d%d%d%d%d%d\n", binary[7], binary[6], binary[5], binary[4],
			binary[3], binary[2], binary[1], binary[0]);
		for(j = 0; j < 8; j++) {
			if(binary[j]) {
				/* <data location:3 bits><eFuse address:7 bits> */
				write_addr = ((j << 7) | i );

				/* address */
				EFUSE_ADR_VAL = write_addr;

				/* STROBE set high to write data */
				EFUSE_DIR_CMD_VAL |= EFUSE_DIR_STROBE;

				/* wait 12us */
				udelay(12);

				/* STROBE set low */
				EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;

				udelay(1);
			}
		}

		p++;
	}

	__efuse_write_exit();

	DBG("========== read byte after write ==========\n");

	efuse_read_bytes(start, pbuf, count);

	p = p_write_bytes;
	for(i = 0; i < count; i++) {
		if(*(pbuf + i) != *(p + i)) {
			ERROR("=========> eFuse Write Failed !!!\n");
			ERROR("efuse_write_bytes: Byte%d wrtie 0x%02X, But read back 0x%02X\n",
				start + i, *(p + i), *(pbuf + i));
			print_write_read_bytes(1, p_write_bytes, pbuf, count);
			kfree(pbuf);
			return -4;
		}
	}

	DBG("=========> eFuse Write Success !!!\n");

#ifdef DEBUG
	print_write_read_bytes(1, p_write_bytes, pbuf, count);
#endif
	kfree(pbuf);

	return count;
}

static int efuse_hamming_write_int(int start, unsigned int *p_write_int, int count)
{
	int i, ret;
	unsigned int *p_hamming_data;
	unsigned char *p_write_bytes;

	if(start > 127 || start < 8) {
		ERROR("efuse_hamming_write_int: invalid start = %d, start should be 8 ~ 127\n", start);
		return -1;
	}

	if(start % 4 != 0) {
		ERROR("efuse_hamming_write_int: invalid start = %d, start should be 4 align\n", start);
		return -1;
	}

	if(count < 1) {
		ERROR("efuse_hamming_write_int: count = %d, it must >= 1\n", count);
		return -1;
	}

	if((start + count * 4) > 128) {
		ERROR("efuse_hamming_write_int: start + count * 4 = %d, it shoud <= 128\n", start + count * 4);
		return -1;
	}

	p_hamming_data = (unsigned int *)kzalloc(count * 4, GFP_KERNEL);
	if (p_hamming_data == NULL) {
		ERROR("efuse_hamming_write_int: kzalloc fail\n");
		return -1;
	}

	for(i = 0; i < count; i++) {
		ret = hamming_encode_31_26(*(p_write_int + i), p_hamming_data + i);
		if(ret) {
			kfree(p_hamming_data);
			return -1;
		}
/*
		{
			unsigned int tmp;
			tmp = *(p_hamming_data + i);
			*(p_hamming_data + i) &= 0xFF0FFFFF;
			INFO("Modify: 0x%08X ==> 0x%08X\n", tmp, *(p_hamming_data + i));
		}
*/
	}

	INFO("write %d hamming data:\n", count);
	print_write_read_integers(0, p_hamming_data, NULL, count);

	p_write_bytes = (unsigned char *)p_hamming_data;

	ret = efuse_write_bytes(start, p_write_bytes, count * 4);
	if(ret != count * 4) {
		kfree(p_hamming_data);
		return -1;
	}

	kfree(p_hamming_data);

	INFO("hamming write Byte%d ~ Byte%d (%d Bytes) success\n", start, start + count * 4 - 1, count * 4);

	return count;
}

static int efuse_hamming_read_int(int start, unsigned int *p_read_int, unsigned char *p_ecc_error, int count)
{
	unsigned int mode;
	int i, index;
	const int timeout = 500;
	int read_count = count;

	if(start > 127 || start < 8) {
		ERROR("efuse_hamming_read_int: start = %d is invalid, it should be 8 ~ 127\n", start);
		return -1;
	}

	if(start % 4 != 0) {
		ERROR("efuse_hamming_read_int: invalid byte_no = %d, byte_no should be 4 aligned\n", start);
		return -1;
	}

	if(read_count < 1) {
		ERROR("efuse_hamming_read_int: read_count = %d, it must >= 1\n", read_count);
		return -1;
	}

	if((start + read_count * 4) > 128) {
		WARNING("efuse_hamming_read_int: start + read_count * 4 = %d, it shoud <= 128\n", start + read_count * 4);
		while ((start + read_count * 4) > 128)
			read_count--;
	}

	/* VDD25 set low */
	efuse_vdd25_active(0);

	udelay(1);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	udelay(1);

	/* Make sure EFUSE_MODE[1:0] = 2'b00 */
	i = 0;
	while(i < timeout) {
		if((EFUSE_MODE_VAL & 0xFF) == EFUSE_MODE_IDLE)
			break;

		i++;
		INFO("efuse_hamming_read_int: wait %d ms for EFUSE_MODE idle\n", i);
		msleep(1);

	}

	if(i == timeout) {
		ERROR("efuse_hamming_read_int fail, EFUSE_MODE couldn't be set to IDLE.\n");
		return -1;
	}

	/* Program EFUSE_MODE[30] = 1'b1 */
	EFUSE_MODE_VAL |= EFUSE_ECC_EN;
	udelay(1);

	/* Program EFUSE_MODE[31] = 1'b1 */
	EFUSE_MODE_VAL |= EFUSE_ECC_READ;
	udelay(1);

	/* Wait EFUSE_MODE[31] return back to 1'b0 */
	i = 0;
	while(i < timeout) {
		if((EFUSE_MODE_VAL & EFUSE_ECC_READ) == 0)
			break;

		i++;
		msleep(1);
		INFO("efuse_hamming_read_int: wait %d ms for read completed\n", i);
	}

	if(i == timeout) {
		ERROR("efuse_hamming_read_int timeout\n");
		return -1;
	}

	INFO("EFUSE_ECC_STATUS_0_VAL = 0x%08X\n", EFUSE_ECC_STATUS_0_VAL);
	INFO("EFUSE_ECC_STATUS_1_VAL = 0x%08X\n", EFUSE_ECC_STATUS_1_VAL);

	index = start / 4 - 2;

	for(i = 0; i < read_count; i++) {
		if((EFUSE_ECC_STATUS_0_VAL >> index) & 0x01) {
			WARNING("ECC Error Detected in Addr %d ~ Addr %d\n", (index + 2) * 4, (index + 2) * 4 + 3);
			*(p_ecc_error + i) = 1;
		} else
			*(p_ecc_error + i) = 0;

		/* Program EFUSE_ECCSRAM_ADR. */
		EFUSE_ECCSRAM_ADR_VAL = (start + i * 4 - 8) / 4;

		/* Note: Program EFUSE_ECCSRAM_ADR again. Otherwise, the  EFUSE_ECCSRAM_RDPORT_VAL isn't updated */
		EFUSE_ECCSRAM_ADR_VAL = (start + i * 4 - 8) / 4;

		udelay(1);

		/* Read the EFUSE_ECCSRAM_RDPORT to retrieve the ECCSRAM content */
		*(p_read_int + i) = EFUSE_ECCSRAM_RDPORT_VAL & 0x3FFFFFF;

		index++;
	}

	if(read_count != count) {
		ERROR("Need read %d bytes. In fact read %d bytes\n", count, read_count);
		print_write_read_integers(1, NULL, p_read_int, count);
	}

	return read_count;
}

static int bytes_3aligned(int bytes_num)
{
	int aligned_bytes;

	// 3 aligned
	if(bytes_num % 3)
		aligned_bytes = bytes_num + (3 - bytes_num % 3);
	else
		aligned_bytes = bytes_num;

	return aligned_bytes;
}

static int caculate_need_bytes(int available_bytes)
{
	int need_bytes;

	// 4 aligned
	need_bytes = bytes_3aligned(available_bytes) / 3 * 4;

	//INFO("caculate_need_bytes = %d\n", need_bytes);

	return need_bytes;
}

/*------------------------------------------------------------------------------
 *
 * Function: efuse_write_otp()
 * Param:
 *	type: CPUID,UUID,...,etc
 *	wbuf: write bytes
 *	wlen: how many bytes need to write
 * Return:
 *      return the write bytes number
 *      If the write bytes number is equal to the excepted bytes number, success.
 *	Otherwise, fail
 *
 *------------------------------------------------------------------------------*/
int efuse_write_otp(OTP_TYPE type, unsigned char *wbuf, int wlen)
{
	int i, ret, start, byte_len, int_len, max_byte_len;
	unsigned char *tmpbuf;
	unsigned int *p_write_int;

	// data length should be 3 aligned
	byte_len = bytes_3aligned(wlen);

	int_len = byte_len / 3;

	switch(type) {
		case OTP_CPUID:
			max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM);
		break;

		case OTP_UUID:
			max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM)
				+ caculate_need_bytes(EFUSE_CPUID_MAX_BYTES_NUM);
		break;

		default:
			ERROR("efuse_write_otp: Not support otp type %s\n", otp_type_str[type]);
		return -1;
	}

	if(wlen > max_byte_len) {
		ERROR("efuse_write_otp(type: %s): length = %d, it should be <= %d\n",
			otp_type_str[type], wlen, max_byte_len);
		return -1;
	}

	if(start + int_len * 4 > 128) {
		ERROR("efuse_write_otp(type: %s): start + int_len * 4 = %d, it should be <= 128\n",
			otp_type_str[type], start + int_len * 4);
		return -1;
	}

	INFO("%s: write %d bytes:\n", otp_type_str[type], wlen);
	print_write_read_bytes(0, wbuf, NULL, wlen);

	tmpbuf = (unsigned char *)kzalloc(int_len * 4, GFP_KERNEL);
	if (tmpbuf == NULL) {
		ERROR("efuse_write_otp(type: 0x%s): kzalloc tmpbuf fail\n", otp_type_str[type]);
		return -1;
	}

	// check whether the efuse is programmed
	ret = efuse_read_bytes(start, tmpbuf, int_len * 4);
	if(ret != int_len * 4) {
		ERROR("efuse_write_otp(type: %s): efuse_read_bytes fail\n", otp_type_str[type]);
		kfree(tmpbuf);
		return -1;
	}

	for(i = 0; i < int_len * 4; i++) {
		if(tmpbuf[i] != 0) {
			ERROR("efuse_write_otp(type: %s): eFuse has been programmed. Byte%d = 0x%02X. Exit.\n",
				otp_type_str[type], start + i, *(tmpbuf + i));
			INFO("Efuse Byte%d ~ Byte%d (%d Bytes) as follows:\n",
				start, start + int_len * 4 - 1, int_len * 4);
			print_write_read_bytes(0, NULL, tmpbuf, int_len * 4);
			kfree(tmpbuf);
			return -1;
		}
	}

	// convert bytes to integer
	memset(tmpbuf, 0, int_len * 4);
	memcpy(tmpbuf, wbuf, wlen);

	p_write_int = (unsigned int *)kzalloc(int_len * 4, GFP_KERNEL);
	if (p_write_int == NULL) {
		ERROR("efuse_write_otp(type: %s): kzalloc p_write_int buffer fail\n", otp_type_str[type]);
		kfree(tmpbuf);
		return -1;
	}

	for(i = 0; i < int_len; i++)
		p_write_int[i] = tmpbuf[i * 3 + 2] << 16 | (tmpbuf[i * 3 + 1] << 8) | tmpbuf[i * 3];

	INFO("write %d raw integers:\n", int_len);
	print_write_read_integers(0, p_write_int, NULL, int_len);

	// write otp
	ret = efuse_hamming_write_int(start, p_write_int, int_len);
	if(ret != int_len) {
		ERROR("efuse_write_otp(type: %s): efuse_hamming_write_int fail\n", otp_type_str[type]);
		kfree(tmpbuf);
		kfree(p_write_int);
		return -1;
	}

	kfree(tmpbuf);
	kfree(p_write_int);

	INFO("efuse_write_otp(type: %s) success\n", otp_type_str[type]);

	return wlen;
}
EXPORT_SYMBOL_GPL(efuse_write_otp);

/*------------------------------------------------------------------------------
 *
 * Function: efuse_read_otp()
 * Param:
 *	type: CPUID,UUID,...,etc
 *	rbuf: readback bytes
 *	rlen: how many bytes need to read
 * Return:
 *	return the read bytes number
 *      If the read bytes number is equal to the excepted bytes number, success.
 *	Otherwise, fail
 *
 *------------------------------------------------------------------------------*/
int efuse_read_otp(OTP_TYPE type, unsigned char *rbuf, int rlen)
{
	int i, ret, start, byte_len, int_len, max_byte_len;
	unsigned int *p_read_int;
	unsigned char *p_ecc_error;

	INFO("[%s] read %d bytes\n", otp_type_str[type], rlen);

	// data length should be 3 aligned
	byte_len = bytes_3aligned(rlen);

	int_len = byte_len / 3;

	switch(type) {
		case OTP_BOUND:
			max_byte_len = EFUSE_BOUND_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE;
		break;

		case OTP_CPUID:
			max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM);
		break;

		case OTP_UUID:
			max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM)
				+ caculate_need_bytes(EFUSE_CPUID_MAX_BYTES_NUM);
		break;

		default:
			ERROR("efuse_read_otp: Not support otp type %s\n", otp_type_str[type]);
		return -1;
	}

	if(rlen > max_byte_len) {
		ERROR("efuse_read_otp(type: %s): length = %d, it should be <= %d\n",
			otp_type_str[type], rlen, max_byte_len);
		return -1;
	}

	if(start + int_len * 4 > 128) {
		ERROR("efuse_read_otp(type: %s): start + int_len * 4 = %d, it should be <= 128\n",
			otp_type_str[type], start + int_len * 4);
		return -1;
	}

	p_read_int = (unsigned int *)kzalloc(int_len * 4, GFP_KERNEL);
	if (p_read_int == NULL) {
		ERROR("efuse_read_otp(type: %s): kzalloc p_read_int buffer fail\n", otp_type_str[type]);
		return -1;
	}

	p_ecc_error = (unsigned char *)kzalloc(int_len * 4, GFP_KERNEL);
	if(p_ecc_error == NULL) {
		ERROR("efuse_read_otp(type: %s): kzalloc p_ecc_error buffer fail\n", otp_type_str[type]);
		kfree(p_read_int);
		return -1;
	}

	ret = efuse_hamming_read_int(start, p_read_int, p_ecc_error, int_len);
	if(ret != int_len) {
		ERROR("efuse_read_otp(type: %s): efuse_hamming_read_int fail\n", otp_type_str[type]);
		kfree(p_read_int);
		kfree(p_ecc_error);
		return -1;
	}

	INFO("efuse_read_otp: read %d integer\n", int_len);
	for(i = 0; i < int_len; i++)
		if(p_ecc_error[i] == 0)
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));
		else
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X (ECC Error Detected)\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));

	for(i = 0; i < rlen; i++)
		rbuf[i] = (p_read_int[i / 3] >> ((i % 3) * 8)) & 0xFF;

	print_write_read_bytes(1, NULL, rbuf, rlen);

	kfree(p_read_int);
	kfree(p_ecc_error);

	return rlen;
}
EXPORT_SYMBOL_GPL(efuse_read_otp);

static ssize_t register_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "VDD25 Level:                   %d\n", vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1);
	len += sprintf(buf + len, "EFUSE_MODE:                    0x%08X\n", EFUSE_MODE_VAL);
	len += sprintf(buf + len, "EFUSE_ADR:                     0x%08X\n", EFUSE_ADR_VAL);
	len += sprintf(buf + len, "EFUSE_DIR_CMD:                 0x%08X\n", EFUSE_DIR_CMD_VAL);
	len += sprintf(buf + len, "EFUSE_RD_DATA:                 0x%08X\n", EFUSE_RD_DATA_VAL);
	len += sprintf(buf + len, "EFUSE_ECCSRAM_ADR:             0x%08X\n", EFUSE_ECCSRAM_ADR_VAL);
	len += sprintf(buf + len, "EFUSE_ECCSRAM_RDPORT:          0x%08X\n", EFUSE_ECCSRAM_RDPORT_VAL);
	len += sprintf(buf + len, "EFUSE_ECC_STATUS_0:            0x%08X\n", EFUSE_ECC_STATUS_0_VAL);
	len += sprintf(buf + len, "EFUSE_ECC_STATUS_1:            0x%08X\n", EFUSE_ECC_STATUS_1_VAL);

	len += sprintf(buf + len, "\n");

	if(vdd25_control) {
		if(vdd25_control_pin.gpiono <= WMT_PIN_GP0_GPIO7) {
			len += sprintf(buf + len, "GPIO_GP0_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0000));
			len += sprintf(buf + len, "GPIO_GP0_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0040));
			len += sprintf(buf + len, "GPIO_GP0_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0080));
			len += sprintf(buf + len, "GPIO_GP0_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C0));
			len += sprintf(buf + len, "GPIO_GP0_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0480));
			len += sprintf(buf + len, "GPIO_GP0_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C0));
		} else if(vdd25_control_pin.gpiono <= WMT_PIN_GP1_GPIO15) {
			len += sprintf(buf + len, "GPIO_GP1_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0001));
			len += sprintf(buf + len, "GPIO_GP1_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0041));
			len += sprintf(buf + len, "GPIO_GP1_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0081));
			len += sprintf(buf + len, "GPIO_GP1_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C1));
			len += sprintf(buf + len, "GPIO_GP1_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0481));
			len += sprintf(buf + len, "GPIO_GP1_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C1));
		} else if(vdd25_control_pin.gpiono <= WMT_PIN_GP2_GPIO19) {
			len += sprintf(buf + len, "GPIO_GP2_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0002));
			len += sprintf(buf + len, "GPIO_GP2_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0042));
			len += sprintf(buf + len, "GPIO_GP2_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0082));
			len += sprintf(buf + len, "GPIO_GP2_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C2));
			len += sprintf(buf + len, "GPIO_GP2_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0482));
			len += sprintf(buf + len, "GPIO_GP2_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C2));
		}
	}

	buf[len++] = 0;

	return len;
}

static ssize_t raw_data_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{

	int i, len;
	unsigned char read_bytes[128];

	efuse_read_bytes(0, read_bytes, 128);

	len = 0;

	len += sprintf(buf + len, "efuse raw data:");
	for(i = 0; i < 128; i++) {

		if(i % 16 == 0)
			len += sprintf(buf + len, "\n");

		if(i != 128 - 1)
			len += sprintf(buf + len, "0x%02X,", *(read_bytes + i));
		else
			len += sprintf(buf + len, "0x%02X\n", *(read_bytes + i));
	}

	buf[len++] = 0;

	return len;
}

static ssize_t available_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int i;

	unsigned char read_bytes[128];

	efuse_read_bytes(0, read_bytes, 128);

	for(i = 8; i < 128; i++) {
		if(read_bytes[i] == 0)
			break;
	}

	if(i != 128)
		return sprintf(buf, "Byte %d is available for writing\n", i);
	else
		return sprintf(buf, "No Byte is available for writing\n");
}

static ssize_t hamming_data_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int i, len;
	unsigned int read_int[30] = {0}; // 120 / 4 = 30
	unsigned char ecc_error[30] = {0};

	efuse_hamming_read_int(8, read_int, ecc_error, 30);

	len = 0;

	len += sprintf(buf + len, "\nefuse hamming data:\n");
	for(i = 0; i < 30; i++) {
		len += sprintf(buf + len, "Byte%3d ~ Byte%3d: 0x%08X",
			8 + i * 4, 8 + i * 4 + 3, read_int[i]);
		if(ecc_error[i] == 0)
			len += sprintf(buf + len, "\n");
		else
			len += sprintf(buf + len, " ECC Error Detected\n");
	}
	len += sprintf(buf + len, "\n");

	buf[len++] = 0;

	return len;
}

static ssize_t cpuid_show(struct class *class,
			struct class_attribute *attr,
			char *buf)
{
	int i, len = 0;
	unsigned char read_bytes[EFUSE_CPUID_MAX_BYTES_NUM] = {0};

	efuse_read_otp(OTP_CPUID, read_bytes, EFUSE_CPUID_MAX_BYTES_NUM);

	for(i = EFUSE_CPUID_MAX_BYTES_NUM - 1; i >= 0; i--)
		len += sprintf(buf + len, "%02X", read_bytes[i]);

	len += sprintf(buf + len, "\n");

	buf[len++] = 0;

	return len;
}

#ifdef EFUSE_WRITE_THROUGH_SYSFS

static ssize_t byte_read_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long val;
	int byte_no;
	unsigned char read_byte;

	if(count == 0 || *buf < '0' || *buf > '9') {
		ERROR("invaild byte_no. buf = %s", buf);
		return count;
	}

	if (strict_strtoul(buf, 0, &val)) {
		ERROR("analyze byte_no fail. buf = %s", buf);
		return count;
	}

	byte_no = (int)val;

	if(byte_no > 127 || byte_no < 0) {
		ERROR("invalid byte_no = %d, byte_no should be 0 ~ 127\n", byte_no);
		return count;
	}

	efuse_read_bytes(byte_no, &read_byte, 1);

	INFO("Byte%d = 0x%02X\n", byte_no, read_byte);

	return count;
}

static ssize_t byte_write_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret, start_no;
	int i, byte_num;
	unsigned long value;
	unsigned char *p_write_bytes;
	const char *p;
	char *endp;

	if(count == 0 || *buf < '0' || *buf > '9') {
		ERROR("invaild start_no. buf = %s", buf);
		return count;
	}

	p = buf;
	start_no = simple_strtoul(p, &endp, 0);

	if(start_no > 127 || start_no < 8) {
		ERROR("invalid start_no = %d, start_no should be 8 ~ 127\n", start_no);
		return count;
	}

	if(*endp == '\0' || *endp == '\n') {
		ERROR("wrong input format. format should be \"start_no val1,val2,...\"\n");
		return count;
	}

	p_write_bytes = (unsigned char *)kzalloc(128, GFP_KERNEL);
	if(p_write_bytes == NULL) {
		ERROR("write_byte_store: kzalloc fail\n");
		return count;
	}

	p = endp + 1;
	byte_num = 0;
	for(i = 0; i < 128; i++) {
		if(*p < '0' || *p > '9') {
			ERROR("wrong input format. wrong str1 = \"%s\"\n", p);
			kfree(p_write_bytes);
			return count;
		}

		value = simple_strtoul(p, &endp, 0);
		if(value > 0xFF) {
			ERROR("byte_val = 0x%lx > 0xFF. wrong str = \"%s\"\n", value,  p);
			kfree(p_write_bytes);
			return count;
		}

		*(p_write_bytes + i) = value & 0xFF;

		byte_num++;

		if(*endp == '\0' || *endp == '\n')
			break;
		else if(*endp != ',') {
			ERROR("wrong input format. wrong str2 = \"%s\"\n", endp);
			kfree(p_write_bytes);
			return count;
		}

		p = endp + 1;
	}

	if(start_no + byte_num > 128) {
		ERROR("start_no = %d, byte_num = %d, start_no + byte_num > 128\n", start_no, byte_num);
		kfree(p_write_bytes);
		return count;
	}

	INFO("start_no: %d, byte_num = %d\n", start_no, byte_num);
	print_write_read_bytes(1, p_write_bytes, NULL, byte_num);

	ret = efuse_write_bytes(start_no, p_write_bytes, byte_num);
	if(ret == byte_num)
		INFO("eFuse Write Success !!!\n");

	kfree(p_write_bytes);

	return count;
}

static ssize_t hamming_write_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret, start_no;
	const char *p;
	char *endp;
	unsigned long value;

	if(count == 0 || *buf < '0' || *buf > '9') {
		ERROR("invaild start_no. buf = %s", buf);
		return count;
	}

	p = buf;
	start_no = simple_strtoul(p, &endp, 0);

	if(*endp == '\0' || *endp == '\n') {
		ERROR("wrong input format. format should be \"start_no value\"\n");
		return count;
	}

	p = endp + 1;

	ret = strict_strtoul(p, 0, &value);
	if(ret) {
		ERROR("invalid value: %s\n", p);
		return count;
	}

	efuse_hamming_write_int(start_no, (unsigned int *)&value, 1);

	return count;
}

static ssize_t hamming_read_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long val;
	int byte_no;
	unsigned int read_int = 0;
	unsigned char ecc_error = 0;

	if(count == 0 || *buf < '0' || *buf > '9') {
		ERROR("invaild byte_no. buf = %s", buf);
		return count;
	}

	if (strict_strtoul(buf, 0, &val)) {
		ERROR("analyze byte_no fail. buf = %s", buf);
		return count;
	}

	byte_no = (int)val;

	if(byte_no > 127 || byte_no < 8) {
		ERROR("invalid byte_no = %d, byte_no should be 8 ~ 127\n", byte_no);
		return -1;
	}

	if(byte_no % 4 != 0) {
		ERROR("invalid byte_no = %d, byte_no should be 4 aligned\n", byte_no);
		return -1;
	}

	efuse_hamming_read_int(byte_no, &read_int, &ecc_error, 1);

	INFO("Hamming read: Byte%d ~ Byte%d, val: 0x%08X\n",
		byte_no, byte_no + 3, read_int);


	return count;
}

static ssize_t otp_write_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	int i, byte_num, offset = 0, max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
	OTP_TYPE otp_type = OTP_CPUID;
	const char *p = buf;
	char *endp;
	unsigned char *p_write_bytes;
	unsigned long value;

	if(strnicmp(buf, "cpuid", 5) == 0) {
		otp_type = OTP_CPUID;
		max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
		offset = 5;
	} else if(strnicmp(buf, "uuid", 4) == 0) {
		otp_type = OTP_UUID;
		max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
		offset = 4;
	} else {
		ERROR("wrong descriptor. format should be \"cpuid(uuid) val1,val2,...\"\n");
		return count;
	}

	p = p + offset;

	if(*p == '\0' || *p == '\n') {
		ERROR("wrong input format. format should be \"cpuid(uuid) val1,val2,...\"\n");
		return count;
	}

	p_write_bytes = (unsigned char *)kzalloc(128, GFP_KERNEL);
	if(p_write_bytes == NULL) {
		ERROR("write_otp_store: kzalloc fail\n");
		return count;
	}

	p++;
	byte_num = 0;

	for(i = 0; i < 128; i++) {
		if(*p < '0' || *p > '9') {
			ERROR("wrong input format. wrong str1 = \"%s\"\n", p);
			kfree(p_write_bytes);
			return count;
		}

		value = simple_strtoul(p, &endp, 0);
		if(value > 0xFF) {
			ERROR("byte_val = 0x%lx > 0xFF. wrong str = \"%s\"\n", value,  p);
			kfree(p_write_bytes);
			return count;
		}

		*(p_write_bytes + i) = value & 0xFF;

		byte_num++;

		if(*endp == '\0' || *endp == '\n')
			break;
		else if(*endp != ',') {
			ERROR("wrong input format. wrong str2 = \"%s\"\n", endp);
			kfree(p_write_bytes);
			return count;
		}

		p = endp + 1;
	}

	if(byte_num > max_byte_len) {
		ERROR("byte_num = %d, %s byte_num can't larger than %d\n",
			byte_num, otp_type_str[otp_type], max_byte_len);
		kfree(p_write_bytes);
		return count;
	}

	efuse_write_otp(otp_type, p_write_bytes, byte_num);

	kfree(p_write_bytes);

	return count;
}

static ssize_t otp_read_store(struct class *class,
			struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret, byte_num, offset = 0, max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
	OTP_TYPE otp_type = OTP_CPUID;
	const char *p = buf;
	unsigned char *p_read_bytes;
	unsigned long value;

	if(strnicmp(buf, "bound", 5) == 0) {
		otp_type = OTP_BOUND;
		max_byte_len = EFUSE_BOUND_MAX_BYTES_NUM;
		offset = 5;
	}else if(strnicmp(buf, "cpuid", 5) == 0) {
		otp_type = OTP_CPUID;
		max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
		offset = 5;
	} else if(strnicmp(buf, "uuid", 4) == 0) {
		otp_type = OTP_UUID;
		max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
		offset = 4;
	} else {
		ERROR("wrong descriptor. format should be \"cpuid(uuid) bytenum\"\n");
		return count;
	}

	p = p + offset;

	if(*p == '\0' || *p == '\n') {
		ERROR("wrong input format. format should be \"cpuid(uuid) bytenum\"\n");
		return count;
	}

	p++;

	ret = strict_strtoul(p, 0, &value);
	if(ret) {
		ERROR("invalid value: %s\n", p);
		return count;
	}

	byte_num = (int)value;

	if(byte_num <= 0) {
		ERROR("byte_num = %d, %s byte_num should be larger than 0\n",
			byte_num, otp_type_str[otp_type]);
		return count;
	}

	if(byte_num > max_byte_len) {
		ERROR("byte_num = %d, %s byte_num can't larger than %d\n",
			byte_num,otp_type_str[otp_type], max_byte_len);
		return count;
	}

	p_read_bytes = (unsigned char *)kzalloc(byte_num, GFP_KERNEL);
	if(p_read_bytes == NULL) {
		ERROR("read_otp_store: kzalloc fail\n");
		return count;
	}

	efuse_read_otp(otp_type, p_read_bytes, byte_num);

	kfree(p_read_bytes);

	return count;
}

#endif

static struct class_attribute efuse_class_attrs[] = {
#ifdef EFUSE_WRITE_THROUGH_SYSFS
	__ATTR(byte_read, 0200, NULL, byte_read_store),
	__ATTR(byte_write, 0200, NULL, byte_write_store),
	__ATTR(hamming_read, 0200, NULL, hamming_read_store),
	__ATTR(hamming_write, 0200, NULL, hamming_write_store),
	__ATTR(otp_read, 0200, NULL, otp_read_store),
	__ATTR(otp_write, 0200, NULL, otp_write_store),
#endif

	__ATTR_RO(register),
	__ATTR_RO(raw_data),
	__ATTR_RO(available),
	__ATTR_RO(hamming_data),
	__ATTR_RO(cpuid),

	__ATTR_NULL
};

static struct class efuse_class = {
	.name        = EFUSE_NAME,
	.owner       = THIS_MODULE,
	.class_attrs = efuse_class_attrs,
};


static int efuse_open(struct inode *inode, struct file *file)
{
	DBG("efuse_open\n");
	return 0;
}

static int efuse_release(struct inode *inode, struct file *file)
{
	DBG("efuse_release\n");
	return 0;
}

static ssize_t efuse_otp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned char *pdata;
	unsigned char type;
	OTP_TYPE otp_type;

	DBG("efuse_otp_read\n");

	if(count < 1) {
		ERROR("efuse_otp_read: count < 1\n");
		return -EINVAL;
    	}

	ret = copy_from_user(&type, buf, 1);
	if(ret) {
		ERROR("efuse_otp_read: copy_from_user fail\n");
		return -EFAULT;
	}

	if (type < OTP_BOUND || type > OTP_UUID) {
		ERROR("efuse_otp_read: invalid otp type: %d\n", type);
		return -EINVAL;
	}

	otp_type = (OTP_TYPE)type;

	pdata = kzalloc(count, GFP_KERNEL);
	if(!pdata) {
		ERROR("efuse_otp_read: alloc memory fail\n");
		return -ENOMEM;
	}

	ret = efuse_read_otp(otp_type, pdata, count);
	if(ret != count) {
		kfree(pdata);
		return -EIO;
	}

	ret = copy_to_user(buf, pdata, count);
	if(ret)	{
		ERROR("efuse_otp_read: copy_to_user fail\n");
		kfree(pdata);
		return -EFAULT;
	}

	kfree(pdata);
	return count;
}

static ssize_t efuse_otp_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int ret;
	unsigned char *pdata;
	unsigned char type;
	OTP_TYPE otp_type;

	DBG("efuse_otp_write\n");

	if(count < 2) {
	    ERROR("efuse_otp_write: count < 2\n");
	    return -EINVAL;
	}

	pdata = kzalloc(count, GFP_KERNEL);
	if(!pdata) {
	    ERROR("efuse_otp_write: alloc memory fail\n");
	    return -ENOMEM;
	}

	ret = copy_from_user(pdata, buf, count);
	if(ret) {
		ERROR("efuse_otp_write: copy_from_user fail\n");
		kfree(pdata);
		return -EFAULT;
	}

	type = *pdata;
	if (type < OTP_BOUND || type > OTP_UUID) {
		ERROR("efuse_otp_write: invalid otp type: %d\n", type);
		return -EINVAL;
	}

	otp_type = (OTP_TYPE)type;

	ret = efuse_write_otp(otp_type, pdata + 1, count - 1);
	if(ret != (count - 1)) {
		kfree(pdata);
		return -EIO;
	}

	kfree(pdata);

	return count;
}

static long efuse_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	DBG("efuse_unlocked_ioctl\n");
	return 0;
}

static const struct file_operations efuse_fops = {
	.owner               = THIS_MODULE,
	.open                = efuse_open,
	.release             = efuse_release,
	.read                = efuse_otp_read,
	.write               = efuse_otp_write,
	.unlocked_ioctl      = efuse_unlocked_ioctl,
};

static int wmt_efuse_probe(struct platform_device *pdev)
{
	int ret;
	dev_t dev_no;
	unsigned char buf[40];
	int num, buflen = 40;
	VDD25_GPIO vdd25_gpio;

	INFO("wmt_efuse_probe\n");

	if(wmt_getsyspara(ENV_EFUSE_GPIO, buf, &buflen) == 0) {
		num = sscanf(buf, "%d:%d", &vdd25_gpio.gpiono, &vdd25_gpio.active);
		if(num == 2) {
			if(vdd25_gpio.gpiono <= WMT_PIN_GP63_SD2CD) {
				vdd25_control_pin.gpiono = vdd25_gpio.gpiono;
				vdd25_control_pin.active = vdd25_gpio.active;
				vdd25_control = 1;
			} else
				WARNING("wrong %s = %s. gpio_no = %d. It can't larger than %d\n",
					ENV_EFUSE_GPIO, buf, vdd25_gpio.gpiono, WMT_PIN_GP63_SD2CD);
		} else
			WARNING("wrong %s = %s. The param's num = %d. It should be equal to 2\n",
				ENV_EFUSE_GPIO, buf, num);
	}

	if(vdd25_control) {
		ret = gpio_request(vdd25_control_pin.gpiono, "efuse-vdd");
		if(ret) {
			ERROR("gpio(%d) request fail for efuse-vdd\n", vdd25_control_pin.gpiono);
			return ret;
		}

		efuse_vdd25_active(0);
		if(vdd25_control_pin.active)
			wmt_gpio_setpull(vdd25_control_pin.gpiono, WMT_GPIO_PULL_DOWN);
		else
			wmt_gpio_setpull(vdd25_control_pin.gpiono, WMT_GPIO_PULL_UP);
	}

	/*
	* create control files in sysfs
	* /sys/class/efuse/
	*/
	ret = class_register(&efuse_class);
	if(ret) {
		ERROR("register efuse_class fail\n");
		if(vdd25_control)
			gpio_free(vdd25_control_pin.gpiono);

		return -EFAULT;
	}

	efuse_major = register_chrdev(0, EFUSE_NAME, &efuse_fops);
	if(efuse_major < 0) {
		ERROR("get efuse_major fail\n");
		class_unregister(&efuse_class);
		if(vdd25_control)
			gpio_free(vdd25_control_pin.gpiono);

		return -EFAULT;
	}

	INFO("mknod /dev/%s c %d 0\n", EFUSE_NAME, efuse_major);

	dev_no = MKDEV(efuse_major, 0);
	p_efuse_device = device_create(&efuse_class, NULL, dev_no, NULL, EFUSE_NAME);
	if (IS_ERR(p_efuse_device)) {
		ERROR("create efuse device fail");
		unregister_chrdev(efuse_major, EFUSE_NAME);
		class_unregister(&efuse_class);
		if(vdd25_control)
			gpio_free(vdd25_control_pin.gpiono);

		return PTR_ERR(p_efuse_device);
	}

	/* Disable Hardware ECC */
	//EFUSE_MODE_VAL &= ~EFUSE_ECC_EN;

	return 0;
}

static int wmt_efuse_remove(struct platform_device *pdev)
{
	dev_t dev_no;

	INFO("wmt_efuse_remove\n");

	dev_no = MKDEV(efuse_major, 0);
	device_destroy(&efuse_class, dev_no);
	unregister_chrdev(efuse_major, EFUSE_NAME);
	class_unregister(&efuse_class);
	if(vdd25_control)
		gpio_free(vdd25_control_pin.gpiono);

	return 0;
}

static int wmt_efuse_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	return 0;
}

static int wmt_efuse_resume(struct platform_device *pdev)
{
	INFO("wmt_efuse_resume\n");

	if(vdd25_control) {
		gpio_re_enabled(vdd25_control_pin.gpiono);
		efuse_vdd25_active(0);
		if(vdd25_control_pin.active)
			wmt_gpio_setpull(vdd25_control_pin.gpiono, WMT_GPIO_PULL_DOWN);
		else
			wmt_gpio_setpull(vdd25_control_pin.gpiono, WMT_GPIO_PULL_UP);
	}

	return 0;
}

static struct platform_driver wmt_efuse_driver = {
	.probe      = wmt_efuse_probe,
	.remove     = wmt_efuse_remove,
	.suspend    = wmt_efuse_suspend,
	.resume     = wmt_efuse_resume,

	.driver = {
		.name = EFUSE_NAME,
		.owner = THIS_MODULE,
	},
};

static void wmt_efuse_release(
	struct device *device
)
{
	INFO("wmt_efuse_release\n");
	return;
}

static struct platform_device wmt_efuse_device = {
    .name           = EFUSE_NAME,
    .id             = 0,
    .dev            = {
        .release =    wmt_efuse_release,
    },
    .num_resources	= 0,
    .resource		= NULL,
};

static int __init wmt_efuse_init(void)
{
	int ret;

	INFO("wmt_efuse_init\n");

	ret = platform_device_register(&wmt_efuse_device);
	if(ret) {
		 ERROR("can't register eFuse device\n");
		 return -ENODEV;
	}

	ret = platform_driver_register(&wmt_efuse_driver);
	if(ret) {
		ERROR("can't register eFuse driver\n");
		platform_device_unregister(&wmt_efuse_device);
		return -ENODEV;
	}

	return 0;
}

static void __exit wmt_efuse_exit(void)
{
	INFO("wmt_efuse_exit\n");

	platform_driver_unregister(&wmt_efuse_driver);
	platform_device_unregister(&wmt_efuse_device);

	return;
}

module_init(wmt_efuse_init);
module_exit(wmt_efuse_exit);

MODULE_DESCRIPTION("WM8880 eFuse driver");
MODULE_AUTHOR("WMT ShenZhen Driver Team");
MODULE_LICENSE("GPL");

