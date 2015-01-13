#include "wlan_includes.h"

static int wlan_get_name(struct net_device *dev, struct iw_request_info *info,
			 char *cwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	/* We could add support for 802.11n here as needed. Jean II */
	snprintf(cwrq, IFNAMSIZ, "IEEE 802.11b/g");

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_freq(struct net_device *dev, struct iw_request_info *info,
			 struct iw_freq *fwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	fwrq->m = (long)2437 *100000;
	fwrq->e = 1;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_wap(struct net_device *dev, struct iw_request_info *info,
			struct sockaddr *awrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	if (priv->connect_status == MAC_CONNECTED) {
		memcpy(awrq->sa_data, priv->curbssparams.bssid, ETH_ALEN);
	} else {
		memset(awrq->sa_data, 0, ETH_ALEN);
	}
	awrq->sa_family = ARPHRD_ETHER;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_nick(struct net_device *dev, struct iw_request_info *info,
			 struct iw_point *dwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_nick(struct net_device *dev, struct iw_request_info *info,
			 struct iw_point *dwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_rts(struct net_device *dev, struct iw_request_info *info,
			struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_rts(struct net_device *dev, struct iw_request_info *info,
			struct iw_param *vwrq, char *extra)
{
	int ret = 0;
	u16 val = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	val = 600;

	vwrq->value = val;
	vwrq->disabled = val > WLAN_RTS_MAX_VALUE;	/* min rts value is 0 */
	vwrq->fixed = 1;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_set_frag(struct net_device *dev, struct iw_request_info *info,
			 struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_frag(struct net_device *dev, struct iw_request_info *info,
			 struct iw_param *vwrq, char *extra)
{
	int ret = 0;
	u16 val = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	val = 1460;

	vwrq->value = val;
	vwrq->disabled = ((val < WLAN_FRAG_MIN_VALUE)
			  || (val > WLAN_FRAG_MAX_VALUE));
	vwrq->fixed = 1;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_get_mode(struct net_device *dev,
			 struct iw_request_info *info, u32 * uwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	*uwrq = IW_MODE_INFRA;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_txpow(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	vwrq->value = 20;	// in dbm
	vwrq->fixed = 1;
	vwrq->disabled = 0;
	vwrq->flags = IW_TXPOW_DBM;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_retry(struct net_device *dev, struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_retry(struct net_device *dev, struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	int ret = 0;
	u16 val = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	vwrq->disabled = 0;

	if (vwrq->flags & IW_RETRY_LONG) {
		val = 7;

		/* Subtract 1 to convert try count to retry count */
		vwrq->value = val - 1;
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_LONG;
	} else {
		val = 6;

		/* Subtract 1 to convert try count to retry count */
		vwrq->value = val - 1;
		vwrq->flags = IW_RETRY_LIMIT | IW_RETRY_SHORT;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

/** 
 * 802.11b/g supported bitrates (in 500Kb/s units) 
 */
u8 wlan_bg_rates[MAX_RATES] = { 0x02, 0x04, 0x0b, 0x16, 0x0c, 0x12, 0x18,
	0x24, 0x30, 0x48, 0x60, 0x6c, 0x00, 0x00
};

u16 wlan_nr_chan = 11;

/**
 *  @brief Get Range Info
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 --success, otherwise fail
 */
static int wlan_get_range(struct net_device *dev, struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	struct iw_range *range = (struct iw_range *)extra;
	int i;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	dwrq->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->min_nwid = 0;
	range->max_nwid = 0;

	range->num_bitrates = sizeof(wlan_bg_rates);
	for (i = 0; i < range->num_bitrates; i++)
		range->bitrate[i] = wlan_bg_rates[i] * 500000;
	range->num_bitrates = i;

	range->num_frequency = 0;

	range->scan_capa = IW_SCAN_CAPA_ESSID;

	for (i = 0; (range->num_frequency < IW_MAX_FREQUENCIES)
	     && (i < wlan_nr_chan); i++) {
		range->freq[range->num_frequency].i = (long)(i + 1);
		range->freq[range->num_frequency].m =
		    (long)((2412 + 5 * i) * 100000);
		range->freq[range->num_frequency].e = 1;
		range->num_frequency++;
	}

	range->num_channels = range->num_frequency;

	/*
	 * Set an indication of the max TCP throughput in bit/s that we can
	 * expect using this interface
	 */
	range->throughput = 5000 * 1000;

	range->min_rts = WLAN_RTS_MIN_VALUE;
	range->max_rts = WLAN_RTS_MAX_VALUE;
	range->min_frag = WLAN_FRAG_MIN_VALUE;
	range->max_frag = WLAN_FRAG_MAX_VALUE;

	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;
	range->num_encoding_sizes = 2;
	range->max_encoding_tokens = 4;

	/*
	 * Right now we support only "iwconfig ethX power on|off"
	 */
	range->pm_capa = IW_POWER_ON;

	/*
	 * Minimum version we recommend
	 */
	range->we_version_source = 15;

	/*
	 * Version we are compiled with
	 */
	range->we_version_compiled = WIRELESS_EXT;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT | IW_RETRY_MAX;

	range->min_retry = 0;
	range->max_retry = 14;

	/*
	 * Set the qual, level and noise range values
	 */
	range->max_qual.qual = 100;
	range->max_qual.level = 0;
	range->max_qual.noise = 0;
	range->max_qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;

	range->avg_qual.qual = 70;
	/* TODO: Find real 'good' to 'bad' threshold value for RSSI */
	range->avg_qual.level = 0;
	range->avg_qual.noise = 0;
	range->avg_qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;

	range->sensitivity = 0;

	/* Setup the supported power level ranges */
	memset(range->txpower, 0, sizeof(range->txpower));
	range->txpower_capa = IW_TXPOW_DBM | IW_TXPOW_RANGE;
	range->txpower[0] = 0;
	range->txpower[1] = 20;
	range->num_txpower = 2;

	range->event_capa[0] = (IW_EVENT_CAPA_K_0 |
				IW_EVENT_CAPA_MASK(SIOCGIWAP) |
				IW_EVENT_CAPA_MASK(SIOCGIWSCAN));
	range->event_capa[1] = IW_EVENT_CAPA_K_1;

	range->enc_capa = IW_ENC_CAPA_WPA
                          | IW_ENC_CAPA_WPA2
                          | IW_ENC_CAPA_CIPHER_TKIP
                          | IW_ENC_CAPA_CIPHER_CCMP;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_power(struct net_device *dev, struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_power(struct net_device *dev, struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	vwrq->value = 0;
	vwrq->flags = 0;
	vwrq->disabled = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_update_bss_stats(wlan_private * priv)
{
	int ret = -1;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	memcpy(priv->curbssparams.ssid,
	       priv->assoc_ssid, sizeof(priv->curbssparams.ssid));

	if (priv->scan_running == WLAN_SCAN_RUNNING)
		return 0;

	ret = wlan_get_bssid(priv, priv->curbssparams.bssid);
	if (ret) {
		WLAN_ERRP("wlan_get_bssid, ret = %d\n", ret);
		goto out;
	}

	ret = wlan_get_rssi(priv, &priv->curbssparams.rssi);
	if (ret) {
		WLAN_ERRP("wlan_get_rssi, ret = %d\n", ret);
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<< ch = %d  rssi = %d\n",
		    __func__, priv->curbssparams.channel,
		    priv->curbssparams.rssi);

out:
	return ret;
}

static struct iw_statistics *wlan_get_wireless_stats(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int stats_valid = 0;
	u8 snr;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	if (priv->connect_status != MAC_CONNECTED)
		goto out;

	wlan_update_bss_stats(priv);

	priv->wstats.miss.beacon = 0;
	priv->wstats.discard.retries = 0;
	priv->wstats.qual.level = 0x100 + (s8)priv->curbssparams.rssi;

	snr = priv->wstats.qual.level - WLAN_NF_DEFAULT_SCAN_VALUE;
	priv->wstats.qual.qual =
	    (100 * RSSI_DIFF * RSSI_DIFF - (PERFECT_RSSI - snr) *
	     (15 * (RSSI_DIFF) + 62 * (PERFECT_RSSI - snr))) /
	    (RSSI_DIFF * RSSI_DIFF);
	if (priv->wstats.qual.qual > 100)
		priv->wstats.qual.qual = 100;
	priv->wstats.qual.noise = WLAN_NF_DEFAULT_SCAN_VALUE;
	priv->wstats.qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;

	stats_valid = 1;

out:
	if (!stats_valid) {
		priv->wstats.miss.beacon = 0;
		priv->wstats.discard.retries = 0;
		priv->wstats.qual.qual = 0;
		priv->wstats.qual.level = 0;
		priv->wstats.qual.noise = 0;
		priv->wstats.qual.updated = IW_QUAL_ALL_UPDATED;
		priv->wstats.qual.updated |= IW_QUAL_NOISE_INVALID |
		    IW_QUAL_QUAL_INVALID | IW_QUAL_LEVEL_INVALID;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return &priv->wstats;
}

static int wlan_set_freq(struct net_device *dev, struct iw_request_info *info,
			 struct iw_freq *fwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_rate(struct net_device *dev, struct iw_request_info *info,
			 struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_get_rate(struct net_device *dev, struct iw_request_info *info,
			 struct iw_param *vwrq, char *extra)
{
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);
	vwrq->fixed = 0;
	vwrq->value = 54*1000;
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_set_mode(struct net_device *dev,
			 struct iw_request_info *info, u32 * uwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

/**
 *  @brief Get Encryption key
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 --success, otherwise fail
 */
static int wlan_get_encode(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq, u8 * extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

/**
 *  @brief Set Encryption key
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 --success, otherwise fail
 */
static int wlan_set_encode(struct net_device *dev,
			   struct iw_request_info *info,
			   struct iw_point *dwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

/**
 *  @brief Set Encryption key (internal)
 *
 *  @param priv         A pointer to private card structure
 *  @param key_material     A pointer to key material
 *  @param key_length       length of key material
 *  @param index        key index to set
 *  @param set_tx_key       Force set TX key (1 = yes, 0 = no)
 *  @return             0 --success, otherwise fail
 */
static int copy_wep_key(wlan_private * priv,
			const char *key_material,
			u16 key_length, u16 index, int set_tx_key)
{
	int ret = 0;
	struct enc_key *pkey;

	/* Paranoid validation of key index */
	if (index > 3) {
		ret = -EINVAL;
		goto out;
	}

	/* validate max key length */
	if (key_length > KEY_LEN_WEP_104) {
		ret = -EINVAL;
		goto out;
	}

	if (key_length == KEY_LEN_WEP_40) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "WEP40 : %02x%02x%02x%02x%02x\n",
			    key_material[0], key_material[1], key_material[2],
			    key_material[3], key_material[4]);
	} else if (key_length == KEY_LEN_WEP_104) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "WEP104 : %02x%02x%02x%02x%02x"
			    " %02x%02x%02x%02x%02x"
			    " %02x%02x%02x\n",
			    key_material[0], key_material[1], key_material[2],
			    key_material[3], key_material[4], key_material[5],
			    key_material[6], key_material[7], key_material[8],
			    key_material[9], key_material[10], key_material[11],
			    key_material[12]);
	} else {
		WLAN_ERRP("Error in WEP Key length %d\n", key_length);
	}

	pkey = &priv->wep_keys[index];

	if (key_length > 0) {
		memset(pkey, 0, sizeof(struct enc_key));
		pkey->type = KEY_TYPE_ID_WEP;

		/* Standardize the key length */
		pkey->len = (key_length > KEY_LEN_WEP_40) ?
		    KEY_LEN_WEP_104 : KEY_LEN_WEP_40;
		memcpy(pkey->key, key_material, key_length);
	}

	if (set_tx_key) {
		/* Ensure the chosen key is valid */
		if (!pkey->len) {
			WLAN_ERRP("key not set, so cannot enable it\n");
			ret = -EINVAL;
			goto out;
		}
		priv->wep_tx_keyidx = index;
	}

	priv->secinfo.wep_enabled = 1;

out:
	return ret;
}

static int validate_key_index(u16 def_index, u16 raw_index,
			      u16 * out_index, u16 * is_default)
{
	if (!out_index || !is_default)
		return -EINVAL;

	/* Verify index if present, otherwise use default TX key index */
	if (raw_index > 0) {
		if (raw_index > 4)
			return -EINVAL;
		*out_index = raw_index - 1;
	} else {
		*out_index = def_index;
		*is_default = 1;
	}
	return 0;
}

static void disable_wep(wlan_private * priv)
{
	int i;

	/* Set Open System auth mode */
	priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;

	/* Clear WEP keys and mark WEP as disabled */
	priv->secinfo.wep_enabled = 0;
	for (i = 0; i < 4; i++)
		priv->wep_keys[i].len = 0;
}

static void disable_wpa(wlan_private * priv)
{
	memset(&priv->wpa_mcast_key, 0, sizeof(struct enc_key));
	priv->wpa_mcast_key.flags = KEY_INFO_WPA_MCAST;

	memset(&priv->wpa_unicast_key, 0, sizeof(struct enc_key));
	priv->wpa_unicast_key.flags = KEY_INFO_WPA_UNICAST;

	priv->secinfo.WPAenabled = 0;
	priv->secinfo.WPA2enabled = 0;
	priv->secinfo.cipther_type = 0;
	priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
}

/**
 *  @brief Get Extended Encryption key (WPA/802.1x and WEP)
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 on success, otherwise failure
 */
static int wlan_get_encodeext(struct net_device *dev,
			      struct iw_request_info *info,
			      struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = -EINVAL;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int index, max_key_len;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	max_key_len = dwrq->length - sizeof(*ext);
	if (max_key_len < 0)
		goto out;

	index = dwrq->flags & IW_ENCODE_INDEX;
	if (index) {
		if (index < 1 || index > 4)
			goto out;
		index--;
	} else {
		index = priv->wep_tx_keyidx;
	}

	if (!(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) &&
	    ext->alg != IW_ENCODE_ALG_WEP) {
		if (index != 0)
			goto out;
	}

	dwrq->flags = index + 1;
	memset(ext, 0, sizeof(*ext));

	if (!priv->secinfo.wep_enabled
	    && !priv->secinfo.WPAenabled 
		&& !priv->secinfo.WPA2enabled) {
		ext->alg = IW_ENCODE_ALG_NONE;
		ext->key_len = 0;
		dwrq->flags |= IW_ENCODE_DISABLED;
	} else {
		u8 *key = NULL;

		if (priv->secinfo.wep_enabled
		    && !priv->secinfo.WPAenabled
		    && !priv->secinfo.WPA2enabled) {
			/* WEP */
			ext->alg = IW_ENCODE_ALG_WEP;
			ext->key_len = priv->wep_keys[index].len;
			key = &priv->wep_keys[index].key[0];
		} else if (!priv->secinfo.wep_enabled
			   && (priv->secinfo.WPAenabled ||
			       priv->secinfo.WPA2enabled)) {
			/* WPA */
			struct enc_key *pkey = NULL;

			if (priv->wpa_mcast_key.len
			    && (priv->wpa_mcast_key.
				flags & KEY_INFO_WPA_ENABLED))
				pkey = &priv->wpa_mcast_key;
			else if (priv->wpa_unicast_key.len
				 && (priv->wpa_unicast_key.
				     flags & KEY_INFO_WPA_ENABLED))
				pkey = &priv->wpa_unicast_key;

			if (pkey) {
				if (pkey->type == KEY_TYPE_ID_AES) {
					ext->alg = IW_ENCODE_ALG_CCMP;
				} else {
					ext->alg = IW_ENCODE_ALG_TKIP;
				}
				ext->key_len = pkey->len;
				key = &pkey->key[0];
			} else {
				ext->alg = IW_ENCODE_ALG_TKIP;
				ext->key_len = 0;
			}
		} else {
			goto out;
		}

		if (ext->key_len > max_key_len) {
			ret = -E2BIG;
			goto out;
		}

		if (ext->key_len)
			memcpy(ext->key, key, ext->key_len);
		else
			dwrq->flags |= IW_ENCODE_NOKEY;
		dwrq->flags |= IW_ENCODE_ENABLED;
	}
	ret = 0;

out:
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

/**
 *  @brief Set Encryption key Extended (WPA/802.1x and WEP)
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 --success, otherwise fail
 */
static int wlan_set_encodeext(struct net_device *dev,
			      struct iw_request_info *info,
			      struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int alg = ext->alg;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	if ((alg == IW_ENCODE_ALG_NONE) || (dwrq->flags & IW_ENCODE_DISABLED)) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "NO SEC\n");

		if (priv->imode != 3 && priv->imode != 5)
			disable_wep(priv);
		disable_wpa(priv);
	} else if (alg == IW_ENCODE_ALG_WEP) {
		u16 is_default = 0, index, set_tx_key = 0;

		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "WEP, flags = 0x%04x\n", dwrq->flags);

		ret = validate_key_index(priv->wep_tx_keyidx,
					 (dwrq->flags & IW_ENCODE_INDEX),
					 &index, &is_default);
		if (ret)
			goto out;

		/* If WEP isn't enabled, or if there is no key data but a valid
		 * index, or if the set-TX-key flag was passed, set the TX key.
		 */
		if (!priv->secinfo.wep_enabled
		    || (dwrq->length == 0 && !is_default)
		    || (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY))
			set_tx_key = 1;

		/* Copy key to driver */
		ret = copy_wep_key(priv, ext->key, ext->key_len, index, set_tx_key);
		if (ret)
			goto out;

		if (dwrq->flags & IW_ENCODE_RESTRICTED) {
			priv->secinfo.auth_mode = IW_AUTH_ALG_SHARED_KEY;
		} else if (dwrq->flags & IW_ENCODE_OPEN) {
			priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
		}

	} else if ((alg == IW_ENCODE_ALG_TKIP) || (alg == IW_ENCODE_ALG_CCMP)) {
		struct enc_key *pkey;

		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "TKIP or CCMP, flags = 0x%04x, alg = %d\n",
			    dwrq->flags, alg);

		/* validate key length */
		if (((alg == IW_ENCODE_ALG_TKIP)
		     && (ext->key_len != KEY_LEN_WPA_TKIP))
		    || ((alg == IW_ENCODE_ALG_CCMP)
			&& (ext->key_len != KEY_LEN_WPA_AES))) {
			WLAN_ERRP("invalid size %d for key of alg, type %d\n",
				  ext->key_len, alg);
			ret = -EINVAL;
			goto out;
		}

		/* Copy key to driver */
		if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			pkey = &priv->wpa_mcast_key;
		} else {
			pkey = &priv->wpa_unicast_key;
		}

		memset(pkey, 0, sizeof(struct enc_key));
		memcpy(pkey->key, ext->key, ext->key_len);
		pkey->len = ext->key_len;
		if (pkey->len)
			pkey->flags |= KEY_INFO_WPA_ENABLED;

		/* Do this after zeroing key structure */
		if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			pkey->flags |= KEY_INFO_WPA_MCAST;
		} else {
			pkey->flags |= KEY_INFO_WPA_UNICAST;
		}

		if (alg == IW_ENCODE_ALG_TKIP) {
			pkey->type = KEY_TYPE_ID_TKIP;
			if (!(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
			    && !(priv->imode & (BIT6))) {
				WLAN_ERRP("imode [0x%x] not match with cipher alg TKIP\n",
				     priv->imode);
			}
		} else if (alg == IW_ENCODE_ALG_CCMP) {
			pkey->type = KEY_TYPE_ID_AES;
			if (!(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
			    && !(priv->imode & (BIT5))) {
				WLAN_ERRP("imode [0x%x] not match with cipher alg CCMP\n",
				     priv->imode);
			}
		}

		/* If WPA isn't enabled yet, do that now */
		if (priv->secinfo.WPAenabled == 0
		    && priv->secinfo.WPA2enabled == 0) {
			priv->secinfo.WPAenabled = 1;
			priv->secinfo.WPA2enabled = 1;
		}

		/* Set Keys to MAC */
		if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			/* Set GTK */
			ret = wlan_set_gtk(priv, (dwrq->flags & IW_ENCODE_INDEX) - 1, ext->tx_seq, IW_ENCODE_SEQ_MAX_SIZE, pkey->key, pkey->len);
			if (ret)
				goto out;
		} else {
			pkey->flags |= KEY_INFO_WPA_UNICAST;
			/* Set PTK */
			ret = wlan_set_ptk(priv, pkey->key, pkey->len);
			if (ret)
				goto out;
		}

		/* Only disable wep if necessary: can't waste time here. */
		disable_wep(priv);
	} else if (alg == IW_ENCODE_ALG_SM4) {	//wapi
		if (ext->key_len != 32)
			goto out;

		priv->is_wapi = 1;

		/* Set Keys to MAC */
		if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
			unsigned char tmp[8];
			/* Set GTK */
			/* 
			 * 
			 * need to toggle among 1, 2, 3
			 */
			ret = wlan_set_gtk(priv, (dwrq->flags & IW_ENCODE_INDEX) - 1, tmp, IW_ENCODE_SEQ_MAX_SIZE, ext->key, ext->key_len);
			if (ret)
				goto out;
		} else {
			/* Set PTK */
			ret = wlan_set_ptk(priv, ext->key, ext->key_len);
			if (ret)
				goto out;
		}
	}

out:
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

/**
 *  @brief PMKSA cache operation (WPA/802.1x and WEP)
 *
 *  @param dev                  A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 on success, otherwise failure
 */
static int wlan_set_pmksa(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}

static int wlan_set_genie(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	if(priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F){
		unsigned char ie_len;
		//huanglei add wps
		ie_len = extra[1] + 2;
		wlan_generic_set_str(priv, WID_GEN_ASSOC_IE, extra ,ie_len);
	}

	if (extra[0] == 0x44)	//wapi ie
	{
		unsigned char ie_len = extra[1] + 2;
		wlan_generic_set_str(priv, WID_WAPI_ASSOC_IE, extra, ie_len);
		goto out;
	}

	if (dwrq->length > MAX_WPA_IE_LEN || (dwrq->length && extra == NULL)) {
		ret = -EINVAL;
		goto out;
	}

	if (dwrq->length) {
		memcpy(&priv->wpa_ie[0], extra, dwrq->length);
		priv->wpa_ie_len = dwrq->length;
	} else {
		memset(&priv->wpa_ie[0], 0, sizeof(priv->wpa_ie));
		priv->wpa_ie_len = 0;
	}

out:

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_get_genie(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	if (priv->wpa_ie_len == 0) {
		dwrq->length = 0;
		goto out;
	}

	if (dwrq->length < priv->wpa_ie_len) {
		ret = -E2BIG;
		goto out;
	}

	dwrq->length = priv->wpa_ie_len;
	memcpy(extra, &priv->wpa_ie[0], priv->wpa_ie_len);

out:
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_set_auth(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
		    "flags = 0x%04x, value = 0x%x\n", dwrq->flags, dwrq->value);

	switch (dwrq->flags & IW_AUTH_INDEX) {
	case IW_AUTH_CIPHER_PAIRWISE:
		if (dwrq->value & (IW_AUTH_CIPHER_WEP104 | IW_AUTH_CIPHER_WEP40)) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WEP Selected \n");
			priv->secinfo.wep_enabled = 1;
			if (dwrq->value & IW_AUTH_CIPHER_WEP104)
				priv->secinfo.cipther_type |= IW_AUTH_CIPHER_WEP104;
			else if (dwrq->value & IW_AUTH_CIPHER_WEP40)
				priv->secinfo.cipther_type |= IW_AUTH_CIPHER_WEP40;
		}
		if (dwrq->value & IW_AUTH_CIPHER_TKIP) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "IW_AUTH_CIPHER_TKIP \n");
			priv->secinfo.cipther_type |= IW_AUTH_CIPHER_TKIP;
		}
		if (dwrq->value & IW_AUTH_CIPHER_CCMP) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "IW_AUTH_CIPHER_CCMP \n");
			priv->secinfo.cipther_type |= IW_AUTH_CIPHER_CCMP;
		}
		if (dwrq->value & IW_AUTH_CIPHER_NONE) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "OPEN System \n");
			priv->secinfo.cipther_type = IW_AUTH_CIPHER_NONE;
		}
		break;

	case IW_AUTH_TKIP_COUNTERMEASURES:
	case IW_AUTH_CIPHER_GROUP:
	case IW_AUTH_DROP_UNENCRYPTED:
		/*
		 * wlan does not use these parameters
		 */
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "DO NOT USE\n");
		break;

	case IW_AUTH_KEY_MGMT:
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "KEY_MGMT, val = %d\n", dwrq->value);
		priv->secinfo.key_mgmt = dwrq->value;
		break;

	case IW_AUTH_WPA_VERSION:
		if (dwrq->value & IW_AUTH_WPA_VERSION_DISABLED) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WPA_VERSION, DISABLED\n");
			priv->secinfo.WPAenabled = 0;
			priv->secinfo.WPA2enabled = 0;
			disable_wpa(priv);
		}
		if (dwrq->value & IW_AUTH_WPA_VERSION_WPA) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WPA_VERSION, WPA\n");
			priv->secinfo.WPAenabled = 1;
			priv->secinfo.wep_enabled = 0;
			priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
		}
		if (dwrq->value & IW_AUTH_WPA_VERSION_WPA2) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WPA_VERSION, WPA2\n");
			priv->secinfo.WPA2enabled = 1;
			priv->secinfo.wep_enabled = 0;
			priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
		}
		break;

	case IW_AUTH_80211_AUTH_ALG:
		if (dwrq->value & IW_AUTH_ALG_SHARED_KEY ||
		    dwrq->value & IW_AUTH_ALG_OPEN_SYSTEM) {
			if (dwrq->value & IW_AUTH_ALG_SHARED_KEY) {
				WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
					    "80211_AUTH_ALG, SHARED_KEY\n");
				priv->secinfo.auth_mode |= IW_AUTH_ALG_SHARED_KEY;
			}
			if (dwrq->value & IW_AUTH_ALG_OPEN_SYSTEM) {
				WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
					    "80211_AUTH_ALG, OPEN\n");
				priv->secinfo.auth_mode |= IW_AUTH_ALG_OPEN_SYSTEM;
			}
		} else if (dwrq->value & IW_AUTH_ALG_LEAP) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "80211_AUTH_ALG, LEAP\n");
			priv->secinfo.auth_mode = IW_AUTH_ALG_LEAP;
		} else if (dwrq->value & IW_AUTH_ALG_WAPI) {
			priv->secinfo.auth_mode = IW_AUTH_ALG_WAPI;
		} else {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "80211_AUTH_ALG, unknown\n");
			ret = -EINVAL;
		}
		break;

	case IW_AUTH_WPA_ENABLED:
		if (dwrq->value) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WPA_ENABLED, value = 0x%x\n", dwrq->value);
			if (!priv->secinfo.WPAenabled &&
			    !priv->secinfo.WPA2enabled) {
				priv->secinfo.WPAenabled = 1;
				priv->secinfo.WPA2enabled = 1;
				priv->secinfo.wep_enabled = 0;
				priv->secinfo.auth_mode = IW_AUTH_ALG_OPEN_SYSTEM;
			}
		} else {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				    "WPA_ENABLED, value = ZERO\n");
			priv->secinfo.WPAenabled = 0;
			priv->secinfo.WPA2enabled = 0;
			disable_wpa(priv);
		}
		break;

	case IW_AUTH_WAPI_ENABLED:
		if (dwrq->value & BIT1) {
			disable_wpa(priv);
			disable_wep(priv);
			priv->secinfo.auth_mode = IW_AUTH_ALG_WAPI;
		} else if (dwrq->value & BIT2) {
			disable_wpa(priv);
			disable_wep(priv);
			priv->secinfo.auth_mode = IW_AUTH_ALG_WAPI;
		}
		break;

	default:
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "NOT SUPPORT\n");
		ret = -EOPNOTSUPP;
		break;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

