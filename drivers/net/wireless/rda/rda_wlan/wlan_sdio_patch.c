#include "wlan_includes.h"
#include "wlan_sdio_patch_90.h"
#include "wlan_sdio_patch_91.h"
#include "wlan_sdio_patch_91e.h"
#include "wlan_sdio_patch_91f.h"

static u8 sdio_patch_complete = 0;
static atomic_t sdio_init_complete = ATOMIC_INIT(0);

int wlan_get_fw_version_polling(wlan_private *priv, u32* version)
{
	int ret = 0;

	ENTER();

	ret = wlan_generic_get_ulong(priv, WID_SYS_FW_VER, (u8*)version);
	if (ret) {
		WLAN_ERRP("get version failed \n");
		goto out;
	}

	WLAN_ERRP("version %x \n", *version);

out:
	LEAVE();
	return ret;
}

int wlan_write_sdio32_polling(wlan_private * priv, const u32(*data)[2],
			      u32 size)
{
	int count = size, index = 0;
	int ret = 0;

	ENTER();
	//each time write five init data
	for(index = 0; index < count/8; index ++){
		ret = wlan_set_core_init_patch(priv, (const unsigned int (*)[2])&data[8*index][0], 8);
		if(ret)
			goto err;
		WLAN_DBGLAP(WLAN_DA_PM, WLAN_DL_DEBUG, "index:%d\n", index);
	}

	if(count%8 > 0){
		ret = wlan_set_core_init_patch(priv, (const unsigned int (*)[2])&data[8*index][0], count%8);
		if(ret)
			goto err;
	}	
err:
	LEAVE();
	return ret;
}

int wlan_write_sdio8_polling(wlan_private *priv, const u8 (*data)[2], u32 size)
{
	int count = size, index = 0;
	int ret = 0;

	ENTER();
	//each time write five init data
	for(index = 0; index < count/8; index ++){
		WLAN_DBGLAP(WLAN_DA_PM, WLAN_DL_DEBUG, "index:%d\n", index);
		ret = wlan_set_core_patch(priv, (const unsigned char (*)[2])data[8*index], 8);
		if(ret)
			goto err;
	}

	if(count%8 > 0){
		ret = wlan_set_core_patch(priv, (const unsigned char (*)[2])data[8*index], count%8);
		if(ret)
			goto err;
	}	
	
err:
	LEAVE();
	return ret;
}

int wlan_sdio_patch_core_32(wlan_private *priv)
{
	int ret = 0;

	ENTER();

	if (priv->version == WLAN_VERSION_90_D) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_patch_data_32_90_D,
					      sizeof(wifi_core_patch_data_32_90_D) /
					      sizeof(wifi_core_patch_data_32_90_D[0]));
	} else if (priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_patch_data_32_90_E,
					      sizeof(wifi_core_patch_data_32_90_E) /
					      sizeof(wifi_core_patch_data_32_90_E[0]));
	} else if (priv->version == WLAN_VERSION_91) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_patch_data_32_91,
					      sizeof(wifi_core_patch_data_32_91)/
					      sizeof(wifi_core_patch_data_32_91[0]));

		ret = wlan_write_sdio32_polling(priv, wifi_clock_switch_91,
						sizeof(wifi_clock_switch_91) /
						sizeof(wifi_clock_switch_91[0]));
	}else if (priv->version == WLAN_VERSION_91_E) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_patch_data_32_91e,
					      sizeof(wifi_core_patch_data_32_91e)/
					      sizeof(wifi_core_patch_data_32_91e[0]));

		ret = wlan_write_sdio32_polling(priv, wifi_clock_switch_91e,
						sizeof(wifi_clock_switch_91e) /
						sizeof(wifi_clock_switch_91e[0]));
	}else if (priv->version == WLAN_VERSION_91_F) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_patch_data_32_91f,
					      sizeof(wifi_core_patch_data_32_91f)/
					      sizeof(wifi_core_patch_data_32_91f[0]));
		ret = wlan_write_sdio32_polling(priv, wifi_clock_switch_91f,
						sizeof(wifi_clock_switch_91f) /
						sizeof(wifi_clock_switch_91f[0]));
	}
	LEAVE();
	return ret;
}

