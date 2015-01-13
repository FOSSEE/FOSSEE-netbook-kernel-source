#ifndef __NMI_HOST_AP__
#define __NMI_HOST_AP__

#include "nmi_wlan_if.h"
#include "nmi_wlan.h"
#include "host_interface.h"

#ifdef NMI_FULLY_HOSTING_AP
/*!  
*  @file	NMI_host_AP.h
*  @brief	code related to AP fully hosting mode on NMI driver
*  @author	asobhy
*  @date	09 APRIL 2013
*  @version	1.0
*/




#define ITIM                			 5   /* Traffic Information Map        */
#define DEFAULT_TIM_LEN        	 4
#define FCS_LEN                 		 4
#define VBMAP_SIZE              		 256


#define WIFI_PERIPH_BASE		0x00000000
#define WIFI_PA_BASE			(WIFI_PERIPH_BASE+0x9800)
#define PA_BASE 				WIFI_PA_BASE

#define rMAC_DTIM_COUNT_ADDR						(( NMI_Uint32 )(PA_BASE + 0x021C))
#define rMAC_BEACON_PERIOD           					(( NMI_Uint32 )(PA_BASE + 0x020C))
#define rMAC_DTIM_PERIOD              					(( NMI_Uint32 )(PA_BASE + 0x0210))
#define rMAC_TSF_CON                 				 		(( NMI_Uint32 )(PA_BASE + 0x0200))



typedef enum {TYPE_OFFSET        = 0,
              LENGTH_OFFSET      = 1,
              DTIM_CNT_OFFSET    = 2,
              DTIM_PERIOD_OFFSET = 3,
              BMAP_CTRL_OFFSET   = 4,
              TIM_OFFSET         = 5
} OFFSET_T;
	
typedef struct {
	int size;
	void* buff;
}beacon_data;

typedef struct {
	NMI_Uint16		u16beacon_len;
	NMI_Uint8             *u8beacon_frame;
	NMI_Uint8		u8tim_element_index;
	NMI_Uint16      	u16tim_element_trailer_len;
	NMI_Uint8       	u8vbmap[VBMAP_SIZE];
	NMI_Uint8             	u8DTIMPeriod;
	NMI_Uint16            u16Beacon_Period;
}beacon_info;

typedef struct {
	int quit;

	/**
		input interface functions
	**/
	nmi_wlan_os_func_t os_func;
	nmi_wlan_io_func_t io_func;
	nmi_wlan_net_func_t net_func;
	nmi_wlan_indicate_func_t indicate_func;

	/**
		host interface functions
	**/
	nmi_hif_func_t hif_func;
	void *hif_lock;

	/**
		configuration interface functions
	**/
	nmi_cfg_func_t cif_func;
	int cfg_frame_in_use;
	nmi_cfg_frame_t cfg_frame;
	uint32_t cfg_frame_offset;
	int cfg_seq_no;
	void *cfg_wait;

	/**
		RX buffer
	**/
	uint32_t rx_buffer_size;
	//uint8_t *rx_buffer;
	//uint32_t rx_buffer_offset;

	/**
		TX buffer
	**/
	uint32_t tx_buffer_size;
	uint8_t *tx_buffer;
	uint32_t tx_buffer_offset;
	
	/**
		TX queue
	**/
	void *txq_lock;		
	struct txq_entry_t *txq_head;
	struct txq_entry_t *txq_tail;
	int txq_entries;
	void *txq_wait;
	int txq_exit;

	/**
		RX queue
	**/
	void *rxq_lock;		
	struct rxq_entry_t *rxq_head;
	struct rxq_entry_t *rxq_tail;	
	int rxq_entries;
	void *rxq_wait;
	int rxq_exit;

#if DMA_VER == DMA_VER_2
	int use_dma_v2; 
#endif
} nmi_wlan_dev_t;


 NMI_Sint32 host_add_beacon(NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32Interval,
									 NMI_Uint32 u32DTIMPeriod,
									 NMI_Uint32 u32HeadLen, NMI_Uint8* pu8Head,
									 NMI_Uint32 u32TailLen, NMI_Uint8* pu8Tail);

 NMI_Sint32 host_del_beacon(NMI_WFIDrvHandle hWFIDrv);

 void process_tbtt_isr(void);

 nmi_wlan_dev_t* Get_wlan_context(NMI_Uint16* pu16size);

 #ifdef NMI_AP_EXTERNAL_MLME
int nmi_wlan_txq_add_mgmt_pkt(void *priv, uint8_t *buffer, uint32_t buffer_size, nmi_tx_complete_func_t func);
#endif


#endif //  NMI_FULLY_HOSTING_AP

#endif 
