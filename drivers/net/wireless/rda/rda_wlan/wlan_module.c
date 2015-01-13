#include "wlan_includes.h"
#include <mach/gpio.h>
//#include <mach/sys_config.h>
#include <linux/gpio.h>
///
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
///
#include <mach/irqs.h>
#include <linux/mmc/sdio.h>
#include <linux/gpio.h>
#include <mach/wmt_iomux.h>
#if 1
#define SDIO_VENDOR_ID_RDA		  0x5449
#define SDIO_DEVICE_ID_RDA		  0x0145


int wmt_mtk6620_intr=0xf; //gpio 15

extern void rda_mci_enable_sdio_irq(struct mmc_host *mmc, int enable);
int wlan_dbg_level = WLAN_DL_DEBUG;

int wlan_dbg_area = WLAN_DA_MAIN
	| WLAN_DA_SDIO
	| WLAN_DA_ETHER
	| WLAN_DA_WID
	| WLAN_DA_WEXT
	| WLAN_DA_TXRX
	| WLAN_DA_PM
	;
/* Module parameters */
//module_param_named(debug_level, wlan_dbg_level, int, 0644); 
//module_param_named(debug_area, wlan_dbg_area, int, 0644); 

static const struct sdio_device_id if_sdio_ids[] = {
	{ SDIO_DEVICE(SDIO_VENDOR_ID_RDA, SDIO_DEVICE_ID_RDA) },
	{ /* end: all zeroes */ 					},
};
MODULE_DEVICE_TABLE(sdio, if_sdio_ids);

static int wlan_dev_open (struct net_device *dev)
{
	int ret = 0;
	wlan_private *priv = NULL;

	priv = (wlan_private *)netdev_priv(dev);

	if(!priv)
		return -EPERM;

	priv->Open = TRUE;

	netif_carrier_on(priv->netDev);
	netif_wake_queue(priv->netDev);

	return ret;
}

static int wlan_eth_stop(struct net_device *dev)
{
	int ret = 0;
	wlan_private *priv = NULL;

	priv = (wlan_private *)netdev_priv(dev);

	if(!priv)
		return -EPERM; 
	
	ENTER();
	
	if (!netif_queue_stopped(priv->netDev)){
		netif_stop_queue(priv->netDev);
	}

	if (netif_carrier_ok(priv->netDev)){
		netif_carrier_off(priv->netDev);
	}

	LEAVE();
	return ret;
}

static int wlan_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret = 0;
	wlan_private *priv = NULL;
	wlan_tx_packet_node * txNode = NULL;
	unsigned long flags = 0;
#ifdef WLAN_RAW_DATA_DEBUG		
	u8 *printStr = NULL;
#endif
	int tx_queue_max_num =0;

	ENTER();

	if(rda_combo_wifi_in_test_mode()){
		dev_kfree_skb(skb);
		return 0;
	}
	
	priv = (wlan_private *)netdev_priv(dev);
	if(!priv || !is_sdio_init_complete()){
		WLAN_ERRP("device is not opened \n");
		return -EPERM;
	}

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_90;
	} else if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E
		|| priv->version == WLAN_VERSION_91_F) {
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_91;
	} else {
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_90;
		WLAN_ERRP("WLAN_TX_QUEUE_NUM doesn't set for this version\n");
	}

	if(atomic_read(&(priv->TxQuNum)) >= tx_queue_max_num || priv->CardRemoved){
		WLAN_ERRP("queue is full, priv->TxQuNum=%d \n", atomic_read(&(priv->TxQuNum)));
		return -ENOMEM;
	}

	txNode = (wlan_tx_packet_node*)kzalloc(sizeof(wlan_tx_packet_node), GFP_ATOMIC);
	if(!txNode){
		WLAN_ERRP("no memory \n");
		return -ENOMEM;
	}
	
	WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_DEBUG,"skb headroom: %d %d len;%d \n", skb_headroom(skb), atomic_read(&priv->netifQuStop), skb->len);

#ifdef WLAN_RAW_DATA_DEBUG	  
	printStr = skb->data;
	WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1],printStr[2],printStr[3],printStr[4],printStr[5],printStr[6],printStr[7]);
	printStr += 8;
	WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1],printStr[2],printStr[3],printStr[4],printStr[5],printStr[6],printStr[7]);
	printStr += 8;
	WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1],printStr[2],printStr[3],printStr[4],printStr[5],printStr[6],printStr[7]);	
