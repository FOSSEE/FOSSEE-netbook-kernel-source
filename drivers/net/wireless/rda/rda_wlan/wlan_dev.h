#ifndef __WLAN_DEV_H__
#define __WLAN_DEV_H__

#ifndef MAX_WPA_IE_LEN
#define MAX_WPA_IE_LEN 100
#endif

#ifndef MAX_WPS_IE_LEN
#define MAX_WPS_IE_LEN (512)
#endif
#ifndef MAX_RATES
#define MAX_RATES           14
#endif

#define WLAN_MAX_NETWORK_NUM	64

#define KEY_LEN_WPA_AES         16
#define KEY_LEN_WPA_TKIP        32
#define KEY_LEN_WEP_104         13
#define KEY_LEN_WEP_40          5

#define ASSOC_FLAG_SSID         1
#define ASSOC_FLAG_CHANNEL      2
#define ASSOC_FLAG_BAND         3
#define ASSOC_FLAG_MODE         4
#define ASSOC_FLAG_BSSID        5
#define ASSOC_FLAG_WEP_KEYS     6
#define ASSOC_FLAG_WEP_TX_KEYIDX    7
#define ASSOC_FLAG_WPA_MCAST_KEY    8
#define ASSOC_FLAG_WPA_UCAST_KEY    9
#define ASSOC_FLAG_SECINFO      10
#define ASSOC_FLAG_WPA_IE       11
#define ASSOC_FLAG_ASSOC_RETRY      12
#define ASSOC_FLAG_ASSOC_START  13
#define ASSOC_FLAG_WLAN_CONNECTING  14

#define WLAN_NF_DEFAULT_SCAN_VALUE           (-96)
#define PERFECT_RSSI ((u8)50)
#define WORST_RSSI   ((u8)0)
#define RSSI_DIFF    ((u8)(PERFECT_RSSI - WORST_RSSI))

#define WLAN_RTS_MIN_VALUE           0
#define WLAN_RTS_MAX_VALUE           2347
#define WLAN_FRAG_MIN_VALUE          256
#define WLAN_FRAG_MAX_VALUE          2346


#define WLAN_AUTH_OPEN 0
#define WLAN_AUTH_SHARED_KEY 1
#define WLAN_AUTH_FT 2
#define WLAN_AUTH_LEAP 128

#define WLAN_AUTH_CHALLENGE_LEN 128

#define WLAN_CAPABILITY_ESS     (1<<0)
#define WLAN_CAPABILITY_IBSS        (1<<1)
#define WLAN_CAPABILITY_CF_POLLABLE (1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST (1<<3)
#define WLAN_CAPABILITY_PRIVACY     (1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE  (1<<5)
#define WLAN_CAPABILITY_PBCC        (1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY (1<<7)
#define IW_AUTH_ALG_WAPI      0x08
#define IW_ENCODE_ALG_WAPI    0x80
#define IW_AUTH_WAPI_ENABLED        0x20

#define WAPI_KEY_MGMT_NONE 0
#define WAPI_KEY_MGMT_CERT BIT2
#define WAPI_KEY_MGMT_PSK BIT3

#define IW_ENCODE_ALG_SM4   0x20

enum WLAN_SCAN_STATUS{
    WLAN_SCAN_IDLE = 0,
    WLAN_SCAN_RUNNING = 1,
    WLAN_SCAN_COMPLET = 2
};

enum WLAN_PACKET_TYPE{
    WLAN_CMD = 1,
    WLAN_DATA = 2
};

/** KEY_TYPE_ID */
enum KEY_TYPE_ID {
    KEY_TYPE_ID_WEP = 0,
    KEY_TYPE_ID_TKIP,
    KEY_TYPE_ID_AES
};

enum PACKET_TYPE{
    WID_REQUEST_PACKET,
    WID_REQUEST_POLLING_PACKET,
    DATA_REQUEST_PACKET
};

/** KEY_INFO_WPA (applies to both TKIP and AES/CCMP) */
enum KEY_INFO_WPA {
    KEY_INFO_WPA_MCAST = 0x01,
    KEY_INFO_WPA_UNICAST = 0x02,
    KEY_INFO_WPA_ENABLED = 0x04
};

typedef struct _wlan_bss_descriptor {
    u8 ssid[IW_ESSID_MAX_SIZE + 1];
    u8 bss_type;
    u8 channel;
    u8 dot11i_info;
    u8 bssid[ETH_ALEN];
    u8 rssi;
    u8 auth_info;
    u8 rsn_cap[2];
}wlan_bss_descriptor;

/**
 *  @brief Structure used to store information for each beacon/probe response
 */
struct bss_descriptor {
    u8 bssid[ETH_ALEN];
    u8 ssid[IW_ESSID_MAX_SIZE + 1];
    u8 ssid_len;

