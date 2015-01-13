#include "wlan_includes.h"

//#define SCAN_RESULT_DEBUG

#ifndef FCS_LEN
#define FCS_LEN                 4
#endif

/* Element ID  of various Information Elements */
typedef enum {
    ISSID         = 0,   /* Service Set Identifier   */
    ISUPRATES     = 1,   /* Supported Rates          */
    IFHPARMS      = 2,   /* FH parameter set         */
    IDSPARMS      = 3,   /* DS parameter set         */
    ICFPARMS      = 4,   /* CF parameter set         */
    ITIM          = 5,   /* Traffic Information Map  */
    IIBPARMS      = 6,   /* IBSS parameter set       */
    ICTEXT        = 16,  /* Challenge Text           */
    IERPINFO      = 42,  /* ERP Information          */
    IEXSUPRATES   = 50,   /* Extended Supported Rates */            
    IWAPI               =68          
} ELEMENTID_T;

/* Capability Information field bit assignments  */
typedef enum {
    ESS           = 0x01,   /* ESS capability               */
    IBSS          = 0x02,   /* IBSS mode                    */
    POLLABLE      = 0x04,   /* CF Pollable                  */
    POLLREQ       = 0x08,   /* Request to be polled         */
    PRIVACY       = 0x10,   /* WEP encryption supported     */
    SHORTPREAMBLE  = 0x20,   /* Short Preamble is supported  */
    SHORTSLOT      = 0x400,  /* Short Slot is supported      */
    PBCC          = 0x40,   /* PBCC                         */
    CHANNELAGILITY = 0x80,   /* Channel Agility              */
    SPECTRUM_MGMT = 0x100,  /* Spectrum Management          */
    DSSS_OFDM      = 0x2000  /* DSSS-OFDM                    */
} CAPABILITY_T;

/* BSS type */
typedef enum {
    INFRASTRUCTURE  = 1,
    INDEPENDENT     = 2,
    ANY_BSS         = 3
} BSSTYPE_T;

u8 *get_ie_elem(u8 * msa, ELEMENTID_T elm_id, u16 rx_len, u16 tag_param_offset)
{
    u16 index = 0;

    /*************************************************************************/
    /*                       Beacon Frame - Frame Body                       */
    /* --------------------------------------------------------------------- */
    /* |Timestamp |BeaconInt |CapInfo |SSID |SupRates |DSParSet |TIM elm   | */
    /* --------------------------------------------------------------------- */
    /* |8         |2         |2       |2-34 |3-10     |3        |4-256     | */
    /* --------------------------------------------------------------------- */
    /*                                                                       */
    /*************************************************************************/

    index = tag_param_offset;

    /* Search for the TIM Element Field and return if the element is found */
    while(index < (rx_len - FCS_LEN)) {
        if(msa[index] == elm_id){
            return(&msa[index]);
        }else{
            index += (2 + msa[index + 1]);
        }
    }

    return(0);
}

/* This function extracts the 'from ds' bit from the MAC header of the input */
/* frame.                                                                    */
/* Returns the value in the LSB of the returned value.                       */
u8 get_from_ds(u8* header)
{
    return ((header[1] & 0x02) >> 1);
}

/* This function extracts the 'to ds' bit from the MAC header of the input   */
/* frame.                                                                    */
/* Returns the value in the LSB of the returned value.                       */
u8 get_to_ds(u8* header)
{
    return (header[1] & 0x01);
}

/* This function extracts the MAC Address in 'address1' field of the MAC     */
/* header and updates the MAC Address in the allocated 'addr' variable.      */
void get_address1(u8* msa, u8* addr)
{
    memcpy(addr, msa + 4, 6);
}

/* This function extracts the MAC Address in 'address2' field of the MAC     */
/* header and updates the MAC Address in the allocated 'addr' variable.      */
void get_address2(u8* msa, u8* addr)
{
    memcpy(addr, msa + 10, 6);
}

/* This function extracts the MAC Address in 'address3' field of the MAC     */
/* header and updates the MAC Address in the allocated 'addr' variable.      */
void get_address3(u8* msa, u8* addr)
{
    memcpy(addr, msa + 16, 6);
}

/* This function extracts the BSSID from the incoming WLAN packet based on   */
/* the 'from ds' bit, and updates the MAC Address in the allocated 'addr'    */
/* variable.                                                                 */
void get_BSSID(u8* data, u8* bssid)
{
    if(get_from_ds(data) == 1)
        get_address2(data, bssid);
    else if(get_to_ds(data) == 1)
        get_address1(data, bssid);
    else
        get_address3(data, bssid);
}

