#include "wlan_includes.h"

DEFINE_SPINLOCK(rssi_buf_lock);

void wlan_add_new_aver_rssi(wlan_private * priv, u8 * bssid, u8 rssi)
{
	t_aver_bssid_rssi * list =  (t_aver_bssid_rssi *)kmalloc(sizeof(t_aver_bssid_rssi), GFP_KERNEL);

	if(!list)
		return;

	list->dirty = 0;
	memset(list->rssi, 0, sizeof(list->rssi));

	if(((s8)rssi) > 0)
		rssi -= 0x100;
    else if(((s8)rssi) > -20)
        rssi = ((s8)rssi) - 20;

	list->rssi[0] = rssi;
	list->index = 1;
	list->count = 1;
	memcpy(list->bssid, bssid, 6);

	spin_lock(&rssi_buf_lock);
	list_add_tail(&list->list, &priv->AverRssiQ);
	spin_unlock(&rssi_buf_lock);
}

t_aver_bssid_rssi* wlan_find_bssid_in_aver_rssi(wlan_private * priv, u8 * bssid)
{
	t_aver_bssid_rssi * rssi_list = NULL;
	
	spin_lock(&rssi_buf_lock);

	if(list_empty(&priv->AverRssiQ)){
		spin_unlock(&rssi_buf_lock);
		return NULL;
	}

	list_for_each_entry(rssi_list, &priv->AverRssiQ, list){
		if(memcmp(bssid, rssi_list->bssid, 6) == 0){
			spin_unlock(&rssi_buf_lock);
			return rssi_list;
		}
	}

	spin_unlock(&rssi_buf_lock);
	return NULL;
}

void wlan_set_aver_rssi(t_aver_bssid_rssi* aver_rssi, u8 rssi)
{
	u8 index = 0;

	u8 *ch = 0;
	ch = aver_rssi->bssid;

	if(((s8)rssi) > 0)
		rssi -= 0x100; 
	else if(((s8)rssi) > -20)
		rssi = ((s8)rssi) - 20;

	index = aver_rssi->index;
	aver_rssi->rssi[index] = rssi;
	index = (index + 1)%MAX_RSSI_RECORD;
	aver_rssi->index = index;

	if(aver_rssi->count < MAX_RSSI_RECORD)
		aver_rssi->count ++;

	if(aver_rssi->dirty > 0)
		aver_rssi->dirty --;
}

void wlan_update_aver_rssi(wlan_private * priv, u8 * bssid, u8 rssi)
{
	t_aver_bssid_rssi* aver_rssi = NULL;

	aver_rssi = wlan_find_bssid_in_aver_rssi(priv, bssid);
	if(aver_rssi)
		wlan_set_aver_rssi(aver_rssi, rssi);
	else
		wlan_add_new_aver_rssi(priv, bssid, rssi);
}

s8 wlan_cal_aver_rssi(t_aver_bssid_rssi* aver_rssi)
{
	s32 rssi = 0;
	u8  i = 0, count = 0;

	count = aver_rssi->count;

	for(i = 0; i < count; i ++){
		rssi += (s8)aver_rssi->rssi[i];
	}
	//-15 was hardware diff
	return ((s8)(rssi/count)) - 15;
}

s8 wlan_get_aver_rssi(wlan_private * priv, u8 * bssid)
{
	t_aver_bssid_rssi * rssi_list = NULL;

	rssi_list = wlan_find_bssid_in_aver_rssi(priv, bssid);
	if(rssi_list)
		return wlan_cal_aver_rssi(rssi_list);
	else{
		return INVALID_RSSI;
	}
}

void wlan_set_rssi_dirty(wlan_private * priv)
{
	t_aver_bssid_rssi * rssi_list = NULL, *next = NULL;

	spin_lock(&rssi_buf_lock);
	if(list_empty(&priv->AverRssiQ)){
		spin_unlock(&rssi_buf_lock);
		return;
	}

	list_for_each_entry_safe(rssi_list, next, &priv->AverRssiQ, list){
		rssi_list->dirty ++;

		if(rssi_list->dirty >= MAX_RSSI_RECORD){
			if(memcmp(rssi_list->bssid, priv->curbssparams.bssid, 6) != 0){
				list_del(&rssi_list->list);
				kfree(rssi_list);
			}
		}	
	}	
	spin_unlock(&rssi_buf_lock);
}

void wlan_free_aver_rssi(wlan_private * priv)
{
	t_aver_bssid_rssi * rssi_list = NULL, *next = NULL;

	spin_lock(&rssi_buf_lock);
	if(list_empty(&priv->AverRssiQ)){
		spin_unlock(&rssi_buf_lock);
		return;
	}

	list_for_each_entry_safe(rssi_list, next, &priv->AverRssiQ, list){
		list_del(&rssi_list->list);
		kfree(rssi_list);
	}
	spin_unlock(&rssi_buf_lock);
}

