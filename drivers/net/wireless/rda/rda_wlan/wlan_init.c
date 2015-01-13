#include "wlan_includes.h"
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irqnr.h>
//#include <mach/sys_config.h>
extern void rda_mci_enable_sdio_irq(struct mmc_host *mmc, int enable);

int wlan_register_host_wake_irq(wlan_private* priv)
{
	int ret = 0;
#ifdef ALL_WINNER
	script_item_u val;
	script_item_value_type_e type;

	type = script_get_item("wifi_para", "rda5990_wl_host_wake", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type){
		printk("get RDA rda5990_wl_host_wake gpio failed\n");
		priv->gpio_2_irq = 0;
	}
	else
		priv->gpio_2_irq = val.gpio.gpio;
#endif
	printk("\n\n\n\n\n\n\n%s %d return\n\n\n\n\n\n\n\n\n",__func__,__LINE__);
    return 0;


	if(priv->external_irq)
		wlan_unregister_host_wake_irq(priv);
	
	priv->external_irq = gpio_to_irq(priv->gpio_2_irq);
	if (IS_ERR_VALUE(priv->external_irq)) {
		printk("map gpio [%d] to virq failed, errno = %d\n", 
			priv->gpio_2_irq, priv->external_irq);
		return -1;
	}
	ret= devm_request_irq(&priv->MmcCard->dev, priv->external_irq, wlan_extern_irq_handle, 
								IRQF_TRIGGER_LOW, "rda_combo_wifi", priv);
	if (IS_ERR_VALUE(ret)) {
		printk("request virq %d failed, errno = %d\n", 
						priv->external_irq,  ret);
		return -1;
	}
	return 0;
}

void wlan_unregister_host_wake_irq(wlan_private* priv)
{
	if(!priv || !priv->MmcCard)
		return;
	
	if(priv->external_irq)
		devm_free_irq(&priv->MmcCard->dev, priv->external_irq, priv);

	priv->external_irq = 0;

}

int wlan_init(wlan_private* priv)
{
    int ret, i, bufsize;
    wlan_sdio_card* card = (wlan_sdio_card*)priv->card;

    spin_lock_init(&priv->WidLock);
    spin_lock_init(&priv->RxLock);
    spin_lock_init(&priv->TxLock);
    spin_lock_init(&priv->EventLock);
    spin_lock_init(&priv->ScanListLock);
    spin_lock_init(&card->cardLock);
    
	INIT_LIST_HEAD(&priv->AverRssiQ);
    INIT_LIST_HEAD(&priv->WidFreeQ);
    INIT_LIST_HEAD(&priv->WidPendingQ);
    INIT_LIST_HEAD(&priv->RxQueue);
    INIT_LIST_HEAD(&priv->TxQueue);
    INIT_LIST_HEAD(&priv->EventQueue);

    /* Initialize scan result lists */
    INIT_LIST_HEAD(&priv->network_free_list);
    INIT_LIST_HEAD(&priv->network_list);    
    /* Allocate buffer to store the BSSID list */
    bufsize = WLAN_MAX_NETWORK_NUM * (sizeof(struct bss_descriptor) + 4);
    priv->networks = kzalloc(bufsize, GFP_KERNEL);
	if (priv->networks == NULL) {
		WLAN_ERRP("kzalloc bss_descriptor memory failed!\n");
		return WLAN_STATUS_FAILED;
	}
    for (i = 0; i < WLAN_MAX_NETWORK_NUM; i++){
		list_add_tail(&priv->networks[i].list, &priv->network_list);
    }

    priv->netDevRegistered = FALSE;
    atomic_set(&(priv->TxQuNum), 0);
	priv->RxQuNum = 0;
    atomic_set(&(priv->netifQuStop), 0);
	priv->reassoc_count = 0;
    priv->CardRemoved = FALSE;
    priv->IgnoreFisrtDisconnect = FALSE;
    priv->assoc_ongoing = FALSE;
    priv->scan_running = WLAN_SCAN_IDLE;
    priv->SdioErrorCount =0;
	priv->sdio_irq_enable = FALSE;
	priv->CardInSleep = FALSE;
	priv->CardSleepWakeLockOn = FALSE;
	priv->CardInSuspend = FALSE;
	priv->earlysuspend_enabled=0;
	priv->sdio_need_reset = 0;
#ifdef WLAN_SDIO_RESET_DEBUG	
	priv->debug_count = 0;
#endif
	priv->EventErrorCount=0;

	ret = wlan_alloc_wid_queue(priv);
    if(ret)
        return WLAN_STATUS_FAILED;   

    wlan_initialize_timer(&priv->StartAssociationTimeOut, NULL, priv, WLAN_EVENT_START_ASSOC);
    wlan_initialize_timer(&priv->ScanResultsTimeout, NULL, priv, WLAN_EVENT_SCAN_RESULT_TIMEOUT);
    wlan_initialize_timer(&priv->AssociationTimeOut, NULL, priv, WLAN_EVENT_ASSOC_TIMEOUT);
    wlan_initialize_timer(&priv->ReAssociationTimeOut, NULL, priv, WLAN_EVENT_REASSOC_TIMEOUT);
    wlan_initialize_timer(&priv->CardToSleepTimer, NULL, priv, WLAN_EVENT_CARD_TO_SLEEP);

    wake_lock_init(&priv->CardSleepTimerLock, WAKE_LOCK_SUSPEND, "CardSleepTimerLock");
    wake_lock_init(&priv->MacStatusLock, WAKE_LOCK_SUSPEND, "MacStatusLock");
    wake_lock_init(&priv->ExtIrqTimerLock, WAKE_LOCK_SUSPEND, "ExtIrqTimerLock");
    
    return WLAN_STATUS_SUCCESS;
}

