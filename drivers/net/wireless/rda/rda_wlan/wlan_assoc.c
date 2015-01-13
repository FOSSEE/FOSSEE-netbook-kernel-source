#include "wlan_includes.h"


//imode
/* BIT0: 1 -> Security ON              0 -> OFF                          */
/* BIT1: 1 -> WEP40  cypher supported  0 -> Not supported                */
/* BIT2: 1 -> WEP104 cypher supported  0 -> Not supported                */
/* BIT3: 1 -> WPA mode      supported  0 -> Not supported                */
/* BIT4: 1 -> WPA2 (RSN)    supported  0 -> Not supported                */
/* BIT5: 1 -> AES-CCMP cphr supported  0 -> Not supported                */
/* BIT6: 1 -> TKIP   cypher supported  0 -> Not supported                */
/* BIT7: 1 -> TSN           supported  0 -> Not supported                */

//authtype
/* BIT0: 1 -> OPEN SYSTEM  */
/* BIT1: 1 -> SHARED KEY  */
/* BIT3: 1 -> WAPI   */
static int assoc_helper_secinfo(wlan_private *priv,
        struct bss_descriptor *assoc_bss)
{
    int ret = 0;

    WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);

    /* set imode and key */
    if (   !priv->secinfo.wep_enabled
	    && !priv->secinfo.WPAenabled && !priv->secinfo.WPA2enabled) {
        WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
        "%s, NO SEC\n", __func__);
        priv->imode = 0;
    } else {
        u16 key_len = 0;

        if (   priv->secinfo.wep_enabled
        && !priv->secinfo.WPAenabled
            && !priv->secinfo.WPA2enabled) {
            /* WEP */
            key_len = priv->wep_keys[0].len;
            WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "%s, WEP, len = %d\n", __func__, key_len * 8);
            if (key_len == KEY_LEN_WEP_40) {
                priv->imode = BIT0 | BIT1;
			} else if (key_len == KEY_LEN_WEP_104) {
                priv->imode = BIT0 | BIT2;
			} else {
				WLAN_ERRP("Invalide WEP Key length %d\n", key_len);
                ret = -EINVAL;
                goto out;
            }
        } else if (   !priv->secinfo.wep_enabled
                   && (priv->secinfo.WPAenabled ||
                       priv->secinfo.WPA2enabled)) {
            /* WPA */
            struct enc_key * pkey = NULL;

            WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "%s, WPA cp:%x wpa:%d wpa2:%d \n", __func__, priv->secinfo.cipther_type, priv->secinfo.WPAenabled, priv->secinfo.WPA2enabled);

            if (   priv->wpa_mcast_key.len
			    && (priv->wpa_mcast_key.flags & KEY_INFO_WPA_ENABLED))
                pkey = &priv->wpa_mcast_key;
            else if (   priv->wpa_unicast_key.len
				 && (priv->wpa_unicast_key.flags & KEY_INFO_WPA_ENABLED))
                pkey = &priv->wpa_unicast_key;

            priv->imode = 0;
            /* turn on security */
            priv->imode |= (BIT0);
            priv->imode &= ~(BIT3 | BIT4);
            if (priv->secinfo.WPA2enabled)
                priv->imode |= (BIT4);
            else if (priv->secinfo.WPAenabled)
                priv->imode |= (BIT3);
            /*
             * we don't know the cipher type by now
             * use dot11i_info to decide
             * and use CCMP if possible
             */
            priv->imode &= ~(BIT5 | BIT6);
            if (priv->secinfo.cipther_type & IW_AUTH_CIPHER_CCMP)
                priv->imode |= BIT5;
			else if (priv->secinfo.cipther_type & IW_AUTH_CIPHER_TKIP)
                priv->imode |= BIT6;
        } else {
            WLAN_ERRP("WEP and WPA/WPA2 enabled simutanously\n");
            ret = -EINVAL;
            goto out;
        }
    }

    /* set authtype */
    if (priv->secinfo.auth_mode & IW_AUTH_ALG_OPEN_SYSTEM
        || priv->secinfo.auth_mode & IW_AUTH_ALG_SHARED_KEY){

        if (priv->secinfo.auth_mode & IW_AUTH_ALG_OPEN_SYSTEM){
            WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "%s, Open Auth, KEY_MGMT = %d, AUTH_ALG mode:%x\n", __func__, priv->secinfo.key_mgmt, priv->secinfo.auth_mode);
            if (priv->secinfo.key_mgmt == 0x01)
                priv->authtype = BIT2;
            else
                priv->authtype = BIT0;
        }else if(priv->secinfo.auth_mode & IW_AUTH_ALG_SHARED_KEY){
            WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "%s, Shared-Key Auth AUTH_ALG mode:%x \n", __func__, priv->secinfo.auth_mode);
            priv->authtype = BIT1;
        }
		
		if (priv->secinfo.key_mgmt == WAPI_KEY_MGMT_PSK
		    || priv->secinfo.key_mgmt == WAPI_KEY_MGMT_CERT)
			priv->authtype = BIT3;
			
    }else if (priv->secinfo.auth_mode == IW_AUTH_ALG_WAPI) {
        WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
            "%s, Shared-Key Auth\n", __func__);
        priv->authtype = IW_AUTH_ALG_WAPI;
    }else if (priv->secinfo.auth_mode == IW_AUTH_ALG_LEAP) {
        WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
            "%s, LEAP Auth, not supported\n", __func__);
        ret = -EINVAL;
        goto out;
    }else {
        WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
            "%s, Unknown Auth\n", __func__);
        ret = -EINVAL;
        goto out;
    }