static int wlan_get_auth(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_param *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);

	switch (dwrq->flags & IW_AUTH_INDEX) {
	case IW_AUTH_KEY_MGMT:
		dwrq->value = priv->secinfo.key_mgmt;
		break;

	case IW_AUTH_WPA_VERSION:
		dwrq->value = 0;
		if (priv->secinfo.WPAenabled)
			dwrq->value |= IW_AUTH_WPA_VERSION_WPA;
		if (priv->secinfo.WPA2enabled)
			dwrq->value |= IW_AUTH_WPA_VERSION_WPA2;
		if (!dwrq->value)
			dwrq->value |= IW_AUTH_WPA_VERSION_DISABLED;
		break;

	case IW_AUTH_80211_AUTH_ALG:
		dwrq->value = priv->secinfo.auth_mode;
		break;

	case IW_AUTH_WPA_ENABLED:
		if (priv->secinfo.WPAenabled && priv->secinfo.WPA2enabled)
			dwrq->value = 1;
		break;

	default:
		ret = -EOPNOTSUPP;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);
	return ret;
}

static int wlan_set_txpow(struct net_device *dev, struct iw_request_info *info,
			  struct iw_param *vwrq, char *extra)
{
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);
	return 0;
}

static int wlan_get_essid(struct net_device *dev, struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);

	memcpy(extra, priv->curbssparams.ssid, strlen(priv->curbssparams.ssid));
	dwrq->length = strlen(priv->curbssparams.ssid);
	extra[dwrq->length] = '\0';

	/*
	 * If none, we may want to get the one that was set
	 */

	dwrq->flags = 1;	/* active */

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>> \n", __func__);
	return 0;
}

