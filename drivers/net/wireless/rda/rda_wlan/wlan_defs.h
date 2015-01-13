#ifndef _WLAN_DEFS_H_
#define _WLAN_DEFS_H_

#ifndef __KERNEL__
/** Character, 1 byte */
typedef char s8;
/** Unsigned character, 1 byte */
typedef unsigned char u8;

/** Short integer */
typedef signed short s16;
/** Unsigned short integer */
typedef unsigned short u16;

/** Long integer */
typedef signed long s32;
/** Unsigned long integer */
typedef unsigned long u32;

/** Long long integer */
typedef signed long long s64;
/** Unsigned long long integer */
typedef unsigned long long u64;
#endif

//#define NORMAL_FIXED
//#define WLAN_FLOW_CTRL_90E
#define WLAN_BIG_CURRENT_90E
//driver version 
#define WLAN_SDIOWIFI_VER_MAJ     0
#define WLAN_SDIOWIFI_VER_MIN     3
#define WLAN_SDIOWIFI_VER_BLD     1
//for not RDA PLATFORM,for example,Allwinner platform, don't define this 
//#define RDA_ANDROID_PLATFORM        1

#define WLAN_VERSION_90_D (1)
#define WLAN_VERSION_90_E (2)
#define WLAN_VERSION_91   (3)
#define WLAN_VERSION_91_E (4)
#define WLAN_VERSION_91_F (5)

//power manager
#define WLAN_SYS_SUSPEND
#define WLAN_POWER_MANAGER        (1)
#define WLAN_UNLOCK_SYSTEM          (1)
#define CARD_ENTER_SLEEP_TIMER (1000)
#define FLOW_CTRL_INT_SLEEP_RETRY_COUNT_91   25
#define FLOW_CTRL_INT_SLEEP_RETRY_COUNT_90  30
#define FLOW_CTRL_RXCMPL_RETRY_COUNT_91   30
#define FLOW_CTRL_RXCMPL_RETRY_COUNT_90   2000
#define SCAN_TIME_AT_EACH_CHANNEL      102

//#define WLAN_FORCE_SUSPEND_SUPPORT	       (1)
#define WIFI_SLEEP_LISTEN_INTERVAL         0xF0
#define WIFI_SLEEP_LINK_LOSS_THRESHOLD_90  0x0A
#define WIFI_SLEEP_LINK_LOSS_THRESHOLD_91  0x0A
#define DEFAULT_MAX_SCAN_AGE 	(25*HZ)
#define GET_SCAN_FROM_NETWORK_INFO  (1)
#define USE_MAC_DYNAMIC_ONCE      (1)
//#define USE_MAC_FROM_RDA_NVRAM
#define WIFI_MAC_ACTIVATED_FLAG    0x5990

#define DEFAULT_WATCHDOG_TIMEOUT (5 * HZ)
#define WID_HEADER_LEN (2)
#define CHECK_SDIO_STAUTS	(1)
#define WLAN_SDIO_MAX_ERR	(3)
#define WLAN_EVENT_MAX_ERR	(3)

//queue number form wid cmd & sdio received queue
#define WLAN_CMD_QUEUE_NUM  (10)
#define WLAN_RX_QUEUE_NUM     (10)
#define WLAN_TX_QUEUE_NUM_90     (10)
#define WLAN_TX_QUEUE_NUM_91     (100)
#define SDIO_MAX_BUFSZ             2048  /* Maximum size of a sdio dma buffer */

#ifndef TRUE
#define TRUE            (1)
#endif

#ifndef FALSE
#define FALSE           (0)
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#define BIT7                    (1 << 7)
#define BIT6                    (1 << 6)
#define BIT5                    (1 << 5)
#define BIT4                    (1 << 4)
#define BIT3                    (1 << 3)
#define BIT2                    (1 << 2)
#define BIT1                    (1 << 1)
#define BIT0                    (1 << 0)

#define CLEAR_BIT(X , Y)  (X) &= (~(Y))
#define SET_BIT(X , Y)    (X) |= (Y)

#define RDA_SLEEP_ENABLE        BIT0
#define RDA_SLEEP_PREASSO       BIT1
#define WIFI_LISTEN_INTERVAL            0x06
/* link_loss_threshold */
#define WIFI_LINK_LOSS_THRESHOLD_90        0x20
#define WIFI_LINK_LOSS_THRESHOLD_91        0x20
/* Link Sleep Threashold,old Value: 0x00A00080 */
#define WIFI_PREASSO_SLEEP              0x000500FF

extern int wlan_dbg_level;
extern int wlan_dbg_area;

typedef enum {
    WLAN_DL_ALL   = 0, 
    WLAN_DL_CRIT  = 1,
    WLAN_DL_TRACE = 2,
    WLAN_DL_NORM  = 3,
    WLAN_DL_DEBUG = 4,
    WLAN_DL_VERB  = 5,
} WLAN_DBG_LEVEL;

#define WLAN_DA_MAIN            (1 << 0)
#define WLAN_DA_SDIO            (1 << 1)
#define WLAN_DA_ETHER           (1 << 2)
#define WLAN_DA_WID             (1 << 3)
#define WLAN_DA_WEXT            (1 << 4)
#define WLAN_DA_TXRX            (1 << 5)
#define WLAN_DA_PM              (1 << 6)
#define WLAN_DA_ALL             0x0000007f

#define WLAN_LOG "WLAN: "

//#define DEBUG
//#define WLAN_RAW_DATA_DEBUG
#ifdef DEBUG 
#define WLAN_DBGLA(area, lvl)                                             \
    (((lvl) <= wlan_dbg_level) && ((area) & wlan_dbg_area))
#define WLAN_DBGLAP(area,lvl, x...)                                       \
    do{                                                                  \
        if (((lvl) <= wlan_dbg_level) && ((area) & wlan_dbg_area)) \
            printk(KERN_ERR WLAN_LOG x );                    \
    }while(0)
#define WLAN_DBGP(x...)                                                   \
    do{                                                                  \
        printk(KERN_ERR WLAN_LOG x );                            \
    }while(0)
#else
#define WLAN_DBGLA(area, lvl)    0
#define WLAN_DBGLAP(area,lvl, x...)  do {} while (0) 
#define WLAN_DBGP(x...)  do {} while (0) 
#endif

#define WLAN_ERRP(fmt, args...)                                          \
    do{                                                                 \
        printk(KERN_ERR WLAN_LOG "%s: "fmt, __func__, ## args ); \
    }while(0)

/** Log entry point for debugging */
#define ENTER()         WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_DEBUG, "Enter: %s :%i\n", __FUNCTION__, \
                            __LINE__)
/** Log exit point for debugging */
#define LEAVE()         WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_DEBUG, "Leave: %s :%i\n", __FUNCTION__, \
                            __LINE__)

#define WLAN_STATUS_FAILED (-1)
#define WLAN_STATUS_SUCCESS (0)
#endif