out:
    WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>> \n", __func__);
    return ret;
}


void wlan_assocication(wlan_private* priv)
{
    int ret = 0;
    struct bss_descriptor *assoc_bss;

    WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

    assoc_bss = get_bss_desc_from_scanlist(priv, priv->assoc_bssid);
    if (assoc_bss == NULL) {
        WLAN_ERRP("****fail to find bss in the scan list\n");
        ret = -EINVAL;
        goto out;
    }

    priv->curbssparams.channel = assoc_bss->channel;
    memcpy(priv->curbssparams.bssid, assoc_bss->bssid, ETH_ALEN);
    memcpy(priv->curbssparams.ssid, assoc_bss->ssid,IW_ESSID_MAX_SIZE + 1);

    ret = assoc_helper_secinfo(priv, assoc_bss);
    if (ret) {
        WLAN_ERRP("assoc_helper_secinfo fail, ret = %d\n", ret);
        goto out;
    }

    //wep so we need retry association
	if (priv->imode & 0x06) {
        priv->ToggalAssociation = TRUE;
    }

    ret = wlan_start_join(priv);
    if (ret) {
        WLAN_ERRP("wlan_set_ssid fail, ret = %d\n", ret);
        wlan_cancel_timer(&priv->AssociationTimeOut);
        priv->assoc_ongoing = FALSE;
        goto out;
    }

    //25S for association
    wlan_mod_timer(&priv->AssociationTimeOut, 25000);

    //reassociation
    wlan_mod_timer(&priv->ReAssociationTimeOut, 3000);
out:
    WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<< \n", __func__);
}

void wlan_re_assocication(wlan_private* priv)
{
    int ret = 0;

    ENTER();
#ifdef WLAN_FORCE_SUSPEND_SUPPORT
	if (priv->CardInSuspend == TRUE) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s ingnored in ap deep sleep mode.\n", __func__);
		return;
	}
#endif
	if(priv->connect_status == MAC_CONNECTED)
		return;
	if(priv->reassoc_count++ > 4)
		return;
    //wep shared key & open turn arount
	if (priv->imode & 0x06) {
		if (priv->authtype == 0x01)
			priv->authtype = 0x02;
        else
			priv->authtype = 0x01;
    }

    ret = wlan_start_join(priv);
    if (ret) {
        WLAN_ERRP("wlan_set_ssid fail, ret = %d\n", ret);
        return;
    }

    wlan_mod_timer(&priv->ReAssociationTimeOut, 3000);
    LEAVE();
}

void wlan_assocication_timeout(wlan_private* priv)
{

    ENTER();


    wlan_cancel_timer(&priv->ReAssociationTimeOut);

    priv->assoc_ongoing = FALSE;
	wlan_assoc_power_save(priv);
    //restore tx rate
    wlan_set_txrate(priv, 0);

    LEAVE();
}