#endif

	txNode->type = WLAN_DATA;
	if(IS_ALIGNED((int)skb->data - WID_HEADER_LEN, 4))
		txNode->Skb = skb;
	else{
		txNode->Skb = dev_alloc_skb(skb->len + 4 + WID_HEADER_LEN);
		if(txNode->Skb){
			skb_align(txNode->Skb, 4);
			skb_reserve(txNode->Skb , WID_HEADER_LEN);
			memcpy(txNode->Skb->data, skb->data, skb->len);
			skb_put(txNode->Skb, skb->len);
		}
		dev_kfree_skb(skb);

		if(!txNode->Skb){
			return -ENOMEM;
		}
	}

	spin_lock_irqsave(&priv->TxLock, flags);
	list_add_tail(&txNode->List, &priv->TxQueue);
	spin_unlock_irqrestore(&priv->TxLock, flags);
	atomic_add(1, &(priv->TxQuNum));
	
	if(atomic_read(&(priv->TxQuNum)) >= tx_queue_max_num){
		netif_stop_queue(dev);
		atomic_set(&(priv->netifQuStop), 1);
	}

	dev->trans_start = jiffies;

	complete(&priv->TxThread.comp);

	LEAVE();
	return ret;
}

static int wlan_set_mac_address(struct net_device *dev, void *addr)
{
	int ret = 0;
	return ret;
}

static void wlan_tx_timeout (struct net_device *dev)
{
	ENTER();
	
	dev->trans_start = jiffies; /* prevent tx timeout */
	netif_wake_queue(dev);
	dev->stats.tx_errors++;

	LEAVE();
}

static struct net_device_stats *wlan_get_stats(struct net_device *dev)
{
	wlan_private *priv = (wlan_private *) netdev_priv(dev);

	return &priv->stats;
}

#define BT_COEXIST  SIOCIWFIRSTPRIV + 2
#define BT_STATE_SCO_ON  0x01
#define BT_STATE_SCO_OFF  0x02
#define BT_STATE_SCO_ONGOING 0x04
#define BT_STATE_A2DP_PLAYING  0x08
#define BT_STATE_A2DP_NO_PLAYING 0x10
#define BT_STATE_CONNECTION_ON 0x20
#define BT_STATE_CONNECTION_OFF 0x40
static int wlan_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	int ret = -EOPNOTSUPP;
	static int bt_state = 0;
	int state = 0, old_state = 0;
	struct pta_param_s pta_param;
	wlan_private *priv = (wlan_private *) netdev_priv(dev);
	
	ENTER();

	if(priv->version < WLAN_VERSION_91_E)
		goto out;

	if(cmd == BT_COEXIST){
		state = rq->ifr_metric;
		ret = 0;

		pta_param.prot_mode = PTA_NONE_PROTECT;
		pta_param.mac_rate = 0x0; 
		pta_param.hw_retry = 0x7;
		pta_param.sw_retry = 0x3;
		pta_param.cca_bypass = TRUE;
		pta_param.active_time = 500;              /* Unit is 100us */
		pta_param.thresh_time = 20;           /* Unit is 100us */
		pta_param.auto_prot_thresh_time = 200;  /* Unit is 100us */
		pta_param.flags = BIT0 | BIT1 | BIT5;

		if(state == BT_STATE_SCO_ONGOING){
			state = BT_STATE_SCO_ON;
		}
		old_state = bt_state;

		if(state &(BT_STATE_SCO_ON | BT_STATE_SCO_ONGOING)){
			bt_state |= BT_STATE_SCO_ON;
		}
		if(state & BT_STATE_A2DP_PLAYING){
			bt_state |= BT_STATE_A2DP_PLAYING;
		}
		if(state & BT_STATE_CONNECTION_ON)
			bt_state |= BT_STATE_CONNECTION_ON;

		if(state == BT_STATE_SCO_OFF){
			bt_state &= ~BT_STATE_SCO_ON;
		}else if(state == BT_STATE_A2DP_NO_PLAYING){
			bt_state &= ~BT_STATE_A2DP_PLAYING;
		}else if(state == BT_STATE_CONNECTION_OFF)
			bt_state &= ~BT_STATE_CONNECTION_ON;

		if(old_state == bt_state)
			goto out;

		if(bt_state){
			if(bt_state & BT_STATE_SCO_ON){
				if(old_state) //should clear pta proc before to set a new pta protec
					ret = wlan_set_pta(priv, &pta_param);
				pta_param.prot_mode = PTA_PS_POLL_PROTECT;
				pta_param.mac_rate = 0x4;
				pta_param.hw_retry = 0x1;
				pta_param.sw_retry = 0x1;
				pta_param.active_time = 25;
				pta_param.thresh_time = 5;
				pta_param.auto_prot_thresh_time = 15;
				pta_param.flags = BIT0 | BIT1;
			}else if(bt_state & BT_STATE_A2DP_PLAYING){
				if(old_state)
					ret = wlan_set_pta(priv, &pta_param);
				pta_param.prot_mode = PTA_NULL_DATA_PROTECT;
				pta_param.active_time = 600;
				pta_param.thresh_time = 20;
				pta_param.auto_prot_thresh_time = 200;
			}else if(bt_state & BT_STATE_CONNECTION_ON){
				if(old_state)
					ret = wlan_set_pta(priv, &pta_param);
				pta_param.prot_mode = PTA_NULL_DATA_PROTECT;
				pta_param.active_time = 600;
				pta_param.thresh_time = 20;
				pta_param.auto_prot_thresh_time = 200;
			}
		}else
			pta_param.prot_mode =  PTA_NONE_PROTECT;
			
		ret = wlan_set_pta(priv, &pta_param);
		//log for debug now do not delete
		printk("***BT_COEXIST state:%x \n", bt_state);
	}

