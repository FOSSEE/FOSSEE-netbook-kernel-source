#ifndef _LW86X0_TS_H_
#define _LW86X0_TS_H_

//#include "wmt_custom_lw86x0.h"

// define byte swap of a WORD
#define swap16(a) ((((a)&0xff)<<8)|(((a)>>8)&0xff))

//struct _reg_word for ioctl read or write register
#define LW86X0_NAME	"touch_lw86x0"

#define SUPPORT_FW_UPGRADE
#define TS_KEY_NUM  4
#define COL_NUM_MAX 28
#define ROW_NUM_MAX 16
#define SUPPORT_POINT_NUM_MAX 10
#define MULTI_DATA_MAX_SIZE 49

typedef struct _reg_word
{
    u16 uOffset;
    u16 uValue;
    u16 multi_data[MULTI_DATA_MAX_SIZE];
    int data_size;
}reg_word;

//struct _flash_op for ioctl write or read frimware
#define FLASH_XFER_PKT_SIZE 256
typedef struct _flash_op
{
    u16 startaddr; //=0 if the first pkt
    u16 lastpkt;   // =1 if last pkt; =0, otherwise
    u16 pktlen;    //data length in this pkt
    char data[FLASH_XFER_PKT_SIZE];
}flash_op;

//struct _raw_data for ioctl read cdc/amb/diff data
typedef struct _raw_data
{
    u8 row;
    u8 col;
    u16 data[COL_NUM_MAX*ROW_NUM_MAX];
}rawdata;

extern void wmt_ts_set_keylen(int keylen);
extern void wmt_ts_set_baseaxis(int axis);
extern void wmt_ts_set_keypos(int index, int min,int max);
extern int lw86x0_write_reg(u16 addr, u16 value);
extern int lw86x0_read_reg(u16 addr, u16 *pdata, int regcnt);
extern void getversion(void);
extern void lw86x0_stop_timer(int flags);

#endif
