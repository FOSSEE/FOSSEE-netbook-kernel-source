#ifndef __WLAN_WID_H__
#define __WLAN_WID_H__

#define MAX_STRING_LEN      (256)
#define MAX_CMD_LEN         (MAX_STRING_LEN)
#define WLAN_MAX_WID_LEN    (MAX_CMD_LEN - 2)

#define MAC_CONNECTED          (1)
#define MAC_DISCONNECTED       (0)
#define WID_STATUS_SUCCESS       (1)

#define print_mac(x) printk("%x %x %x %x %x %x \n", x[0], x[1], x[2],x[3], x[4], x[5])

typedef enum
{
    G_SHORT_PREAMBLE = 0,
    G_LONG_PREAMBLE = 1,
    G_AUTO_PREAMBLE = 2
}G_PREAMBLE_T;


/* WID Data Types */
typedef enum {
    WID_CHAR     = 0,
    WID_SHORT    = 1,
    WID_INT      = 2,
    WID_STR      = 3,
    WID_BIN_DATA = 4
} WID_TYPE_T;

typedef enum{
    HOST_MSG_DATAOUT    = 0x10, /* Receive data from host */
    HOST_MSG_DATAIN     = 0x20, /* Transmit data to host  */
    HOST_MSG_CONFIGRSP  = 0x30, /* Response to host       */
    HOST_MSG_CONFIGREQ  = 0x40, /* Request from host      */
    HOST_MSG_ASYNCEVENT = 0x50
}HOST_MSG_TYPE;