out:
	LEAVE();

	return ret;
}

static const struct net_device_ops rda_netdev_ops = {
	.ndo_open		= wlan_dev_open,
	.ndo_stop		= wlan_eth_stop,
	.ndo_start_xmit 	= wlan_hard_start_xmit,
	.ndo_set_mac_address	= wlan_set_mac_address,
	.ndo_tx_timeout 	= wlan_tx_timeout,
	.ndo_get_stats = wlan_get_stats,
	.ndo_do_ioctl = wlan_ioctl
};

#ifdef WLAN_FORCE_SUSPEND_SUPPORT
static int rda_wlan_set_suspend(wlan_private * priv)
{
	int ret;

	ENTER();

	if (!priv) {
		ret = -1;
		return ret;
	}

	ret= wlan_set_listen_interval(priv, WIFI_SLEEP_LISTEN_INTERVAL);
	if(ret){
		WLAN_ERRP("wlan_set_listen_interval failed! \n");
	}

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_SLEEP_LINK_LOSS_THRESHOLD_90);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91){
		ret = wlan_set_link_loss_threshold(priv, WIFI_SLEEP_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_SLEEP_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91_F){
		ret = wlan_set_link_loss_threshold(priv, WIFI_SLEEP_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}

	LEAVE();

	return ret;
}

static int rda_wlan_set_resume(wlan_private * priv)
{
	int ret;

	ENTER();

	if (!priv) {
		ret = -1;
		return ret;
	}

	ret= wlan_set_listen_interval(priv, WIFI_LISTEN_INTERVAL);
	if(ret){
		WLAN_ERRP("wlan_set_listen_interval failed! \n");
	}

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_90);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}else if(priv->version == WLAN_VERSION_91_F){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
		}
	}

	LEAVE();
	return ret;
}

static int rda_wlan_early_suspend(struct device *dev)
{
	ENTER();
	LEAVE();
	return 0;
}

static int rda_wlan_late_resume(struct device *dev)
{
	int ret;
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);

	ENTER();

	if (priv->CardInSuspend == TRUE) {
		priv->CardInSuspend = FALSE;
		ret = rda_wlan_set_resume(priv);
		if (ret) {
			WLAN_ERRP("rda_wlan_set_resume failed! \n");
		}
	}

	LEAVE();
	return 0;
}