void wlan_indicate_disconnected(wlan_private * priv)
{
	union iwreq_data wrqu;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_NORM, "%s <<<\n", __func__);

	memset(wrqu.ap_addr.sa_data, 0, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(priv->netDev, SIOCGIWAP, &wrqu, NULL);

	/*report disconnect to upper layer */
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_NORM, "%s >>>\n", __func__);
}

void wlan_indicate_connected(wlan_private * priv)
{
	union iwreq_data wrqu;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_NORM, "%s <<<\n", __func__);

	memcpy(wrqu.ap_addr.sa_data, priv->curbssparams.bssid, ETH_ALEN);
	wrqu.ap_addr.sa_family = ARPHRD_ETHER;
	wireless_send_event(priv->netDev, SIOCGIWAP, &wrqu, NULL);


	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_NORM, "%s >>> \n connect mac: %2x:%2x:%2x:%2x:%2x:%2x \n",
		    __func__, priv->curbssparams.bssid[0],
		    priv->curbssparams.bssid[1], priv->curbssparams.bssid[2],
		    priv->curbssparams.bssid[3], priv->curbssparams.bssid[4],
		    priv->curbssparams.bssid[5]);
}

struct bss_descriptor *get_bss_desc_from_scanlist(wlan_private * priv,
						  unsigned char *bssid)
{
	struct bss_descriptor *iter_bss;
	struct bss_descriptor *ret_bss = NULL;

	spin_lock(&priv->ScanListLock);
	/* report all bss to upper layer */
	list_for_each_entry(iter_bss, &priv->network_list, list) {
		if (memcmp(iter_bss->bssid, bssid, 6) == 0) {
			ret_bss = iter_bss;
			break;
		}
	}
	spin_unlock(&priv->ScanListLock);
	return ret_bss;
}