void wlan_network_information(wlan_private * priv, u8 * info, u16 info_len)
{
    struct bss_descriptor *iter_bss; 
	struct bss_descriptor *bss = NULL;
    unsigned char  *pos, *end, *p;
    unsigned char n_ex_rates = 0, got_basic_rates = 0, n_basic_rates = 0;
    struct ieee_ie_country_info_set *pcountryinfo;

    unsigned char* msa = &info[9];
    unsigned short msa_len = info[6] | (info[7] << 8);
    
#if 0
    if(priv->scan_running == WLAN_SCAN_IDLE){
        WLAN_ERRP("is not in scan process \n");
        goto done;
    }
#endif

    if((msa_len - 1 + 9 ) != info_len){
        WLAN_ERRP("rda5890_network_information verify lengh feild failed \n");
    }

	bss = kzalloc(sizeof(struct bss_descriptor), GFP_KERNEL);
	if (bss == NULL) {
		WLAN_ERRP("alloc bss_descriptor memory failed \n");
		return;
	}
    memset(bss, 0, sizeof (struct bss_descriptor));
    bss->rssi = info[8];
    msa_len -= 1; // has rssi

    get_BSSID(msa, bss->bssid);

    end = msa + msa_len;
    
    //mac head
    pos = msa + 24;
    //time stamp
    pos += 8;
    //beacon
    bss->beaconperiod = *(pos) | (*(pos + 1) << 8);
    pos += 2 ;
    //capability
    bss->capability = *(pos) | (*(pos + 1) << 8);
    pos += 2;

    if (bss->capability & WLAN_CAPABILITY_IBSS)
        bss->mode = IW_MODE_ADHOC;
    else
        bss->mode = IW_MODE_INFRA;

  /* process variable IE */
    while (pos + 2 <= end) {
        
    if (pos + pos[1] > end) {
#ifdef SCAN_RESULT_DEBUG            
         WLAN_DBGP("process_bss: error in processing IE, "
                 "bytes left < IE length\n");
#endif
			kfree(bss);
			return;
		}

		switch (pos[0]) {
		case WLAN_EID_SSID:
			bss->ssid_len = min_t(int, IEEE80211_MAX_SSID_LEN, pos[1]);
			memcpy(bss->ssid, pos + 2, bss->ssid_len);

			if (priv->scan_ssid_len > 0) {
				if (bss->ssid_len != priv->scan_ssid_len) {
					kfree(bss);
					return;
				}

				if (memcmp(bss->ssid, priv->scan_ssid, priv->scan_ssid_len)) {
					kfree(bss);
					return;
				}
    }
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got SSID IE: '%s', len %u %d\n",
				  bss->ssid, bss->ssid_len, pos[1]);
#endif
          break;

    case WLAN_EID_SUPP_RATES:
        n_basic_rates = min_t(uint8_t, MAX_RATES, pos[1]);
        memcpy(bss->rates, pos + 2, n_basic_rates);
        got_basic_rates = 1;
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got RATES IE\n");
#endif
          break;

    case WLAN_EID_FH_PARAMS:
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got FH IE\n");
#endif
        break;

    case WLAN_EID_DS_PARAMS:
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got DS IE\n");
#endif
        bss->channel = pos[2];
        break;

    case WLAN_EID_CF_PARAMS:
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got CF IE\n");
#endif
        break;

    case WLAN_EID_IBSS_PARAMS:
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got IBSS IE\n");
#endif
        break;

    case WLAN_EID_COUNTRY:
        pcountryinfo = (struct ieee_ie_country_info_set *) pos;
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got COUNTRY IE\n");
#endif
        break;

    case WLAN_EID_EXT_SUPP_RATES:
        /* only process extended supported rate if data rate is
         * already found. Data rate IE should come before
         * extended supported rate IE
         */
#ifdef SCAN_RESULT_DEBUG             
        WLAN_DBGP("got RATESEX IE\n");
#endif
        if (!got_basic_rates) {
#ifdef SCAN_RESULT_DEBUG                
            WLAN_DBGP("... but ignoring it\n");
#endif
            break;
        }

        n_ex_rates = pos[1];
        if (n_basic_rates + n_ex_rates > MAX_RATES)
            n_ex_rates = MAX_RATES - n_basic_rates;

        p = bss->rates + n_basic_rates;
        memcpy(p, pos + 2, n_ex_rates);
        break;

    case WLAN_EID_GENERIC:
        if (pos[1] >= 4 &&
        pos[2] == 0x00 && pos[3] == 0x50 &&
        pos[4] == 0xf2 && pos[5] == 0x01) {
            bss->wpa_ie_len = min(pos[1] + 2, MAX_WPA_IE_LEN);
				bss->wpa_ie = kzalloc(bss->wpa_ie_len, GFP_KERNEL);
				if(bss->wpa_ie)
					memcpy(bss->wpa_ie, pos, bss->wpa_ie_len);
				else
					bss->wpa_ie_len = 0;
#ifdef SCAN_RESULT_DEBUG
				WLAN_DBGP("got WPA IE \n");
#endif
			}

	/* huanglei add for wps */
	else if(pos[1] >= 4 &&
			pos[2] == 0x00 && pos[3] == 0x50 &&
			pos[4] == 0xf2 && pos[5] == 0x04 &&
			(priv->version == WLAN_VERSION_91_E || priv->version == WLAN_VERSION_91_F)) {
			bss->wps_ie_len = min(pos[1] + 2, MAX_WPS_IE_LEN);
			bss->wps_ie = kzalloc(bss->wps_ie_len, GFP_KERNEL);
			if(bss->wps_ie)
				memcpy(bss->wps_ie, pos, bss->wps_ie_len);
			else
				bss->wps_ie_len = 0;
#ifdef SCAN_RESULT_DEBUG
	WLAN_DBGP("got WPS IE \n");
#endif
			}
			else {
#ifdef SCAN_RESULT_DEBUG
            WLAN_DBGP("got generic IE: %02x:%02x:%02x:%02x, len %d\n",
				     pos[2], pos[3], pos[4], pos[5], pos[1]);
#endif
        }
          break;

    case WLAN_EID_RSN:
#ifdef SCAN_RESULT_DEBUG            
        WLAN_DBGP("got RSN IE\n");
#endif
        bss->rsn_ie_len = min(pos[1] + 2, MAX_WPA_IE_LEN);
			bss->rsn_ie = kzalloc(bss->rsn_ie_len, GFP_KERNEL);
			if(bss->rsn_ie)
				memcpy(bss->rsn_ie, pos, bss->rsn_ie_len);
			else
				bss->rsn_ie_len = 0;

			break;

		case IWAPI:
#ifdef SCAN_RESULT_DEBUG
			WLAN_DBGP("got WAPI IE\n");
#endif
			bss->wapi_ie_len = min(pos[1] + 2, 100);
			bss->wapi_ie = kzalloc(bss->wapi_ie_len, GFP_KERNEL);
			if(bss->wapi_ie)
				memcpy(bss->wapi_ie, pos, bss->wapi_ie_len);
			else
				bss->wapi_ie_len = 0;
			break;

    default:
        break;
    }

        pos += pos[1] + 2;
    }

	bss->last_scanned = jiffies;

	wlan_update_aver_rssi(priv, bss->bssid, bss->rssi);
	/* add scaned bss into list */
	if (1) {
		struct bss_descriptor *found = NULL;
		struct bss_descriptor *oldest = NULL;
		struct bss_descriptor *unsed = NULL;
		struct bss_descriptor *bss_lower_rssi = NULL; 
		s8 rssi = 0, lowRssi = 0;

		spin_lock(&priv->ScanListLock);
		/* Try to find this bss in the scan table */
		list_for_each_entry(iter_bss, &priv->network_list, list) {
			if (is_same_network(iter_bss, bss)) {
				found = iter_bss;
				break;
			}

			if (time_before(iter_bss->last_scanned + DEFAULT_MAX_SCAN_AGE, jiffies)) {
				if(!oldest || (oldest->last_scanned > iter_bss->last_scanned))
					oldest = iter_bss;
			}

			if (!iter_bss->ssid_len && unsed==NULL ) {
					unsed=iter_bss;
					break;
            }
		
			if(!is_zero_eth_addr(iter_bss->bssid)){
				rssi = wlan_get_aver_rssi(priv, iter_bss->bssid);
				if(bss_lower_rssi == NULL || rssi < lowRssi){
					lowRssi = rssi;
					bss_lower_rssi = iter_bss;
					if(rssi == INVALID_RSSI)
						break;
				}
			}
		}

		if (found) { // have in network list
			 WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_DEBUG, "FOUND SAME %s, update\n", found->ssid);
			/* found, clear it */
			clear_bss_descriptor(found);
			memcpy(found, bss, offsetof(struct bss_descriptor, list));
		} else { //not have in network list
			if (unsed) {
				WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_DEBUG,
							"FOUND NEW %s, add\n", bss->ssid);
				clear_bss_descriptor(unsed);
				memcpy(unsed, bss, offsetof(struct bss_descriptor, list));
			}else { // do not have space
				if(oldest) { // have oldest
					printk("**FOUND NEW %s, no space, replace oldest %s\n",
								bss->ssid, oldest->ssid);
					clear_bss_descriptor(oldest);
					memcpy(oldest, bss, offsetof(struct bss_descriptor, list));
				}else if (bss_lower_rssi) { // have lower rssi
					printk("**FOUND NEW %s, no space, replace low_rssi %s\n",
								bss->ssid, bss_lower_rssi->ssid);
					clear_bss_descriptor(bss_lower_rssi);
					memcpy(bss_lower_rssi, bss, offsetof(struct bss_descriptor, list));
				}else { // don't have oldest
					WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_CRIT, "Networklist Oldest is NULL\n");
				}
			}
		}
		spin_unlock(&priv->ScanListLock);
	}
#ifdef SCAN_RESULT_DEBUG
	WLAN_DBGP("rda5890_network_information .\n");
#endif
	kfree(bss);
    return;
}

void wlan_report_scan_result(wlan_private * priv)
{
    union iwreq_data wrqu;
    ENTER();
    priv->scan_running = WLAN_SCAN_COMPLET;
    memset(&wrqu, 0, sizeof(union iwreq_data));
    wireless_send_event(priv->netDev, SIOCGIWSCAN, &wrqu, NULL);    
	wlan_mod_timer(&priv->CardToSleepTimer, 100);
    LEAVE();
}