static int rda_wlan_notify(struct notifier_block *nb,
			       unsigned long mode, void *_unused)
{
	int ret;
	wlan_private * priv = NULL;
	priv = container_of(nb, wlan_private, pm_nb);

	ENTER();

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		if (priv->CardInSuspend == FALSE) {
			ret = rda_wlan_set_suspend(priv);
			if (ret) {
				WLAN_ERRP("rda_wlan_set_suspend failed! \n");
			}
			priv->CardInSuspend = TRUE;
		}
		break;
	}

	LEAVE();

	return 0;
}
#endif /*WLAN_FORCE_SUSPEND_SUPPORT*/
#ifdef WLAN_SYS_SUSPEND

static int rda_sdio_suspend(struct device *dev)
{
	mmc_pm_flag_t sdio_flags;
	wlan_private * priv = NULL;
	wlan_sdio_card * card = NULL;
	struct sdio_func *func = dev_to_sdio_func(dev);  
	int ret = 0, ret1 = 0;
	ENTER();

	card = (wlan_sdio_card*)sdio_get_drvdata(func);
	if(card)
		priv = card->priv;
	else
		return 0;
	
	printk("rda_sdio_suspend ***** \n");	
	netif_stop_queue(priv->netDev); 
	priv->Suspend = TRUE;

	wlan_cancel_timer(&priv->StartAssociationTimeOut);
	wlan_remove_tx_data_queue(priv);
	wlan_release_wid_pending_queue(priv);

	while(1){
		if(!priv->CardInSleep){
			wlan_push_event(priv, WLAN_EVENT_CARD_TO_SLEEP, priv, FALSE);
			wlan_sched_timeout(50);
		}else
			break;
	}
	sdio_flags = sdio_get_host_pm_caps(func);
	if (!(sdio_flags & MMC_PM_KEEP_POWER)) {
		printk("Host can't keep power while suspended \n");
		ret1 = 0;
	}	

	ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
	if (ret) {
		printk("set host pm flags failed \n");
		ret1 = 0;	
	}
	//enable wifi eint for waking up system when suspend
	wlan_register_host_wake_irq(priv);
	LEAVE();
	return ret1;
}


static int rda_sdio_resume(struct device *dev)
{
	//add by LA
	wlan_private * priv = NULL;
	wlan_sdio_card * card = NULL;
	struct sdio_func *func = dev_to_sdio_func(dev);  
	ENTER();

	card = (wlan_sdio_card*)sdio_get_drvdata(func);
	if(card)
		priv = (wlan_private*)card->priv;
	else
		return 0;

	printk("rda_sdio_resume \n");
	netif_wake_queue(priv->netDev);
	priv->Suspend = FALSE;
	//disable wifi eint when system resume
	wlan_unregister_host_wake_irq(priv);
	LEAVE();
	return 0;
}

static struct dev_pm_ops rda_sdio_pm_ops = {
	.suspend = rda_sdio_suspend,
	.resume  = rda_sdio_resume,
};

#endif

extern int wlan_set_rssi_91(wlan_private * priv, u8 rssi);

unsigned int wlan_extern_irq_handle(int irq, void *para)
{
	wlan_private* priv = (wlan_private* )para;
	disable_irq_nosync(priv->external_irq);
	wake_lock_timeout(&priv->ExtIrqTimerLock, HZ/5);
	return 0;
}