static int wlan_set_essid(struct net_device *dev, struct iw_request_info *info,
			  struct iw_point *dwrq, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;
	u8 ssid[IW_ESSID_MAX_SIZE + 1];
	u8 ssid_len = 0;
	int in_ssid_len = dwrq->length;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	/* Check the size of the string */
	if (in_ssid_len > IW_ESSID_MAX_SIZE) {
		ret = -E2BIG;
		goto out;
	}

	memset(&ssid, 0, sizeof(ssid));

	if (!dwrq->flags || !in_ssid_len) {
		/* "any" SSID requested; leave SSID blank */
	} else {
		/* Specific SSID requested */
		memcpy(&ssid, extra, in_ssid_len);
		ssid[in_ssid_len] = '\0';
		ssid_len = in_ssid_len;
	}

	if (!ssid_len) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "requested any SSID\n");
	} else {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "requested SSID len = %d ssid:%s\n",
			    ssid_len, ssid);
	}

	if (ssid_len) {
		memcpy(&priv->assoc_ssid[0], ssid, sizeof(ssid));
		priv->assoc_ssid_len = ssid_len;
#if 0
		priv->ToggalAssociation = FALSE;

		if (priv->StartAssociationTimeOut.timer_is_canceled)
			wlan_mod_timer(&priv->StartAssociationTimeOut, 100);
		priv->assoc_ongoing = TRUE;
#endif
	}

