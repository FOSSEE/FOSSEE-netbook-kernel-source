#ifndef __WLAN_AVER_RSSI_H__
#define __WLAN_AVER_RSSI_H__

#define MAX_RSSI_RECORD (8)
#define INVALID_RSSI (-110)
typedef struct _t_aver_bssid_rssi{
	struct list_head list;
	u8 bssid[6];
	s8 dirty;
	u8 count;
	u8 index;
	u8 rssi[8];
}t_aver_bssid_rssi;


void wlan_add_new_aver_rssi(wlan_private * priv, u8 * bssid, u8 rssi);
t_aver_bssid_rssi* wlan_find_bssid_in_aver_rssi(wlan_private * priv, u8 * bssid);
void wlan_update_aver_rssi(wlan_private * priv, u8 * bssid, u8 rssi);
void wlan_set_rssi_dirty(wlan_private * priv);
s8 wlan_cal_aver_rssi(t_aver_bssid_rssi* aver_rssi);
s8 wlan_get_aver_rssi(wlan_private * priv, u8 * bssid);
void wlan_free_aver_rssi(wlan_private * priv);

#endif