/* WLAN Identifiers */
typedef enum {
    WID_NIL                            = -1,
    WID_BSS_TYPE                       = 0x0000,
    WID_CURRENT_TX_RATE                = 0x0001,
    WID_CURRENT_CHANNEL                = 0x0002,
    WID_PREAMBLE                       = 0x0003,
    WID_11G_OPERATING_MODE             = 0x0004,
    WID_STATUS                         = 0x0005,
    WID_11G_PROT_MECH                  = 0x0006,

#ifdef MAC_HW_UNIT_TEST_MODE
    WID_GOTO_SLEEP                     = 0x0007,
#else /* MAC_HW_UNIT_TEST_MODE */
    WID_SCAN_TYPE                      = 0x0007,
#endif /* MAC_HW_UNIT_TEST_MODE */
    WID_PRIVACY_INVOKED                = 0x0008,
    WID_KEY_ID                         = 0x0009,
    WID_QOS_ENABLE                     = 0x000A,
    WID_POWER_MANAGEMENT               = 0x000B,
    WID_802_11I_MODE                   = 0x000C,
    WID_AUTH_TYPE                      = 0x000D,
    WID_SITE_SURVEY                    = 0x000E,
    WID_LISTEN_INTERVAL                = 0x000F,
    WID_DTIM_PERIOD                    = 0x0010,
    WID_ACK_POLICY                     = 0x0011,
    WID_RESET                          = 0x0012,
    WID_PCF_MODE                       = 0x0013,
    WID_CFP_PERIOD                     = 0x0014,
    WID_BCAST_SSID                     = 0x0015,

#ifdef MAC_HW_UNIT_TEST_MODE
    WID_PHY_TEST_PATTERN               = 0x0016,
#else /* MAC_HW_UNIT_TEST_MODE */
    WID_DISCONNECT                     = 0x0016,
#endif /* MAC_HW_UNIT_TEST_MODE */

    WID_READ_ADDR_SDRAM                = 0x0017,
    WID_TX_POWER_LEVEL_11A             = 0x0018,
    WID_REKEY_POLICY                   = 0x0019,
    WID_SHORT_SLOT_ALLOWED             = 0x001A,
    WID_PHY_ACTIVE_REG                 = 0x001B,
    WID_PHY_ACTIVE_REG_VAL             = 0x001C,
    WID_TX_POWER_LEVEL_11B             = 0x001D,
    WID_START_SCAN_REQ                 = 0x001E,
    WID_RSSI                           = 0x001F,
    WID_JOIN_REQ                       = 0x0020,
    WID_ANTENNA_SELECTION              = 0x0021,
    WID_USER_CONTROL_ON_TX_POWER       = 0x0027,
    WID_MEMORY_ACCESS_8BIT             = 0x0029,
    WID_UAPSD_SUPPORT_AP               = 0x002A,

    WID_CURRENT_MAC_STATUS             = 0x0031,
    WID_AUTO_RX_SENSITIVITY            = 0x0032,
    WID_DATAFLOW_CONTROL               = 0x0033,
    WID_SCAN_FILTER                    = 0x0036,
    WID_LINK_LOSS_THRESHOLD            = 0x0037,
    WID_AUTORATE_TYPE                  = 0x0038,
    WID_CCA_THRESHOLD                  = 0x0039,

    WID_802_11H_DFS_MODE               = 0x003B,
    WID_802_11H_TPC_MODE               = 0x003C,

    WID_PHY_REG_ADDR                   = 0x0040,
    WID_PHY_REG_VAL                    = 0x0041,
    WID_PTA_MODE                       = 0x0042,
    WID_TRAP_TEST                      = 0x0043,
    WID_PTA_BLOCK_BT                   = 0x0044,
    WID_NETWORK_INFO_EN                = 0x0045,
    WID_RX_DATA_RATE                   = 0x004B,
    WID_POWER_SAVE                     = 0x004C,

    WID_RTS_THRESHOLD                  = 0x1000,
    WID_FRAG_THRESHOLD                 = 0x1001,
    WID_SHORT_RETRY_LIMIT              = 0x1002,
    WID_LONG_RETRY_LIMIT               = 0x1003,
    WID_CFP_MAX_DUR                    = 0x1004,
    WID_PHY_TEST_FRAME_LEN             = 0x1005,
    WID_BEACON_INTERVAL                = 0x1006,
    WID_MEMORY_ACCESS_16BIT            = 0x1008,

    WID_RX_SENSE                       = 0x100B,
    WID_ACTIVE_SCAN_TIME               = 0x100C,
    WID_PASSIVE_SCAN_TIME              = 0x100D,
    WID_SITE_SURVEY_SCAN_TIME          = 0x100E,
    WID_JOIN_TIMEOUT                   = 0x100F,
    WID_AUTH_TIMEOUT                   = 0x1010,
    WID_ASOC_TIMEOUT                   = 0x1011,
    WID_11I_PROTOCOL_TIMEOUT           = 0x1012,
    WID_EAPOL_RESPONSE_TIMEOUT         = 0x1013,
    WID_CCA_BUSY_STATUS                = 0x1014,

    WID_FAILED_COUNT                   = 0x2000,
    WID_RETRY_COUNT                    = 0x2001,
    WID_MULTIPLE_RETRY_COUNT           = 0x2002,
    WID_FRAME_DUPLICATE_COUNT          = 0x2003,
    WID_ACK_FAILURE_COUNT              = 0x2004,
    WID_RECEIVED_FRAGMENT_COUNT        = 0x2005,
    WID_MULTICAST_RECEIVED_FRAME_COUNT = 0x2006,
    WID_FCS_ERROR_COUNT                = 0x2007,
    WID_SUCCESS_FRAME_COUNT            = 0x2008,
    WID_PHY_TEST_PKT_CNT               = 0x2009,
    WID_PHY_TEST_TXD_PKT_CNT           = 0x200A,
    WID_TX_FRAGMENT_COUNT              = 0x200B,
    WID_TX_MULTICAST_FRAME_COUNT       = 0x200C,
    WID_RTS_SUCCESS_COUNT              = 0x200D,
    WID_RTS_FAILURE_COUNT              = 0x200E,
    WID_WEP_UNDECRYPTABLE_COUNT        = 0x200F,
    WID_REKEY_PERIOD                   = 0x2010,
    WID_REKEY_PACKET_COUNT             = 0x2011,
#ifdef MAC_HW_UNIT_TEST_MODE
    WID_Q_ENABLE_INFO                  = 0x2012,
#else /* MAC_HW_UNIT_TEST_MODE */
    WID_802_1X_SERV_ADDR               = 0x2012,
#endif /* MAC_HW_UNIT_TEST_MODE */
    WID_STACK_IP_ADDR                  = 0x2013,
    WID_STACK_NETMASK_ADDR             = 0x2014,
    WID_HW_RX_COUNT                    = 0x2015,
    WID_MEMORY_ADDRESS                 = 0x201E,
    WID_MEMORY_ACCESS_32BIT            = 0x201F,
    WID_RF_REG_VAL                     = 0x2021,
    WID_FIRMWARE_INFO                  = 0x2023,

    WID_SYS_FW_VER                     = 0x2801,
    WID_SYS_DBG_LVL                    = 0x2802,
    WID_SYS_DBG_AREA                   = 0x2803,
    WID_UT_MODE                        = 0x2804,
    WID_UT_TX_LEN                      = 0x2805,
    WID_PTA_CTS_FRAME_LEN              = 0x2806,
    WID_PREASSO_SLEEP                  = 0x2807,

    WID_SSID                           = 0x3000,
    WID_FIRMWARE_VERSION               = 0x3001,
    WID_OPERATIONAL_RATE_SET           = 0x3002,
    WID_BSSID                          = 0x3003,
    WID_WEP_KEY_VALUE0                 = 0x3004,
    WID_WEP_KEY_VALUE1                 = 0x3005,
    WID_WEP_KEY_VALUE2                 = 0x3006,
    WID_WEP_KEY_VALUE3                 = 0x3007,
    WID_802_11I_PSK                    = 0x3008,
    WID_HCCA_ACTION_REQ                = 0x3009,
    WID_802_1X_KEY                     = 0x300A,
    WID_HARDWARE_VERSION               = 0x300B,
    WID_MAC_ADDR                       = 0x300C,
    WID_PHY_TEST_DEST_ADDR             = 0x300D,
    WID_PHY_TEST_STATS                 = 0x300E,
    WID_PHY_VERSION                    = 0x300F,
    WID_SUPP_USERNAME                  = 0x3010,
    WID_SUPP_PASSWORD                  = 0x3011,
    WID_SITE_SURVEY_RESULTS            = 0x3012,
    WID_RX_POWER_LEVEL                 = 0x3013,

    WID_ADD_WEP_KEY                    = 0x3019,
    WID_REMOVE_WEP_KEY                 = 0x301A,
    WID_ADD_PTK                        = 0x301B,
    WID_ADD_RX_GTK                     = 0x301C,
    WID_ADD_TX_GTK                     = 0x301D,
    WID_REMOVE_KEY                     = 0x301E,
    WID_ASSOC_REQ_INFO                 = 0x301F,
    WID_ASSOC_RES_INFO                 = 0x3020,
    WID_UPDATE_RF_SUPPORTED_INFO       = 0x3021,
    WID_COUNTRY_IE                     = 0x3022,

    WID_WAPI_ASSOC_IE                  = 0x3023,
    WID_ADD_WAPI_PTK                   = 0x3024,
    WID_ADD_WAPI_RX_GTK                = 0x3025,
    WID_ADD_WAPI_TX_GTK                = 0x3026,
    WID_HIDE_SSID                      = 0x3027,
    //huanglei add for wps
    WID_GEN_ASSOC_IE                   = 0x3028,

    WID_CONFIG_HCCA_ACTION_REQ         = 0x4000,
    WID_UAPSD_CONFIG                   = 0x4001,
    WID_UAPSD_STATUS                   = 0x4002,
    WID_WMM_AP_AC_PARAMS               = 0x4003,
    WID_WMM_STA_AC_PARAMS              = 0x4004,
    WID_NEWORK_INFO                    = 0x4005,
    WID_STA_JOIN_INFO                  = 0x4006,
    WID_CONNECTED_STA_LIST             = 0x4007,
    WID_HUT_STATS                      = 0x4082,
    WID_STATISTICS                     = 0x4008,
    WID_MEMORY_DUMP                    = 0x4009,
    WID_LOAD_TRAP_MAP                  = 0x400a,
    WID_AGC_DGC_TBL                    = 0x400b,
    // miaodefang for PTA
    WID_PTA_PARAMETER				   = 0x4010,

    /* NMAC Binary WID list */
    WID_11N_AUTORATE_TABLE			   = 0x4080,

    WID_ALL                            = 0x7FFE,
    WID_MAX                            = 0xFFFF
} WID_T;