out:
	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return 0;
}


/**
 *  @brief Connect to the AP or Ad-hoc Network with specific bssid
 *
 *  @param dev          A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param awrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *  @return             0 --success, otherwise fail
 */

static int wlan_set_wap(struct net_device *dev, struct iw_request_info *info,
			struct sockaddr *awrq, char *extra)
{
	unsigned char *ap_addr = NULL;
	wlan_private *priv = (wlan_private *) netdev_priv(dev);

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<< \n", __func__);

	ap_addr = awrq->sa_data;
	if (!is_zero_eth_addr(ap_addr)) {
		priv->reassoc_count = 0;
		if (memcmp(priv->assoc_bssid, ap_addr, 6)) {
			memcpy(priv->assoc_bssid, ap_addr, 6);
			priv->ToggalAssociation = FALSE;
			if (!priv->ReAssociationTimeOut.timer_is_canceled) {
				wlan_cancel_timer(&priv->ReAssociationTimeOut);
			}
			if (priv->StartAssociationTimeOut.timer_is_canceled)
				wlan_mod_timer(&priv->StartAssociationTimeOut, 100);
		} else {
			if (priv->ReAssociationTimeOut.timer_is_canceled) {
				wlan_mod_timer(&priv->StartAssociationTimeOut, 100);
			}
		}
		priv->assoc_ongoing = TRUE;
	} else {
		memcpy(priv->assoc_bssid, ap_addr, 6);
		wlan_cancel_timer(&priv->ReAssociationTimeOut);
		wlan_cancel_timer(&priv->StartAssociationTimeOut);

		disable_wep(priv);
		disable_wpa(priv);
	}

	WLAN_ERRP("%s <<< \n connect mac: %2x:%2x:%2x:%2x:%2x:%2x \n", __func__,
		  ap_addr[0], ap_addr[1], ap_addr[2], ap_addr[3], ap_addr[4], ap_addr[5]);

	return 0;
}

unsigned char num_2_char(unsigned char num)
{
	if (num >= 0 && num <= 9) {
		return '0' + num;
	} else
		return 'a' + (num - 0x0a);
}