void if_sdio_interrupt(struct sdio_func *func)
{ 
	int ret = 0;
	wlan_sdio_card *card = NULL;
	wlan_private* priv = NULL;
	u8 status; 
	unsigned long flags = 0;
	u8 size_l = 0, size_h = 0; 
	u16 size = 0, payload_len = 0;
	struct sk_buff *skb = NULL;
	wlan_rx_packet_node * rx_node = NULL;

	ENTER();
	card = (wlan_sdio_card*)sdio_get_drvdata(func);
	if(!card || !is_sdio_init_complete())
	{
		WLAN_ERRP("card is NULL or sdio init is not completed!\n");
		return; 
	}
	
	priv = card->priv;
	
	sdio_claim_host(card->func);
	
	ret = wlan_read_byte(priv, IF_SDIO_FUN1_INT_STAT, &status);
	sdio_release_host(card->func);
	if (ret){
		WLAN_ERRP("SDIO read IF_SDIO_FUN1_INT_STAT status failed!\n");
		goto out;
	}
	
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_VERB,"if_sdio_interrupt, status = 0x%02x\n", status);

	if (status & IF_SDIO_INT_AHB2SDIO){

		sdio_claim_host(card->func);
		if(wlan_read_byte(priv, IF_SDIO_AHB2SDIO_PKTLEN_L, &size_l)){
			WLAN_ERRP("SDIO read IF_SDIO_AHB2SDIO_PKTLEN_L failed!\n");
			sdio_release_host(card->func);
			goto out;
		}

		if(wlan_read_byte(priv, IF_SDIO_AHB2SDIO_PKTLEN_H, &size_h)){
			WLAN_ERRP("SDIO read IF_SDIO_AHB2SDIO_PKTLEN_H failed!\n");
			sdio_release_host(card->func);
			goto out;
		}

		size = (size_l | ((size_h & 0x7f) << 8)) * 4;
		if(size > SDIO_MAX_BUFSZ){
			WLAN_ERRP("if_sdio_interrupt received buffer is too larger size %d \n", size);
			goto out;
		}else if(size < 4){
			WLAN_ERRP("invalid size %d \n", size);
			goto out;
		}
		
		skb =  dev_alloc_skb(size + NET_IP_ALIGN + WID_HEADER_LEN + 3);
		if(!skb){
			WLAN_ERRP("if_sdio_interrupt alloc skb failed \n");
			goto out;
		}

		rx_node = kzalloc(sizeof(wlan_rx_packet_node), GFP_ATOMIC);
		if(!rx_node){
			WLAN_ERRP("kzalloc wlan_rx_packet_node failed \n");
			dev_kfree_skb(skb);
			goto out;
		}
		
		skb_reserve(skb, NET_IP_ALIGN);
		//4byte align 
		skb_align(skb, 4);
		rx_node->Skb = skb;
		if(wlan_read_bytes(priv, IF_SDIO_FUN1_FIFO_RD, skb->data, size)
			   || priv->CardRemoved){
			WLAN_ERRP("SDIO read IF_SDIO_FUN1_FIFO_RD failed! \n");
			sdio_release_host(card->func);
			kfree(rx_node);
			dev_kfree_skb(skb);			
			goto out;
		}		
		sdio_release_host(card->func);

		//put payload length in skb
		payload_len = (u16)(skb->data[0] + ((skb->data[1]&0x0f) << 8));
		if(payload_len > size){
			WLAN_ERRP("SDIO read payload_len invalid! \n");
			kfree(rx_node);
			dev_kfree_skb(skb);
			goto out;
		}
		skb_put(skb, payload_len);

		spin_lock_irqsave(&priv->RxLock, flags);
		list_add_tail(&rx_node->List, &priv->RxQueue);
		priv->RxQuNum++;
		spin_unlock_irqrestore(&priv->RxLock, flags);
		complete(&priv->RxThread.comp);

	}else if (status & IF_SDIO_INT_ERROR){
		sdio_claim_host(card->func);
		ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, IF_SDIO_INT_ERROR);
		sdio_release_host(card->func);
		if (ret){
			WLAN_ERRP("write FUN1_INT_STAT reg fail \n");
			goto out;
		}
		  WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_TRACE,"%s, INT_ERROR \n", __func__);
	}

out:
	LEAVE();
}

extern unsigned int rda_wlan_version(void);

static struct platform_device *platform_device = NULL;


static ssize_t show_dbgl(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",wlan_dbg_level);
}

static ssize_t store_dbgl(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long dbgl;

	if (strict_strtoul(buf, 0, &dbgl))
		return -EINVAL;

	wlan_dbg_level = dbgl;
	  
	return count;
}

static ssize_t show_dbga(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n",wlan_dbg_area);
}

static ssize_t store_dbga(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long dbga;

	if (strict_strtoul(buf, 0, &dbga))
		return -EINVAL;

	wlan_dbg_area = dbga;
	  
	return count;
}

static ssize_t show_wlanpm(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);
	return sprintf(buf, "%d \n",priv?(u32)priv->wlan_pm_enable:0);
}