int wlan_add_card(wlan_sdio_card * card)
{
    struct net_device *dev = NULL;
    wlan_private *priv = NULL;
    int ret = -1;

    /* Allocate an Ethernet device and register it */
    dev = alloc_netdev_mq(sizeof(wlan_private), "wlan%d", ether_setup, 1);
    ENTER();
    
    if(!dev)
        return ret;
   
    priv = netdev_priv(dev);
    card->priv = priv;
    priv->card = card;
    priv->netDev = dev;

    dev->hard_header_len += WID_HEADER_LEN;
    dev->watchdog_timeo = DEFAULT_WATCHDOG_TIMEOUT;
    dev->trans_start = jiffies;

#ifdef  WIRELESS_EXT
    dev->wireless_handlers = (struct iw_handler_def *)&wlan_wext_handler_def;
#endif
    dev->flags |= IFF_BROADCAST | IFF_MULTICAST;


    wlan_init(priv);

    priv->RxThread.priv = priv;
    wlan_create_thread(wlan_rx_thread,
                       &priv->RxThread, "wlan_rx_thread");

    priv->EventThread.priv = priv;
    wlan_create_thread(wlan_event_thread,
                       &priv->EventThread, "EventThread");

    priv->TxThread.priv = priv;
    wlan_create_thread(wlan_tx_thread,
                       &priv->TxThread, "wlan_tx_thread");

    while(priv->RxThread.pid == 0 ||
       priv->EventThread.pid == 0 ||
       priv->TxThread.pid == 0)
       schedule();

    return 0;
}