void num_2_str(unsigned char num, unsigned char *str)
{
	*str = num_2_char((num >> 4) & 0x0f);
	*(str + 1) = num_2_char(num & 0x0f);
}

static char *translate_scan(wlan_private * priv,
			    struct iw_request_info *info,
			    char *start, char *stop,
			    struct bss_descriptor *bss_desc)
{

	struct iw_event iwe;	/* Temporary buffer */
	u8 snr;
	struct bss_descriptor *bss = bss_desc;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "translate_scan, ssid = %s ssi=%d ssid_len=%d \n", bss->ssid, bss->rssi, bss->ssid_len);

	/* First entry *MUST* be the BSSID */
	iwe.cmd = SIOCGIWAP;
	iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
	memcpy(iwe.u.ap_addr.sa_data, bss->bssid, ETH_ALEN);
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_ADDR_LEN);

	/* SSID */
	iwe.cmd = SIOCGIWESSID;
	iwe.u.data.flags = 1;
	iwe.u.data.length = bss->ssid_len;
	start = iwe_stream_add_point(info, start, stop, &iwe, bss->ssid);

	/* Mode */
	iwe.cmd = SIOCGIWMODE;
	iwe.u.mode = bss->mode;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_UINT_LEN);

	/* Frequency */
	iwe.cmd = SIOCGIWFREQ;
	iwe.u.freq.m = (2412 + 5 * (bss->channel - 1)) * 100000;
	iwe.u.freq.e = 1;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_FREQ_LEN);

	/* Add quality statistics */
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.updated = IW_QUAL_ALL_UPDATED;
	iwe.u.qual.level = 0x100 + wlan_get_aver_rssi(priv, bss->bssid);

	snr = iwe.u.qual.level - WLAN_NF_DEFAULT_SCAN_VALUE;
	iwe.u.qual.qual =
	    (100 * RSSI_DIFF * RSSI_DIFF - (PERFECT_RSSI - snr) *
	     (15 * (RSSI_DIFF) + 62 * (PERFECT_RSSI - snr))) /
	    (RSSI_DIFF * RSSI_DIFF);
	if (iwe.u.qual.qual > 100)
		iwe.u.qual.qual = 100;
	iwe.u.qual.noise = WLAN_NF_DEFAULT_SCAN_VALUE;
	start = iwe_stream_add_event(info, start, stop, &iwe, IW_EV_QUAL_LEN);

	/* Add encryption capability */
	iwe.cmd = SIOCGIWENCODE;
	if (bss->capability & WLAN_CAPABILITY_PRIVACY) {
		iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
	} else {
		iwe.u.data.flags = IW_ENCODE_DISABLED;
	}
	iwe.u.data.length = 0;
	start = iwe_stream_add_point(info, start, stop, &iwe, bss->ssid);

	memset(&iwe, 0, sizeof(iwe));
	if (bss_desc->wpa_ie_len && !bss_desc->wapi_ie_len) {
		char buf[MAX_WPA_IE_LEN];

		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "translate_scan, wpa_ie, len %d\n",
			    bss_desc->wpa_ie_len);
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "%02x %02x %02x %02x ... ... %02x %02x %02x %02x\n",
			    bss_desc->wpa_ie[0], bss_desc->wpa_ie[1],
			    bss_desc->wpa_ie[2], bss_desc->wpa_ie[3],
			    bss_desc->wpa_ie[bss_desc->wpa_ie_len - 4],
			    bss_desc->wpa_ie[bss_desc->wpa_ie_len - 3],
			    bss_desc->wpa_ie[bss_desc->wpa_ie_len - 2],
			    bss_desc->wpa_ie[bss_desc->wpa_ie_len - 1]);

		memcpy(buf, bss->wpa_ie, bss->wpa_ie_len);
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = bss_desc->wpa_ie_len;
		start = iwe_stream_add_point(info, start, stop, &iwe, buf);
	}

	memset(&iwe, 0, sizeof(iwe));
	if (bss_desc->rsn_ie_len) {
		char buf[MAX_WPA_IE_LEN];

		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "translate_scan, rsn_ie, len %d\n",
			    bss_desc->rsn_ie_len);
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "%02x %02x %02x %02x ... ... %02x %02x %02x %02x\n",
			    bss_desc->rsn_ie[0], bss_desc->rsn_ie[1],
			    bss_desc->rsn_ie[2], bss_desc->rsn_ie[3],
			    bss_desc->rsn_ie[bss_desc->rsn_ie_len - 4],
			    bss_desc->rsn_ie[bss_desc->rsn_ie_len - 3],
			    bss_desc->rsn_ie[bss_desc->rsn_ie_len - 2],
			    bss_desc->rsn_ie[bss_desc->rsn_ie_len - 1]);

		memcpy(buf, bss->rsn_ie, bss->rsn_ie_len);
		iwe.cmd = IWEVGENIE;
		iwe.u.data.length = bss_desc->rsn_ie_len;
		start = iwe_stream_add_point(info, start, stop, &iwe, buf);
	}
	if(priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F){
		/* huanglei add for wps */
		memset(&iwe, 0, sizeof(iwe));
		if (bss_desc->wps_ie_len ) {
			char buf[MAX_WPS_IE_LEN];

			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
				"translate_scan, wps_ie, len %d\n", bss_desc->wps_ie_len);

			memcpy(buf, bss->wps_ie, bss->wps_ie_len);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = bss_desc->wps_ie_len;
			start = iwe_stream_add_point(info, start, stop, &iwe, buf);
		}
	}
	if (bss_desc->wapi_ie_len) {
		char buf[200];
		unsigned char pos = 0;

		memset(&iwe, 0, sizeof(iwe));
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "translate_scan, wapi_len %d\n",
			    bss_desc->wapi_ie_len);
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "%02x %02x %02x %02x ... ... %02x %02x %02x %02x\n",
			    bss_desc->wapi_ie[0], bss_desc->wapi_ie[1],
			    bss_desc->wapi_ie[2], bss_desc->wapi_ie[3],
			    bss_desc->wapi_ie[bss_desc->wapi_ie_len - 4],
			    bss_desc->wapi_ie[bss_desc->wapi_ie_len - 3],
			    bss_desc->wapi_ie[bss_desc->wapi_ie_len - 2],
			    bss_desc->wapi_ie[bss_desc->wapi_ie_len - 1]);

		memcpy(buf, "wapi_ie=", 8);

		while (pos < bss_desc->wapi_ie_len) {
			//transfer hex to ascii string because wpa_supplicant need do so
			num_2_str(bss_desc->wapi_ie[pos],
				  (unsigned char *)(buf + 8 + 2 * pos));
			pos++;
		}

		iwe.cmd = IWEVCUSTOM;
		iwe.u.data.length = bss_desc->wapi_ie_len * 2 + 8;
		start = iwe_stream_add_point(info, start, stop, &iwe, buf);
	}

	return start;
}

/**
 *  @brief Handle Scan Network ioctl
 *
 *  @param dev          A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param vwrq         A pointer to iw_param structure
 *  @param extra        A pointer to extra data buf
 *
 *  @return             0 --success, otherwise fail
 */