    u16 capability;
    u32 rssi;
    u32 channel;
    u16 beaconperiod;

    /* IW_MODE_AUTO, IW_MODE_ADHOC, IW_MODE_INFRA */
    u8 mode;

    /* zero-terminated array of supported data rates */
    u8 rates[MAX_RATES + 1];    
	u8* wpa_ie;
	size_t wpa_ie_len;
	u8* rsn_ie;
	size_t rsn_ie_len;

	u8*    wapi_ie;
	size_t wapi_ie_len; //wapi valid payload length

	//huanglei add wps
	u8 *wps_ie;//added in probe req
	int wps_ie_len;

	unsigned long last_scanned;
    struct list_head list;
};

struct wlan_802_11_security {
    u8 WPAenabled;
    u8 WPA2enabled;
    u8 wep_enabled;
    u8 auth_mode;
    u32 key_mgmt;
    u32 cipther_type;
};

typedef struct _wlan_rx_packet_node{
    struct list_head List;
    struct sk_buff *Skb;
}wlan_rx_packet_node;

typedef struct _wlan_wid_packet_node{
    struct list_head List;
    wait_queue_head_t WidDone;
    u8 WidWaitOption;
    u16 WidCmd;
    u8 WidMsgId;
    u8 BufType;//use this to record the rsp type. in case we need use it to determine system big or little end
    u32 BufLen;
    u8  *Buf;
    u32 RspLen;
    u8  *RspBuf;
}wlan_wid_packet_node;

typedef struct _wlan_tx_packet_node{
    struct list_head List;
    struct sk_buff *Skb;
    u8 type;
    wlan_wid_packet_node * wid_node;
}wlan_tx_packet_node;

/* Generic structure to hold all key types. */
struct enc_key {
    u16 len;
    u16 flags;  /* KEY_INFO_* from defs.h */
    u16 type; /* KEY_TYPE_* from defs.h */
    u8 key[32];
};

typedef struct _wlan_private{
	struct mmc_card* MmcCard;
    u8 CardRemoved;
    int Open;
    int version;
#ifdef WLAN_SDIO_RESET_DEBUG	
	int debug_count;	
#endif

	u32 gpio_2_irq;
	u32 external_irq;
    
    struct dentry *debugfs_dir;
    struct dentry *debugfs_files[6];
    u8 wlan_pm_enable;

    struct net_device *netDev;
    u8     netDevRegistered;
    atomic_t netifQuStop;
	struct mmc_card * mmcCard;
    void *card;
    u32  SdioErrorCount;
	u8	sdio_need_reset;
	u8	 sdio_irq_enable;
    u8   IgnoreFisrtDisconnect;
    u8   Suspend;
    atomic_t   CardNeedSleep;
    u8   CardInSleep;
    WLAN_DRV_TIMER CardToSleepTimer;

	struct notifier_block pm_nb;
	u8 CardInSuspend;
	u8 earlysuspend_enabled;

	struct net_device_stats stats;
	spinlock_t TxLock;		 
	spinlock_t RxLock;	
	spinlock_t WidLock;
	spinlock_t EventLock;
	u8 CardSleepWakeLockOn;
	struct wake_lock CardSleepTimerLock;
	struct wake_lock MacStatusLock;
	struct wake_lock ExtIrqTimerLock;

	struct list_head AverRssiQ;
	u8 wid_msg_id;

	struct completion widComp;
	/** Free command buffers */
	struct list_head WidFreeQ;
	/** Pending command buffers */
	struct list_head WidPendingQ;

    /**rx packet queue*/
	u32 RxQuNum;
    struct list_head RxQueue;

    /**tx packet queue*/
    atomic_t TxQuNum;
    struct list_head TxQueue;

    /**tx packet queue*/
    u32 EventQuNum;
    struct list_head EventQueue;
	u32	EventErrorCount;

    /** thread to rx thread */
    wlan_thread RxThread;

    /** thread to event thread */
    wlan_thread EventThread;
    
    /** thread to tx  thread */
    wlan_thread TxThread;

    struct iw_statistics wstats;
    wlan_bss_descriptor curbssparams;
    int connect_status;
    int ToggalAssociation;
	u8 reassoc_count;
    u8 assoc_ongoing;
    u8 assoc_bssid[6];
    u8 assoc_ssid[IW_ESSID_MAX_SIZE + 1];
    u8 assoc_ssid_len;
    WLAN_DRV_TIMER StartAssociationTimeOut;
    WLAN_DRV_TIMER AssociationTimeOut;
    WLAN_DRV_TIMER ReAssociationTimeOut;
    /** Encryption parameter */
    u8 imode;
    u8 authtype;
    struct wlan_802_11_security secinfo;

    /** WEP keys */
    struct enc_key wep_keys[4];
    u16 wep_tx_keyidx;

    /** WPA keys */
    struct enc_key wpa_mcast_key;
    struct enc_key wpa_unicast_key;

    /** WPA Information Elements*/
    u8 wpa_ie[MAX_WPA_IE_LEN];
    u8 wpa_ie_len;

    u8 is_wapi;
	u8 wps_ie[MAX_WPS_IE_LEN];//added in probe req
	int wps_ie_len;
    
    /** Scan results list */
    int scan_running;
    WLAN_DRV_TIMER ScanResultsTimeout;
    spinlock_t ScanListLock; 
    struct list_head network_list;
    struct list_head network_free_list;
    struct bss_descriptor *networks;
    int scan_ssid_len;
    u8 scan_ssid[IW_ESSID_MAX_SIZE + 1];
}wlan_private;