void wlan_release_dev(wlan_sdio_card * card)
{
    wlan_private * priv = (wlan_private*) card->priv;

    ENTER();
    if(!priv)
       return;

    netif_stop_queue(priv->netDev);
    netif_carrier_off(priv->netDev);

    //first remove event queue , we shold keep this befrore set CardRmoved
    //because the event has para should be free in it event handler.
    wlan_remove_event_queue(priv);
    
    //release wid pending queue 
    wlan_release_wid_pending_queue(priv);
    
        if(!(priv->CardToSleepTimer).timer_is_canceled)
    wlan_cancel_timer(&priv->CardToSleepTimer);
    wlan_cancel_timer(&priv->StartAssociationTimeOut);
    wlan_cancel_timer(&priv->AssociationTimeOut);
    wlan_cancel_timer(&priv->ReAssociationTimeOut);
    wlan_cancel_timer(&priv->ScanResultsTimeout);

    if(priv->netDev && priv->netDevRegistered)
        unregister_netdev(priv->netDev);

    priv->CardRemoved = 1;
    //waiting rx thread terminate
    while(priv->RxThread.pid){       
       complete(&priv->RxThread.comp);
       wlan_sched_timeout(2);
    }
    //waiting event thread timeout
    while(priv->EventThread.pid){
       complete(&priv->EventThread.comp);
       wlan_sched_timeout(2);
    }
    
    //waiting tx thread timeout
    while(priv->TxThread.pid){
       complete(&priv->TxThread.comp);
       wlan_sched_timeout(2);
    }

    wlan_unit(priv);
      
    if(priv->netDev)
        free_netdev(priv->netDev);
#ifdef WLAN_FORCE_SUSPEND_SUPPORT
	unregister_pm_notifier(&priv->pm_nb);
#endif

    LEAVE();
}

void wlan_remove_tx_data_queue(wlan_private * priv)
{
    struct list_head *qe = NULL, *qen = NULL;
    wlan_tx_packet_node *txNode = NULL;
    ENTER();
    spin_lock(&priv->TxLock);
    if(!list_empty(&priv->TxQueue)){
        list_for_each_safe(qe, qen, &priv->TxQueue){
            txNode= (wlan_tx_packet_node*)qe;
			if(txNode->Skb && (txNode->type == WLAN_DATA))
                dev_kfree_skb(txNode->Skb);

            list_del(&txNode->List);
            kfree(txNode);
        }
    }
    spin_unlock(&priv->TxLock);
    LEAVE();
}

void wlan_remove_rx_queue(wlan_private * priv)
{
    struct list_head *qe = NULL, *qen = NULL;
    wlan_rx_packet_node* rxNode = NULL;

    ENTER();
    spin_lock(&priv->RxLock);
    if(!list_empty(&priv->RxQueue)){
         list_for_each_safe(qe, qen, &priv->RxQueue){
            rxNode= (wlan_rx_packet_node*)qe;

            if(rxNode->Skb)
                dev_kfree_skb(rxNode->Skb);          

            list_del(&rxNode->List);
            kfree(rxNode);
        }
    }
    spin_unlock(&priv->RxLock);
    LEAVE();
}

void wlan_remove_event_queue(wlan_private * priv)
{
    while(1){
        if(!list_empty(&priv->EventQueue)){
             complete(&priv->EventThread.comp);           
        }else
            break;     
        wlan_sched_timeout(2);
    }
}

void wlan_unit(wlan_private * priv)
{
	struct bss_descriptor *iter_bss;
	wlan_free_wid_queue(priv);
	wlan_remove_tx_data_queue(priv);
	wlan_remove_rx_queue(priv);

	if(priv->networks){
		list_for_each_entry(iter_bss, &priv->network_list, list) {
			clear_bss_descriptor(iter_bss);
		}
		kfree(priv->networks);
	}

    priv->networks = NULL;

	wlan_free_aver_rssi(priv);

	wake_lock_destroy(&priv->CardSleepTimerLock);
	wake_lock_destroy(&priv->MacStatusLock);
    wake_lock_destroy(&priv->ExtIrqTimerLock);
}

