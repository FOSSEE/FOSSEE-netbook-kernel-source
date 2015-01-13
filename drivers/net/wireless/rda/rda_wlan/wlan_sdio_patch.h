#ifndef __WLAN_SDIO_PATCH_H__
#define __WLAN_SDIO_PATCH_H__

int wlan_sdio_init(wlan_private *priv);
u8 is_sdio_init_complete(void);
u8 is_sdio_patch_complete(void) ;
int wlan_set_test_mode(wlan_private *priv);
int wlan_assoc_power_save(wlan_private * priv);
int wlan_set_phy_timeout(wlan_private * priv);

#endif//__WLAN_SDIO_PATCH_H__