static ssize_t store_wlanpm(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long wlanpm;
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);

	if (strict_strtoul(buf, 0, &wlanpm))
		return -EINVAL;

	if(priv)
		priv->wlan_pm_enable = wlanpm;
	  
	return count;
}

static ssize_t show_wlanirq(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);
	return sprintf(buf, "%d \n",priv?(u32)priv->sdio_irq_enable:0);
}
static ssize_t store_wlanirq(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long wlanirq;
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);
	if (strict_strtoul(buf, 0, &wlanirq))
		return -EINVAL;
	if (wlanirq) {
		rda_mci_enable_sdio_irq(priv->MmcCard->host, 1);
		priv->sdio_irq_enable = TRUE;
	} else {
		//rda_mmc_set_sdio_irq(1, false);
		rda_mci_enable_sdio_irq(priv->MmcCard->host, 0);
		priv->sdio_irq_enable = FALSE;
	}
	return count;
}
static ssize_t show_wlan_earlysuspend_enabled(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",priv?(u32)priv->earlysuspend_enabled:0);
}

static ssize_t store_wlan_earlysuspend_enabled(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	int ret;
	int set;
	wlan_private* priv = (wlan_private*)dev_get_drvdata(dev);

	ret = kstrtoint(buf, 0, &set);
	if (ret < 0) {
		return ret;
	}

	set = !!set;

	if (priv->earlysuspend_enabled == set) {
		return count;
	}

#ifdef WLAN_FORCE_SUSPEND_SUPPORT
	if (set) {
		ret = rda_wlan_late_resume(dev);
	} else {
		ret = rda_wlan_early_suspend(dev);
	}
#endif

	priv->earlysuspend_enabled = set;

	return count;
}
static DEVICE_ATTR(dbgl, S_IWUSR | S_IRUGO , show_dbgl, store_dbgl);
static DEVICE_ATTR(dbga, S_IWUSR | S_IRUGO, show_dbga, store_dbga);
static DEVICE_ATTR(wlanpm, S_IWUSR | S_IRUGO, show_wlanpm, store_wlanpm);
static DEVICE_ATTR(wlanirq, S_IWUSR | S_IRUGO, show_wlanirq, store_wlanirq);
static DEVICE_ATTR(enabled, S_IWUSR | S_IWGRP | S_IRUGO, show_wlan_earlysuspend_enabled, store_wlan_earlysuspend_enabled);