int wlan_sleep_flags = RDA_SLEEP_ENABLE | RDA_SLEEP_PREASSO;
int wlan_init_pm(wlan_private *priv)
{
    int ret = 0;
#ifdef WLAN_POWER_MANAGER
    wlan_sdio_card *card =  (wlan_sdio_card *)priv->card;
#endif

    if(rda_combo_wifi_in_test_mode())
        return 0;
	
#ifdef WLAN_POWER_MANAGER    
    if (wlan_sleep_flags & RDA_SLEEP_ENABLE){
        ret = wlan_set_pm_mode(priv, 2);
        if(ret < 0)
            goto err;
    }
    if (wlan_sleep_flags & RDA_SLEEP_PREASSO){
        ret = wlan_set_preasso_sleep(priv, WIFI_PREASSO_SLEEP);
        if(ret < 0)
            goto err;
    }

    sdio_claim_host(card->func);
    ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_TO_DEV, 1);
    if (ret) {
        WLAN_ERRP("write FUN1_INT_TO_DEV reg fail\n");
    }
    sdio_release_host(card->func);
    priv->wlan_pm_enable = 1;
err:
    return ret;
#else
    return ret;
#endif
}

int wlan_disable_self_cts(wlan_private *priv)
{
    int ret = 0;

    ENTER();
    ret = wlan_generic_set_uchar(priv, WID_PTA_MODE, 0);

    if(ret < 0){
        WLAN_ERRP("failed \n");
        goto err;
    }
    
    return 0;

err:
    return ret;
}

int wlan_disable_block_bt(wlan_private *priv)
{
    int ret = 0;

    ENTER();

    ret = wlan_generic_set_uchar(priv, WID_PTA_BLOCK_BT, 0);
    if(ret < 0){
        WLAN_ERRP("failed \n");
        goto err;
    }
        
    return 0;

err:
    return ret;
}

int wlan_card_control_init(wlan_private *priv)
{
    int ret = -1;
    
    ENTER();
    if(!rda_combo_wifi_in_test_mode()){
		ret=wlan_sdio_init(priv);
		if(ret<0){
			WLAN_ERRP("wlan_sdio_init failed! \n");
			goto err;
		}

        ret = wlan_start_card((wlan_sdio_card *)priv->card);
        if(ret<0){
			WLAN_ERRP("wlan_start_card failed! \n");
            goto err;
        }
		
        ret = wlan_disable_self_cts(priv);
        if(ret){
			WLAN_ERRP("wlan_disable_self_cts failed! \n");
            goto err;
        }

        ret = wlan_disable_block_bt(priv);
        if(ret){
			WLAN_ERRP("wlan_disable_block_bt failed! \n");
            goto err;
        }

        ret = wlan_set_scan_timeout(priv);
        if (ret) {
			WLAN_ERRP("wlan_set_scan_timeout failed! \n");
            goto err;
        }
            
        ret= wlan_set_listen_interval(priv, WIFI_LISTEN_INTERVAL);
        if(ret){
			WLAN_ERRP("wlan_set_listen_interval failed! \n");
            goto err;
        }

		if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
                        ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_90);
			if(ret){
				WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
				goto err;
			}
		}else if(priv->version == WLAN_VERSION_91){
                        ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
			if(ret){
				WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
				goto err;
                }
			ret = wlan_set_power_save(priv);
        if(ret){
				WLAN_ERRP("wlan_set_power_save failed! \n");
				goto err;
			}
		}else if(priv->version == WLAN_VERSION_91_E){
			ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
			if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
            goto err;
        }
			ret = wlan_set_power_save(priv);
			if(ret){
				WLAN_ERRP("wlan_set_power_save failed! \n");
				goto err;
			}

		}else if(priv->version == WLAN_VERSION_91_F){
			ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
			if(ret){
				WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
				goto err;
			}
			ret = wlan_set_power_save(priv);
			if(ret){
				WLAN_ERRP("wlan_set_power_save failed! \n");
				goto err;
			}
		}

        //    ret = wlan_generic_set_ushort(priv, WID_JOIN_TIMEOUT, 3000);
        //    if(ret)
        //        goto err;
         
        ret = wlan_init_pm(priv);
        if(ret){
			WLAN_ERRP("wlan_init_pm failed! \n");
            goto err;
        }
    }else{        
        ret = wlan_set_test_mode(priv);

        if (register_netdev(priv->netDev)) {
            WLAN_ERRP("register_netdev failed\n");
            priv->netDevRegistered = FALSE;
			goto err;
        }else
            priv->netDevRegistered = TRUE;
        
         wlan_indicate_disconnected(priv);
    }

    return 0; /*success*/