typedef struct _wlan_sdio_card
{
    spinlock_t  cardLock;
    struct sdio_func    * func;
    wlan_private  * priv;
}wlan_sdio_card;

/*get mac from rda nvram*/
struct wlan_mac_info {
	u16 activated;
	u8 mac_addr[ETH_ALEN];
};
static inline int is_same_network(struct bss_descriptor *src,
                  struct bss_descriptor *dst)
{
    /* A network is only a duplicate if the channel, BSSID, and ESSID
    * all match.  We treat all <hidden> with the same BSSID and channel
    * as one network */
    return ((src->channel == dst->channel) &&
            !compare_ether_addr(src->bssid, dst->bssid) &&
            !memcmp(src->ssid, dst->ssid, IW_ESSID_MAX_SIZE));
}

static inline unsigned char is_zero_eth_addr(unsigned char *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

static inline void clear_bss_descriptor(struct bss_descriptor *bss)
{
    /* Don't blow away ->list, just BSS data */
	if(bss->wpa_ie)
		kfree(bss->wpa_ie);
		
	if(bss->rsn_ie)
		kfree(bss->wapi_ie);
	
	if(bss->wapi_ie)
		kfree(bss->wapi_ie);

	if(bss->wps_ie)
		kfree(bss->wapi_ie);
	
    memset(bss, 0, offsetof(struct bss_descriptor, list));
}

static inline int is_ap_support_11b(u8* rates)
{
    int i = 0;
    for(i = 0; i <= MAX_RATES; i ++){
        if(rates[i] == 0x96)
            return TRUE;
    }
    return FALSE;
}

static inline void skb_align(struct sk_buff *skb, int align)
{
    int off = ((unsigned long)skb->data) & (align - 1);

    if (off)
        skb_reserve(skb, align - off);
}

void if_sdio_interrupt(struct sdio_func *func);
struct bss_descriptor *get_bss_desc_from_scanlist(
        wlan_private *priv, unsigned char *bssid);
void wlan_network_information(wlan_private *priv, 
        unsigned char *info, unsigned short info_len);
void wlan_assocication(wlan_private* priv);
void wlan_re_assocication(wlan_private* priv);
void wlan_assocication_timeout(wlan_private* priv);
int wlan_card_control_init(wlan_private *priv);
void wlan_report_scan_result(wlan_private * priv);
int wlan_read_mac_from_file(char* buf);
int wlan_write_mac_to_file(char * buf);
int wlan_read_mac_from_nvram(char *buf);
int wlan_write_mac_to_nvram(const char *buf);
void wlan_indicate_disconnected(wlan_private *priv);
void wlan_set_scan_by_driver(wlan_private *priv);
//wlan_init
unsigned int wlan_extern_irq_handle(int irq, void *para);
int wlan_register_host_wake_irq(wlan_private* priv);
void wlan_unregister_host_wake_irq(wlan_private* priv);

int wlan_init(wlan_private* priv);
int wlan_add_card(wlan_sdio_card * card);
int wlan_start_card(wlan_sdio_card * card);
int wlan_reset_card(wlan_private *priv);
void wlan_release_dev(wlan_sdio_card * card);
void wlan_remove_tx_data_queue(wlan_private * priv);
void wlan_remove_rx_queue(wlan_private * priv);
void wlan_remove_event_queue(wlan_private * priv);
void wlan_unit(wlan_private * priv);
//wlan_rxtx
void wlan_process_event(wlan_private * priv);
void wlan_process_rx(wlan_private *priv);
int wlan_tx_thread(void *data);
int wlan_rx_thread(void *data);

//extern void rda_mmc_set_sdio_irq(u32 host_id, u8 enable);
extern struct iw_handler_def wlan_wext_handler_def;
extern unsigned char rda_combo_wifi_in_test_mode(void);
void wlan_debugfs_init(void);
void wlan_debugfs_remove(void);
void wlan_debugfs_init_all(wlan_private *priv);
void wlan_debugfs_remove_all(wlan_private *priv);
#endif

