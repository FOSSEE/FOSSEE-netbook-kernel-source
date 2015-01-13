#ifndef _RDA5890_IF_SDIO_H
#define _RDA5890_IF_SDIO_H
#include <linux/mmc/core.h>

#define IF_SDIO_SDIO2AHB_PKTLEN_L       0x00
#define IF_SDIO_SDIO2AHB_PKTLEN_H       0x01

#define IF_SDIO_AHB2SDIO_PKTLEN_L       0x02
#define IF_SDIO_AHB2SDIO_PKTLEN_H       0x03

#define IF_SDIO_FUN1_INT_MASK   0x04
#define IF_SDIO_FUN1_INT_PEND   0x05
#define IF_SDIO_FUN1_INT_STAT   0x06

#define   IF_SDIO_INT_AHB2SDIO  0x01
#define   IF_SDIO_INT_ERROR     0x04
#define   IF_SDIO_INT_SLEEP     0x10
#define   IF_SDIO_INT_AWAKE     0x20
#define   IF_SDIO_INT_RXCMPL    0x40
#define   IF_SDIO_HOST_TX_FLAG  0x80

#define IF_SDIO_FUN1_FIFO_WR    0x07
#define IF_SDIO_FUN1_FIFO_RD    0x08

#define IF_SDIO_FUN1_INT_TO_DEV 0x09

int wlan_card_check_sdio(wlan_private * priv);
int wlan_read_byte(wlan_private * priv, u32 addr, u8* data);
int wlan_write_byte(wlan_private * priv, u32 addr, u8 data);
int wlan_read_bytes(wlan_private * priv, u32 addr, u8* buf, u32 count);
int wlan_write_sdio_2_ahb(wlan_private * priv, u32 addr, u8* buf, u32 count);
int wlan_wake_up_card(wlan_private * priv);
int wlan_card_enter_sleep(wlan_private * priv);
void handle_card_to_sleep_cmd(wlan_private * priv);
int wlan_sdio_flow_ctrl_90(wlan_private * priv);
int wlan_sdio_flow_ctrl_91(wlan_private * priv);
int wlan_sdio_flow_ctrl_91e(wlan_private * priv);
int sdio_send_io_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr);
int sdio_select_card(struct mmc_host *host, struct mmc_card *card);
int sdio_send_relative_addr(struct mmc_host *host, unsigned int *rca);
#endif