err:
    LEAVE();
    return -1; /*fail*/
}


int wlan_start_card(wlan_sdio_card * card)
{
    u8 mac_addr[ETH_ALEN];
    int ret = 0;
    
    wlan_private * priv = (wlan_private*) card->priv;

    if(!priv)
        return -1;  

    ENTER();

#ifdef USE_MAC_DYNAMIC_ONCE
    if(wlan_read_mac_from_file(mac_addr) != ETH_ALEN){
#ifdef USE_MAC_FROM_RDA_NVRAM
		ret = wlan_read_mac_from_nvram(mac_addr);
		if (ret) {
			WLAN_ERRP("nvram:get wifi mac addr form nvram failed, make a random mac addr instead\n");
			random_ether_addr(mac_addr);
			wlan_write_mac_to_nvram(mac_addr);
		} else {
			if (!is_valid_ether_addr(mac_addr)) {
				mac_addr[0] &= 0xfe;	/* clear multicast bit */
				mac_addr[0] |= 0x02;	/* set local assignment bit (IEEE802) */
			}
		}
#else
		random_ether_addr(mac_addr);
#endif /*USE_MAC_FROM_RDA_NVRAM*/
        ret = wlan_write_mac_to_file(mac_addr);
    }

#else
    mac_addr[0] = 0x00;
    mac_addr[1] = 0xc0;
    mac_addr[2] = 0x52;

    mac_addr[3] = 0x00;
    mac_addr[4] = 0xc0;
    mac_addr[5] = 0x53; 
#endif

    ret = wlan_set_mac_addr(priv, mac_addr);
    if (ret){
        goto done;
    }        

    ret = wlan_get_mac_addr(priv, mac_addr);
    if (ret) {
        goto done;
    }
    memcpy(priv->netDev->dev_addr, mac_addr, ETH_ALEN); 

    rda5890_set_preamble(priv, G_AUTO_PREAMBLE);
    
    if (register_netdev(priv->netDev)) {
        WLAN_ERRP("register_netdev failed\n");
        ret = -1;
        priv->netDevRegistered = FALSE;
        goto done;
    }else
        priv->netDevRegistered = TRUE;

    wlan_indicate_disconnected(priv);

done:
    LEAVE();

    return ret;
}

extern int rda_wifi_power_off(void);
extern int rda_wifi_power_on(void);
extern int sdio_reset_comm(struct mmc_card *card);
extern void rda_mmc_reset_host(u32 host_id);