int wlan_set_scan(struct net_device *dev, struct iw_request_info *info,
		  union iwreq_data *wrqu, char *extra)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s scan:%d assoc:%d <<< \n",
		    __func__, priv->scan_running, priv->assoc_ongoing);

	if (!is_sdio_init_complete() || priv->scan_running == WLAN_SCAN_RUNNING
	    || priv->assoc_ongoing || priv->sdio_need_reset)
		goto out;

	if (wrqu->data.length == sizeof(struct iw_scan_req) &&
	    wrqu->data.flags & IW_SCAN_THIS_ESSID) {
		struct iw_scan_req *req = (struct iw_scan_req *)extra;
		priv->scan_ssid_len = req->essid_len > 32 ? 32 : req->essid_len;
		memcpy(priv->scan_ssid, req->essid, priv->scan_ssid_len);
		priv->scan_ssid[priv->scan_ssid_len] = '\0';
	} else {
		priv->scan_ssid_len = 0;
		memset(priv->scan_ssid, '\0', sizeof(priv->scan_ssid));
	}

	wlan_set_rssi_dirty(priv);
	priv->scan_running = WLAN_SCAN_RUNNING;
	ret = wlan_start_scan_enable_network_info(priv);
	if (!ret) {
		wlan_mod_timer(&priv->ScanResultsTimeout, 1500);
	} else {
		priv->scan_running = WLAN_SCAN_IDLE;
	}
out:

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s ret %d >>> \n", __func__, ret);
	return 0;
}


void  wlan_set_scan_by_driver(wlan_private *priv)
{
	int ret = 0;
	priv->scan_ssid_len = 0;
	memset(priv->scan_ssid, '\0', sizeof(priv->scan_ssid));
	priv->scan_running = WLAN_SCAN_RUNNING;
	ret = wlan_start_scan_enable_network_info(priv);
	if (!ret) {
		wlan_mod_timer(&priv->ScanResultsTimeout, 1500);
	} else {
		priv->scan_running = WLAN_SCAN_IDLE;
	}
}


static inline unsigned int elapsed_jiffies_msecs(unsigned long start)
{
	unsigned long end = jiffies;

	if (end >= start)
		return jiffies_to_msecs(end - start);

	return jiffies_to_msecs(end + (MAX_JIFFY_OFFSET - start) + 1);
}

/**
 *  @brief  Handle Retrieve scan table ioctl
 *
 *  @param dev          A pointer to net_device structure
 *  @param info         A pointer to iw_request_info structure
 *  @param dwrq         A pointer to iw_point structure
 *  @param extra        A pointer to extra data buf
 *
 *  @return             0 --success, otherwise fail
 */
int wlan_get_scan(struct net_device *dev, struct iw_request_info *info,
		  struct iw_point *dwrq, char *extra)
{
#define SCAN_ITEM_SIZE 128
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	int ret = 0;
	struct bss_descriptor *iter_bss;
	struct bss_descriptor *safe;
	char *ev = extra;
	char *stop = ev + dwrq->length;
	u8 items = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_DEBUG, "%s >>>\n", __func__);

	/* iwlist should wait until the current scan is finished */
	if (priv->scan_running == WLAN_SCAN_RUNNING) {
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "Scan is Running, return AGAIN\n");
		return -EAGAIN;
	}

	spin_lock(&priv->ScanListLock);
	/* report all bss to upper layer */
	list_for_each_entry_safe(iter_bss, safe, &priv->network_list, list) {
		char *next_ev;
		unsigned long stale_time;
		

		if (stop - ev < SCAN_ITEM_SIZE) {
			ret = -E2BIG;
			break;
		}

		if (!iter_bss->ssid_len){
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,"No valid AP here\n");
			continue;
		}

		/* Prune old an old scan result */
		stale_time = iter_bss->last_scanned + DEFAULT_MAX_SCAN_AGE;
		if (time_before(stale_time, jiffies)) {
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
						"Not show %s, due to age (%ums).\n", iter_bss->ssid,
						elapsed_jiffies_msecs(iter_bss->last_scanned));
			clear_bss_descriptor(iter_bss);
			continue;
		}

#ifdef WIFI_SELECT_CHANNEL
		if (iter_bss->channel > channel_nums) {
			clear_bss_descriptor(iter_bss);
			continue;
		}
#endif
		/* Translate to WE format this entry */
		next_ev = translate_scan(priv, info, ev, stop, iter_bss);
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "Report BSS %s\n", iter_bss->ssid);

		if (next_ev == NULL)
			continue;
		ev = next_ev;
		items++;
	}
	spin_unlock(&priv->ScanListLock);
	dwrq->length = (ev - extra);
	dwrq->flags = 0;

	if (priv->scan_running != WLAN_SCAN_RUNNING)
		priv->scan_running = WLAN_SCAN_IDLE;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s ap:%d< <<\n", __func__, items);
	return ret;
}

int wlan_set_mlme(struct net_device *dev,
		  struct iw_request_info *info,
		  union iwreq_data *wrqu, char *extra)
{
	struct iw_mlme *mlme = (struct iw_mlme *)extra;
	wlan_private *priv = netdev_priv(dev);
	int ret = 0;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	switch (mlme->cmd) {
	case IW_MLME_DEAUTH:
	case IW_MLME_DISASSOC:
		{
			unsigned char null_data[6];
			memset(null_data, 0, 6);
			WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "DISASSOC\n");

			wlan_cancel_timer(&priv->AssociationTimeOut);
			wlan_cancel_timer(&priv->ReAssociationTimeOut);
			priv->assoc_ongoing = FALSE;

			/* silently ignore */
			if(priv->version >= WLAN_VERSION_91){
				//wlan_set_bssid(priv, null_data);
				//wlan_disconnect(priv);
				ret = wlan_disconnect_silent(priv);
			}
			else
				ret = wlan_set_ssid((wlan_private *) netdev_priv(dev), null_data, 6);
			
		}
                         break;
	default:
		WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE,
			    "Not supported cmd %d\n", mlme->cmd);
		ret = -EOPNOTSUPP;
                        break;
	}

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s <<<\n", __func__);
	return ret;
}

#define MAX_WX_STRING 80
static int wl_iw_get_macaddr(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int error;
	char *p = extra;
	char buf[ETH_ALEN] = { 0 };
	wlan_private *priv = netdev_priv(dev);

	error = wlan_get_mac_addr(priv, buf);
	p += snprintf(p, MAX_WX_STRING,
		      "Macaddr = %02X:%02X:%02X:%02X:%02X:%02X\n", buf[0],
		      buf[1], buf[2], buf[3], buf[4], buf[5]);

	wrqu->data.length = p - extra + 1;

	return error;
}

static int wlan_set_priv(struct net_device *dev, struct iw_request_info *info,
			 struct iw_point *dwrq, char *ext)
{
	int ret = 0;
	char *extra;