typedef enum
{
	PTA_NONE_PROTECT = 0,
	PTA_NULL_DATA_PROTECT,
	PTA_PS_POLL_PROTECT,
	PTA_SELF_CTS_PROTECT,
	PTA_AUTO_PROTECT

} PTA_PROTECT_MODE_T;

struct pta_param_s
{
	u8  prot_mode;
	u8  mac_rate;           // 0: MIN_basic rate
	u8  hw_retry;
	u8  sw_retry;
	u8  cca_bypass;

	u8	restore;

	u16 active_time;            /* Unit is 100us */
	u16 thresh_time;            /* Unit is 100us */

	u16 auto_prot_thresh_time;  /* Unit is 100us */

	/*
	 * BIT0: Check high priority Q NULL before send PS_Poll or NULL frame
	 * BIT1: Check normal priority Q(AC_VO_Q) NULL before send PS_Poll or NULL frame
	 * BIT2: Check AC_VI_Q NULL before send PS_Poll or NULL frame
	 * BIT3: Check AC_BE_Q NULL before send PS_Poll or NULL frame
	 * BIT4: Check AC_BK_Q NULL before send PS_Poll or NULL frame
	 * BIT5: Check g_more_data_expected when send PS_Poll
	 */
	u16 flags;

} __packed;