int wlan_sdio_init_core_polling(wlan_private *priv)
{
	int ret = 0;

	ENTER();
	if(priv->version == WLAN_VERSION_90_D)				
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_90_D, 
					      sizeof(wifi_core_init_data_32_90_D) /
					      sizeof(wifi_core_init_data_32_90_D[0]));
	else if(priv->version == WLAN_VERSION_90_E)
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_90_E, 
					      sizeof(wifi_core_init_data_32_90_E) /
					      sizeof(wifi_core_init_data_32_90_E[0]));
	else if(priv->version == WLAN_VERSION_91){
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91, 
						sizeof(wifi_core_init_data_32_91) /
						sizeof(wifi_core_init_data_32_91[0]));
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91, 
					      sizeof(wifi_core_AM_PM_data_32_91)/
					      sizeof(wifi_core_AM_PM_data_32_91[0]));
	}else if (priv->version == WLAN_VERSION_91_E) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91e,
						sizeof(wifi_core_init_data_32_91e) /
						sizeof(wifi_core_init_data_32_91e[0]));
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91e,
					      sizeof(wifi_core_AM_PM_data_32_91e)/
					      sizeof(wifi_core_AM_PM_data_32_91e[0]));
	}else if (priv->version == WLAN_VERSION_91_F) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91f,
						sizeof(wifi_core_init_data_32_91f) /
						sizeof(wifi_core_init_data_32_91f[0]));
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91f,
					      sizeof(wifi_core_AM_PM_data_32_91f)/
					      sizeof(wifi_core_AM_PM_data_32_91f[0]));
	}
	LEAVE();
	return ret;
}

int wlan_sdio_core_wake_mode_polling(wlan_private *priv)
{
	int ret = 0;

	if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E || priv->version == WLAN_VERSION_91_F)
		return 0;
		
	ret = wlan_write_sdio32_polling(priv, wifi_core_data_wake, 
					sizeof(wifi_core_data_wake) /
					sizeof(wifi_core_data_wake[0]));
	return ret;
}

int wlan_sdio_patch_core_8_polling(wlan_private *priv)
{
	int ret = 0;
	//for patch in byte mode
	
	ENTER();

	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_90_8,
					     sizeof(wifi_core_patch_data_90_8) /
					     sizeof(wifi_core_patch_data_90_8[0]));
		if(ret)
			goto err; 
	}else if(priv->version == WLAN_VERSION_91){
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91_8,
					       sizeof(wifi_core_patch_data_91_8)/
					       sizeof(wifi_core_patch_data_91_8[0]));
		if (ret)
			goto err;
	}else if (priv->version == WLAN_VERSION_91_E) {
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91e_8,
					       sizeof(wifi_core_patch_data_91e_8)/
					       sizeof(wifi_core_patch_data_91e_8[0]));
		if(ret)
			goto err; 
	}else if (priv->version == WLAN_VERSION_91_F) {
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91f_8,
					       sizeof(wifi_core_patch_data_91f_8)/
					       sizeof(wifi_core_patch_data_91f_8[0]));
		if (ret)
			goto err;
	}

	//for patch in wake continue clock mode
	ret = wlan_sdio_core_wake_mode_polling(priv);
	if(ret)
		goto err;  

err:
	LEAVE();
	return ret;
}

int wlan_sdio_set_default_notch_polling(wlan_private *priv)
{
	int ret = 0;

	ENTER();

	if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E || priv->version == WLAN_VERSION_91_F)
		return 0;
		
	if(priv->version == WLAN_VERSION_90_D) 
		ret = wlan_write_sdio32_polling(priv, wifi_notch_data_90_D, 
						sizeof(wifi_notch_data_90_D) /
						sizeof(wifi_notch_data_90_D[0]));
	else if(priv->version == WLAN_VERSION_90_E)
		ret = wlan_write_sdio32_polling(priv, wifi_notch_data_90_E, 
						sizeof(wifi_notch_data_90_E) /
						sizeof(wifi_notch_data_90_E[0]));
	LEAVE();
	
	return ret;
}

extern void rda_mci_enable_sdio_irq(struct mmc_host *mmc, int enable);
int wlan_sdio_init(wlan_private *priv)
{
	int ret = 0;

	atomic_set(&sdio_init_complete, 0);
	sdio_patch_complete = 0;

	ret = wlan_sdio_patch_core_32(priv);
	if(ret < 0)
		goto err;
	

	sdio_patch_complete = 1;
	wlan_sched_timeout(10);   //10ms delay

	ret = wlan_sdio_init_core_polling(priv);
	if(ret)
		goto err;

	ret = wlan_sdio_patch_core_8_polling(priv);
	if(ret < 0)
		goto err;
		
	ret = wlan_sdio_set_default_notch_polling(priv);
	if(ret < 0)
		goto err;

	atomic_set(&sdio_init_complete, 1);
	//wlan_register_host_wake_irq(priv);	
	rda_mci_enable_sdio_irq(priv->MmcCard->host, 1);
	priv->sdio_irq_enable = TRUE;
	
err:
	return ret;
}

u8 is_sdio_init_complete(void)
{
	return atomic_read(&sdio_init_complete);
}
u8 is_sdio_patch_complete(void) //after patch complete need check write flow
{
	return sdio_patch_complete;


	
}