	if (!(extra = kmalloc(dwrq->length, GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(extra, dwrq->pointer, dwrq->length)) {
		kfree(extra);
		return -EFAULT;
	}

	if (dwrq->length && extra) {
		if (strnicmp(extra, "MACADDR", strlen("MACADDR")) == 0) {
			ret =
			    wl_iw_get_macaddr(dev, info,
					      (union iwreq_data *)dwrq, extra);
		} else if (strnicmp(extra, "CSCAN", strlen("CSCAN")) == 0) {
		}
	}

	if (extra) {
		if (copy_to_user(dwrq->pointer, extra, dwrq->length)) {
			kfree(extra);
			return -EFAULT;
		}
		kfree(extra);
	}

	return ret;
}

static int wlan_wext_get_wireless_stats(struct net_device *dev,
					struct iw_request_info *info,
					u32 * uwrq, char *extra)
{
	struct iw_statistics *wstats = (struct iw_statistics *)extra;
	wlan_private *priv = netdev_priv(dev);
	int ret = 0;
	int stats_valid = 0;
	u8 snr;

	WLAN_DBGLAP(WLAN_DA_WEXT, WLAN_DL_TRACE, "%s >>>\n", __func__);

	ret = wlan_update_bss_stats(priv);
	if (ret)
		goto out;

	wstats->miss.beacon = 0;
	wstats->discard.retries = 0;
	wstats->qual.level = 0x100 + (s8)priv->curbssparams.rssi;

	snr = wstats->qual.level - WLAN_NF_DEFAULT_SCAN_VALUE;
	wstats->qual.qual =
	    (100 * RSSI_DIFF * RSSI_DIFF - (PERFECT_RSSI - snr) *
	     (15 * (RSSI_DIFF) + 62 * (PERFECT_RSSI - snr))) /
	    (RSSI_DIFF * RSSI_DIFF);
	if (wstats->qual.qual > 100)
		wstats->qual.qual = 100;
	wstats->qual.noise = WLAN_NF_DEFAULT_SCAN_VALUE;
	wstats->qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;

	stats_valid = 1;

out:
	if (!stats_valid) {
		wstats->miss.beacon = 0;
		wstats->discard.retries = 0;
		wstats->qual.qual = 0;
		wstats->qual.level = 0;
		wstats->qual.noise = 0;
		wstats->qual.updated = IW_QUAL_ALL_UPDATED;
		wstats->qual.updated |= IW_QUAL_NOISE_INVALID |
		    IW_QUAL_QUAL_INVALID | IW_QUAL_LEVEL_INVALID;
	}
	return ret;
}

/*
 * iwconfig settable callbacks
 */
static const iw_handler wlan_wext_handler[] = {
	(iw_handler) NULL,	/* SIOCSIWCOMMIT */
	(iw_handler) wlan_get_name,	/* SIOCGIWNAME */
	(iw_handler) NULL,	/* SIOCSIWNWID */
	(iw_handler) NULL,	/* SIOCGIWNWID */
	(iw_handler) wlan_set_freq,	/* SIOCSIWFREQ */
	(iw_handler) wlan_get_freq,	/* SIOCGIWFREQ */
	(iw_handler) wlan_set_mode,	/* SIOCSIWMODE */
	(iw_handler) wlan_get_mode,	/* SIOCGIWMODE */
	(iw_handler) NULL,	/* SIOCSIWSENS */
	(iw_handler) NULL,	/* SIOCGIWSENS */
	(iw_handler) NULL,	/* SIOCSIWRANGE */
	(iw_handler) wlan_get_range,	/* SIOCGIWRANGE */
	(iw_handler) wlan_set_priv,	/* SIOCSIWPRIV */
	(iw_handler) NULL,	/* SIOCGIWPRIV */
	(iw_handler) NULL,	/* SIOCSIWSTATS */
	(iw_handler) wlan_wext_get_wireless_stats,	/* SIOCGIWSTATS */
	(iw_handler) NULL,	/* SIOCSIWSPY */
	(iw_handler) NULL,	/* SIOCGIWSPY */
	(iw_handler) NULL,	/* SIOCSIWTHRSPY */
	(iw_handler) NULL,	/* SIOCGIWTHRSPY */
	(iw_handler) wlan_set_wap,	/* SIOCSIWAP */
	(iw_handler) wlan_get_wap,	/* SIOCGIWAP */
	(iw_handler) wlan_set_mlme,	/* SIOCSIWMLME */
	(iw_handler) NULL,	/* SIOCGIWAPLIST - deprecated */
	(iw_handler) wlan_set_scan,	/* SIOCSIWSCAN */
	(iw_handler) wlan_get_scan,	/* SIOCGIWSCAN */
	(iw_handler) wlan_set_essid,	/* SIOCSIWESSID */
	(iw_handler) wlan_get_essid,	/* SIOCGIWESSID */
	(iw_handler) wlan_set_nick,	/* SIOCSIWNICKN */
	(iw_handler) wlan_get_nick,	/* SIOCGIWNICKN */
	(iw_handler) NULL,	/* -- hole -- */
	(iw_handler) NULL,	/* -- hole -- */
	(iw_handler) wlan_set_rate,	/* SIOCSIWRATE */
	(iw_handler) wlan_get_rate,	/* SIOCGIWRATE */
	(iw_handler) wlan_set_rts,	/* SIOCSIWRTS */
	(iw_handler) wlan_get_rts,	/* SIOCGIWRTS */
	(iw_handler) wlan_set_frag,	/* SIOCSIWFRAG */
	(iw_handler) wlan_get_frag,	/* SIOCGIWFRAG */
	(iw_handler) wlan_set_txpow,	/* SIOCSIWTXPOW */
	(iw_handler) wlan_get_txpow,	/* SIOCGIWTXPOW */
	(iw_handler) wlan_set_retry,	/* SIOCSIWRETRY */
	(iw_handler) wlan_get_retry,	/* SIOCGIWRETRY */
	(iw_handler) wlan_set_encode,	/* SIOCSIWENCODE */
	(iw_handler) wlan_get_encode,	/* SIOCGIWENCODE */
	(iw_handler) wlan_set_power,	/* SIOCSIWPOWER */
	(iw_handler) wlan_get_power,	/* SIOCGIWPOWER */
	(iw_handler) NULL,	/* -- hole -- */
	(iw_handler) NULL,	/* -- hole -- */
	(iw_handler) wlan_set_genie,	/* SIOCSIWGENIE */
	(iw_handler) wlan_get_genie,	/* SIOCGIWGENIE */
	(iw_handler) wlan_set_auth,	/* SIOCSIWAUTH */
	(iw_handler) wlan_get_auth,	/* SIOCGIWAUTH */
	(iw_handler) wlan_set_encodeext,	/* SIOCSIWENCODEEXT */
	(iw_handler) wlan_get_encodeext,	/* SIOCGIWENCODEEXT */
	(iw_handler) wlan_set_pmksa,	/* SIOCSIWPMKSA */
};

struct iw_handler_def wlan_wext_handler_def = {
	.num_standard = ARRAY_SIZE(wlan_wext_handler),
	.standard = (iw_handler *) wlan_wext_handler,
	.get_wireless_stats = wlan_get_wireless_stats,
};