int wlan_reset_card(wlan_private *priv)
{
	int ret = 0;
	wlan_sdio_card * card = NULL;
	struct mmc_host * host = NULL;
	card = (wlan_sdio_card *) priv->card;
	host = priv->mmcCard->host;
	ENTER();
	wlan_cancel_timer(&priv->CardToSleepTimer);
	wlan_remove_tx_data_queue(priv);
	wlan_release_wid_pending_queue(priv);
	sdio_claim_host(card->func);
	ret = sdio_release_irq(card->func); 
	if(ret){
		WLAN_ERRP("reset_card sdio_release_irq fail, ret = %d\n", ret);
	}
	sdio_release_host(card->func);
	ret = rda_wifi_power_off();
	if(ret)
		WLAN_ERRP("reset card power off sdio failed \n");

	ret = rda_wifi_power_on();
	if(ret){
		WLAN_ERRP("reset card power off sdio failed \n");
        //kevin add, otherwise ,call sdio_reset_comm will cause kernel crash
        goto err;
    }

	//mmc_power_restore_host(host);
rda_mci_enable_sdio_irq(priv->MmcCard->host, 0);

	//rda_mmc_set_sdio_irq(1, false);
	printk("\n\n\n\n\n\n\nkevin delete %s %d\n\n\n\n\n\n\n",__func__,__LINE__);
	priv->sdio_irq_enable = FALSE;
	priv->wlan_pm_enable = 0;

	sdio_reset_comm(priv->mmcCard);

	sdio_claim_host(card->func);
	ret = sdio_enable_func(card->func);
	if (ret){
		WLAN_ERRP("reset_card sdio_enable_func fail, ret = %d\n", ret);
		sdio_release_host(card->func);
		goto err;
	}

	ret = sdio_claim_irq(card->func, if_sdio_interrupt);
	if (ret){
		WLAN_ERRP("reset_card sdio_claim_irq fail, ret = %d\n", ret);
		sdio_release_host(card->func);
		goto err;
	}

	//enable interrupt
	if(wlan_write_byte(priv, IF_SDIO_FUN1_INT_MASK, 0x07)){
		WLAN_ERRP("err_enable_int \n");
		sdio_release_host(card->func);
		goto err;
	}

	//re-set sdio block size
	sdio_set_block_size(card->func, 512);
	sdio_release_host(card->func);
	
	ret = wlan_sdio_init(priv);
	if(ret < 0){
		WLAN_ERRP("wlan_sdio_init failed! \n");
		goto err;
	}

	priv->sdio_need_reset = 2;

	ret = wlan_set_mac_addr(priv, priv->netDev->dev_addr);
	if (ret){
		goto err;
	}	

	ret = rda5890_set_preamble(priv, G_AUTO_PREAMBLE);
	if (ret){
		goto err;
	}

	ret = wlan_disable_self_cts(priv);
	if(ret){
		WLAN_ERRP("wlan_disable_self_cts failed! \n");
		goto err;
	}

	ret = wlan_disable_block_bt(priv);
	if(ret){
		WLAN_ERRP("wlan_disable_block_bt failed! \n");
		goto err;
	}

	ret = wlan_set_scan_timeout(priv);
	if (ret) {
		WLAN_ERRP("wlan_set_scan_timeout failed! \n");
		goto err;
	}
		
	ret= wlan_set_listen_interval(priv, WIFI_LISTEN_INTERVAL);
	if(ret){
		WLAN_ERRP("wlan_set_listen_interval failed! \n");
		goto err;
	}

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_90);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
			goto err;
		}
	}else if(priv->version == WLAN_VERSION_91){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
			goto err;
		}
		ret = wlan_set_power_save(priv);
		if(ret){
			WLAN_ERRP("wlan_set_power_save failed! \n");
			goto err;
		}
	}else if(priv->version == WLAN_VERSION_91_E){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
			goto err;
		}
		ret = wlan_set_power_save(priv);
		if(ret){
			WLAN_ERRP("wlan_set_power_save failed! \n");
			goto err;
		}

	}else if(priv->version == WLAN_VERSION_91_F){
		ret = wlan_set_link_loss_threshold(priv, WIFI_LINK_LOSS_THRESHOLD_91);
		if(ret){
			WLAN_ERRP("wlan_set_link_loss_threshold failed! \n");
			goto err;
		}
		ret = wlan_set_power_save(priv);
		if(ret){
			WLAN_ERRP("wlan_set_power_save failed! \n");
			goto err;
		}
	}

	ret = wlan_init_pm(priv);
	if(ret){
		WLAN_ERRP("wlan_init_pm failed! \n");
		goto err;
	}

	//send a scan event after reset
	wlan_push_event(priv, WLAN_EVENT_START_SCAN, priv, FALSE);

	return 0;
err:
	LEAVE();
	return -1; /*fail*/	
}