int wlan_set_test_mode(wlan_private *priv)
{
	int ret = 0;
	ENTER();
	atomic_set(&sdio_init_complete, 0);
	sdio_patch_complete = 0;
	ret = wlan_sdio_patch_core_32(priv);
	if(ret)
		goto err;
	
	sdio_patch_complete = 1;

	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_90_8, 
					     sizeof(wifi_core_patch_data_90_8)/
					     sizeof(wifi_core_patch_data_90_8[0]));
		if(ret)
			goto err; 
	}else if(priv->version == WLAN_VERSION_91){
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91,
						sizeof(wifi_core_init_data_32_91) /
						sizeof(wifi_core_init_data_32_91[0]));
		if (ret)
			goto err;
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91,
					      sizeof(wifi_core_AM_PM_data_32_91)/
					      sizeof(wifi_core_AM_PM_data_32_91[0]));
		if (ret)
			goto err;
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91_8, 
					       sizeof(wifi_core_patch_data_91_8)/
					       sizeof(wifi_core_patch_data_91_8[0]));
		if(ret)
			goto err; 

	}else if (priv->version == WLAN_VERSION_91_E) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91e,
						sizeof(wifi_core_init_data_32_91e) /
						sizeof(wifi_core_init_data_32_91e[0]));
		if(ret)
			goto err;
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91e,
					      sizeof(wifi_core_AM_PM_data_32_91e)/
					      sizeof(wifi_core_AM_PM_data_32_91e[0]));
		if(ret)
			goto err;	

		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91e_8,
					       sizeof(wifi_core_patch_data_91e_8)/
					       sizeof(wifi_core_patch_data_91e_8[0]));
		if(ret)
		goto err;
	}else if (priv->version == WLAN_VERSION_91_F) {
		ret = wlan_write_sdio32_polling(priv, wifi_core_init_data_32_91f,
						sizeof(wifi_core_init_data_32_91f) /
						sizeof(wifi_core_init_data_32_91f[0]));
		if (ret)
			goto err;
		ret = wlan_write_sdio32_polling(priv, wifi_core_AM_PM_data_32_91f,
					      sizeof(wifi_core_AM_PM_data_32_91f)/
					      sizeof(wifi_core_AM_PM_data_32_91f[0]));
		if (ret)
			goto err;
		ret = wlan_write_sdio8_polling(priv, wifi_core_patch_data_91f_8,
					       sizeof(wifi_core_patch_data_91f_8)/
					       sizeof(wifi_core_patch_data_91f_8[0]));
		if (ret)
			goto err;
	}
	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio32_polling(priv, wlan_test_mode_digital32_90,
					      sizeof(wlan_test_mode_digital32_90) /
					      sizeof(wlan_test_mode_digital32_90[0]));
		if(ret)
			goto err;
	}
	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio32_polling(priv,
					      wifi_test_mode_agc_patch32_90,
					      sizeof(wifi_test_mode_agc_patch32_90) /
					      sizeof(wifi_test_mode_agc_patch32_90[0]));
		if(ret)
			goto err;
	}

	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_write_sdio32_polling(priv,
					      wifi_test_mode_rx_notch_32_90,
					      sizeof(wifi_test_mode_rx_notch_32_90) /
					      sizeof(wifi_test_mode_rx_notch_32_90[0]));
		if(ret)
		goto err;
	}

	ret = wlan_sdio_set_default_notch_polling(priv);
	if(ret)
	goto err;  
	
	atomic_set(&sdio_init_complete, 1);
	rda_mci_enable_sdio_irq(priv->MmcCard->host, 1);
	priv->sdio_irq_enable = TRUE;
	 
err:
	
	LEAVE();
	return ret;
} 
int wlan_assoc_power_save(wlan_private * priv)
{
	int ret = 0;
	ENTER();
	if (priv->version == WLAN_VERSION_91){
		ret = wlan_set_core_init_patch(priv, wifi_assoc_power_save_data_32_91, 
								sizeof(wifi_assoc_power_save_data_32_91) / sizeof(wifi_assoc_power_save_data_32_91[0]));
	}

	LEAVE();
	return ret;
}
int wlan_set_phy_timeout(wlan_private * priv)
{
	int ret = 0;
	if ((priv->version == WLAN_VERSION_90_D) ||
			(priv->version == WLAN_VERSION_90_E)) {
		ret = wlan_write_sdio32_polling(priv,
				wifi_phy_timeout_cfg_90,
				sizeof(wifi_phy_timeout_cfg_90) /
				sizeof(wifi_phy_timeout_cfg_90[0]));
	} else if ((priv->version == WLAN_VERSION_91) ||
			(priv->version == WLAN_VERSION_91_E) || priv->version == WLAN_VERSION_91_F) {
		ret = wlan_write_sdio32_polling(priv,
				wifi_phy_timeout_cfg_91e,
				sizeof(wifi_phy_timeout_cfg_91e) /
				sizeof(wifi_phy_timeout_cfg_91e[0]));
	}
	return ret;
}