static struct attribute *wlan_dbg_sysfs_entries[] = {
	&dev_attr_dbgl.attr,
	&dev_attr_dbga.attr,
	&dev_attr_wlanpm.attr,
	&dev_attr_wlanirq.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group wlan_dbg_attr_group = {
	.attrs	= wlan_dbg_sysfs_entries,
};

static int if_sdio_probe(struct sdio_func *func,
								const struct sdio_device_id *id)
{
	wlan_sdio_card * card = NULL;
	wlan_private * priv = NULL;
	int ret = -1;

	ENTER();

	if(func) {
		printk("bcmsdh_sdmmc: %s Enter\n", __FUNCTION__);
		printk("sdio_bcmsdh: func->class=%x\n", func->class);
		printk("sdio_vendor: 0x%04x\n", func->vendor);
		printk("sdio_device: 0x%04x\n", func->device);
		printk("Function#: 0x%04x\n", func->num);
	} else {
		printk("func is null\n");
	}
	
	if(id->vendor != SDIO_VENDOR_ID_RDA){
	   WLAN_ERRP("rda5890 sdio	not corrent vendor:%x \n", id->vendor);
	   return -1;
	}
	
	card = (wlan_sdio_card*)kzalloc(sizeof(wlan_sdio_card), GFP_KERNEL);
	if (!card){
		ret = -ENOMEM;
		goto err_alloc_card;
	}

	card->func = func;
	sdio_set_drvdata(func, card);

	if(wlan_add_card(card))
		goto err_add_card;
	
	priv = card->priv;
	priv->MmcCard = func->card;
	wlan_debugfs_init_all(priv);

	platform_device = platform_device_alloc("rda-wlan", -1);
	if (platform_device){
		platform_set_drvdata(platform_device, priv);
		if(!platform_device_add(platform_device)){
			 ret = sysfs_create_group(&platform_device->dev.kobj,
					&wlan_dbg_attr_group);
		}else{
			platform_device_put(platform_device);
		}
	}
	
	priv->netDev->netdev_ops = &rda_netdev_ops;
	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	if (ret){
		WLAN_ERRP("sdio_enable_func fail, ret = %d\n", ret);
		sdio_release_host(func);
		goto err_enable_func;
	}

	
	rda_mci_enable_sdio_irq(priv->MmcCard->host, 0);
	priv->version = rda_wlan_version();
	
	ret = sdio_claim_irq(func, if_sdio_interrupt);
	if (ret){
		WLAN_ERRP("sdio_claim_irq fail, ret = %d\n", ret);
		sdio_release_host(func);
		goto err_claim_irq;
	}
	
	ret = -1;

	//enable interrupt
	if(wlan_write_byte(priv, IF_SDIO_FUN1_INT_MASK, 0x07)){
		WLAN_ERRP("err_enable_int \n");
		sdio_release_host(func);
		goto err_enable_int;
	}
	sdio_release_host(func);
	
	wlan_push_event(priv, WLAN_EVENT_CARD_CONTROL_INIT, priv, FALSE);

#ifdef WLAN_FORCE_SUSPEND_SUPPORT
	priv->pm_nb.notifier_call = rda_wlan_notify;
	register_pm_notifier(&priv->pm_nb);
#endif
	LEAVE();
	
	return 0;

err_enable_int:
err_claim_irq:
err_enable_func:
	wlan_release_dev(card);
	
err_add_card:
	kfree(card);

err_alloc_card:
	
	return ret;
}


static void if_sdio_remove(struct sdio_func *func)
{
	wlan_sdio_card * card = NULL;
	wlan_private * priv = NULL;
	ENTER();

	card = (wlan_sdio_card *)sdio_get_drvdata(func);
	if(!card)
		return;

	priv = (wlan_private*)card->priv;
	if(!priv){
		kfree(card);
		return;
	}

	if(platform_device){
		sysfs_remove_group(&platform_device->dev.kobj, &wlan_dbg_attr_group);
		platform_device_unregister(platform_device);
		platform_device = NULL;
	}
	wlan_debugfs_remove_all(priv);

	sdio_claim_host(func);
	sdio_release_irq(func);
	sdio_disable_func(func);
	sdio_release_host(func);

	rda_mci_enable_sdio_irq(priv->MmcCard->host, 0);
	priv->sdio_irq_enable = FALSE;

	wlan_unregister_host_wake_irq(priv);
	wlan_release_dev(card);  
	kfree(card);
	priv->MmcCard = NULL;
	LEAVE();
}

static struct sdio_driver if_sdio_driver = {
	.name		= "rda_wlan_sdio",
	.id_table	= if_sdio_ids,
	.probe		= if_sdio_probe,
	.remove 	= if_sdio_remove,
#ifdef WLAN_SYS_SUSPEND 
	.drv.pm 	= &rda_sdio_pm_ops,
#endif	
};

extern void wmt_detect_sdio2(void);
extern void force_remove_sdio2(void);

static int __init wlan_init_module(void)
{
	int ret = 0;

	printk(KERN_INFO "\nRDA5890 SDIO WIFI Driver for st_linux \n");
	printk(KERN_INFO "Ver: %d.%d.%d\n\n", 
	WLAN_SDIOWIFI_VER_MAJ, 
	WLAN_SDIOWIFI_VER_MIN, 
	WLAN_SDIOWIFI_VER_BLD);
	wlan_debugfs_init();
	ret = sdio_register_driver(&if_sdio_driver);
	wmt_detect_sdio2();
	printk("wlan_init_module2014022801 %d \n", ret);
	return ret;
}

static void __exit wlan_exit_module(void)
{
	sdio_unregister_driver(&if_sdio_driver);
	wlan_debugfs_remove();
	printk("wlan_exit_module \n");

    force_remove_sdio2();

    
}

module_init(wlan_init_module);
module_exit(wlan_exit_module);
MODULE_DESCRIPTION("RDA SDIO WLAN Driver");
MODULE_AUTHOR("albert");
MODULE_LICENSE("GPL");

#endif