void wlan_clean_wid_node(wlan_wid_packet_node * widNode);
wlan_wid_packet_node * wlan_get_wid_node(wlan_private * priv);
void wlan_put_wid_node_in_freeQ(wlan_private * priv, wlan_wid_packet_node * widNode);
wlan_wid_packet_node * wlan_get_wid_node_in_freeQ(wlan_private * priv);
int wlan_put_wid_node_in_pendingQ(wlan_private * priv, wlan_wid_packet_node * widNode);
wlan_wid_packet_node * wlan_get_wid_node_in_pendingQ(wlan_private * priv);
int wlan_alloc_wid_queue(wlan_private * priv);
int wlan_release_wid_pending_queue(wlan_private *priv);
int wlan_free_wid_queue(wlan_private * priv);
int wlan_read_wid_rsp_polling(wlan_private *priv);
int wlan_generic_get(wlan_private *priv, 
        u16 wid, u8 *val, u16 val_len, u32*rspLen, WID_TYPE_T type);
int wlan_generic_get_uchar(wlan_private *priv, 
        u16 wid, u8 *val);

int wlan_generic_get_ushort(wlan_private *priv, 
        u16 wid, u8 *val);
int wlan_generic_get_ulong(wlan_private *priv, 
        u16 wid, u8 *val);
int wlan_generic_get_str(wlan_private *priv, 
        u16 wid, u8 *val, u32 len, u32 * rspLen);
int wlan_send_wid_packet(wlan_private *priv, u8 *val, u16 val_len, 
    u8 wid_msg_id);
int wlan_generic_set(wlan_private *priv, 
        u16 wid, u8 *val, u16 val_len, WID_TYPE_T type);
int wlan_generic_set_uchar(wlan_private *priv, 
        u16 wid, u8 val);
int wlan_generic_set_ushort(wlan_private *priv, 
        u16 wid, u16 val);
int wlan_generic_set_ulong(wlan_private *priv, 
        u16 wid, u32 val);
int wlan_generic_set_str(wlan_private *priv, 
        u16 wid, u8* val, u32 val_len);
int wlan_generic_set_bin(wlan_private * priv, u16 wid, u8 * val, u32 val_len);
int wlan_set_core_init_patch(wlan_private *priv, const u32 (*data)[2], u8 num);
int wlan_set_core_patch(wlan_private *priv, const u8 (*patch)[2], u8 num);
void wlan_wid_response(wlan_private *priv, 
        u8 *wid_rsp, u16 wid_rsp_len);
int wlan_set_scan_timeout(wlan_private *priv);
int wlan_start_scan_enable_network_info(wlan_private *priv);
int wlan_start_join(wlan_private *priv);
int wlan_set_txrate(wlan_private *priv, u8 mbps);
int wlan_get_fw_ver(wlan_private *priv, u32 *fw_ver);
int wlan_get_mac_addr(wlan_private *priv, u8 *mac_addr);
int wlan_get_bssid(wlan_private *priv, u8 *bssid);
int wlan_get_channel(wlan_private *priv, u8 *channel);
int wlan_get_rssi(wlan_private *priv, u8 *rssi);
int wlan_set_mac_addr(wlan_private *priv, u8 *mac_addr);
int wlan_set_preamble(wlan_private *priv, u8  preamble);
int wlan_set_scan_complete(wlan_private *priv);
int wlan_set_ssid(wlan_private *priv, 
        u8 *ssid, u8 ssid_len);
int wlan_get_ssid(wlan_private *priv, 
        u8 *ssid, u8 *ssid_len);
int wlan_set_bssid(wlan_private *priv, u8 *bssid);
int wlan_disconnect(wlan_private *priv);
int wlan_disconnect_silent(wlan_private * priv);
int wlan_set_imode(wlan_private *priv, u8 imode);
int wlan_set_authtype(wlan_private *priv, u8 authtype);
int wlan_set_listen_interval(wlan_private *priv, u8 interval);
int wlan_set_link_loss_threshold(wlan_private *priv, u8 threshold);
int wlan_set_power_save(wlan_private *priv);
int wlan_set_wepkey(wlan_private *priv, 
        u16 index, u8 *key, u8 key_len);            
int wlan_set_ptk(wlan_private *priv, 
        u8 *key, u8 key_len);
int wlan_set_gtk(wlan_private *priv, u8 key_id,
        u8 *key_rsc, u8 key_rsc_len,
        u8 *key, u8 key_len);
int wlan_set_pm_mode(wlan_private *priv, u8 pm_mode);
int wlan_set_preasso_sleep(wlan_private *priv, u32 preasso_sleep);
int rda5890_set_preamble(wlan_private *priv, unsigned char  preamble);                                        
int wlan_set_pta(wlan_private * priv, struct pta_param_s* param);
#endif
