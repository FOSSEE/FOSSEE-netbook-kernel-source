#include "wlan_includes.h"

extern void rda_mci_enable_sdio_irq(struct mmc_host *mmc, int enable);
void check_sdio_status(wlan_private * priv, int result)
{
#ifdef CHECK_SDIO_STAUTS
	if (result) {
		priv->SdioErrorCount++;
		if ( !rda_combo_wifi_in_test_mode() && priv->SdioErrorCount > WLAN_SDIO_MAX_ERR) {
			wlan_push_event(priv, WLAN_EVENT_CHECK_SDIO, priv, FALSE);
			priv->SdioErrorCount = 0;
			priv->sdio_need_reset = 1;
		}
	}else{
		priv->SdioErrorCount = 0;
	}
#endif	  
}

int wlan_card_check_sdio(wlan_private * priv)
{
	int ret = 0;
#ifdef WLAN_SDIO_RESET_DEBUG	
	priv->debug_count = 0;
#endif
	WLAN_ERRP("###################################################");
	WLAN_ERRP("###################################################");
	WLAN_ERRP("###################################################");

	wlan_indicate_disconnected(priv);
	
	ret = wlan_reset_card(priv);
	if(ret < 0)
		WLAN_ERRP("wlan sdio reset failed");
	priv->sdio_need_reset = 0;
	return 0;
}
int wlan_read_byte(wlan_private * priv, u32 addr, u8* data)
{
	wlan_sdio_card *card;
	int ret = 0;
	
	card = (wlan_sdio_card*)priv->card;

	if (!card || !card->func){
		WLAN_ERRP("wlan_read_byte(): card or function is NULL!\n");
		return WLAN_STATUS_FAILED;
	}

	*data = sdio_readb(card->func, addr, &ret);
	if (ret){
		WLAN_ERRP("wlan_read_byte(): sdio_readb failed! ret=%d\n", ret);
        //kevin add ,fix restore factory default. sdio fail too many time!
        if(ret == -110){
            WLAN_ERRP("sleep 100 msec\n");
            msleep(100);
        }
		check_sdio_status(priv, ret);
	}

	return ret;
}

int wlan_write_byte(wlan_private * priv, u32 addr, u8 data)
{
	wlan_sdio_card *card;
	int ret = 0;

	card = (wlan_sdio_card*)priv->card;

	if (!card || !card->func){
		WLAN_ERRP("wlan_write_byte(): card or function is NULL!\n");
		return WLAN_STATUS_FAILED;
	}

	sdio_writeb(card->func, data, addr, &ret);
	if (ret){
		WLAN_ERRP("wlan_write_byte(): sdio_writeb failed! ret=%d\n", ret);
		check_sdio_status(priv, ret);
	}

	return ret;
}

int wlan_read_bytes(wlan_private * priv, u32 addr, u8* buf, u32 count)
{
	wlan_sdio_card *card;
	int ret = 0;

	card = (wlan_sdio_card*)priv->card;

	if (!card || !card->func){
		WLAN_ERRP("wlan_read_bytes(): card or function is NULL!\n");
		return WLAN_STATUS_FAILED;
	}
	
	ret = sdio_readsb(card->func, buf, addr, count);
	if (ret){
		WLAN_ERRP("wlan_read_bytes(): sdio_readsb failed! ret=%d\n", ret);
		check_sdio_status(priv, ret);
	}else{
		 WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "wlan_read_bytes :len %d\n", count);
	}

	return ret;
}

int wlan_write_sdio_2_ahb(wlan_private * priv, u32 addr, u8* buf, u32 count)
{
	wlan_sdio_card *card;
	int ret = 0;
	u16 size = 0;
	u8 size_l = 0, size_h = 0;
	u16 bytes_left = 0, offset = 0, batch = 0;
#ifdef RDA_ANDROID_PLATFORM
	u32 blockSize = 4, nb = 0;
	u8 *packet_to_send = NULL;
	struct page *pg = NULL;
#endif
	card = (wlan_sdio_card*)priv->card;

	if (!card || !card->func ){
		WLAN_ERRP("wlan_read_byte(): card or function is NULL!\n");
		return WLAN_STATUS_FAILED;
	}

	if(rda_combo_wifi_in_test_mode() && is_sdio_init_complete()){
		return WLAN_STATUS_SUCCESS;
	}

	if (priv->version == WLAN_VERSION_90_D
	    || priv->version == WLAN_VERSION_90_E) {
		ret = wlan_sdio_flow_ctrl_90(priv);
		if(ret){
			WLAN_ERRP("wlan_sdio_flow_ctrl 5990 failed! \n");
			return ret;
		}
	} else if(priv->version == WLAN_VERSION_91){
		ret = wlan_sdio_flow_ctrl_91(priv);
		if(ret){
			WLAN_ERRP("wlan_sdio_flow_ctrl 5991 failed! \n");
			return ret;
		}
	}else if (priv->version == WLAN_VERSION_91_E) {
		ret = wlan_sdio_flow_ctrl_91e(priv);
		if (ret) {
			WLAN_ERRP("wlan_sdio_flow_ctrl 5991e failed! \n");
			return ret;
		}
	}else if (priv->version == WLAN_VERSION_91_F) {
		ret = wlan_sdio_flow_ctrl_91e(priv);
		if (ret) {
			WLAN_ERRP("wlan_sdio_flow_ctrl 5991f failed! \n");
			return ret;
		}
	} else {
		WLAN_ERRP("wlan_sdio_flow_ctrl unkown version:%d\n",priv->version);
		return ret;
	}
	
//	IN rda android platform sdio should 2^n alligen
#ifdef RDA_ANDROID_PLATFORM   
	if(count > card->func->cur_blksize){//the left bytes should be 2^n alligen
		nb	= count%(card->func->cur_blksize);
		count -= nb;
		while (blockSize < nb ){ 
				blockSize = blockSize << 1;
			}
		count += blockSize;
		blockSize = count;
	} else {
		while (blockSize < count){
			blockSize = blockSize << 1;
		}
	}
	size = blockSize/4;
	count = blockSize;
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG,"wlan_write_sdio_2_ahb size:%d %d cur_blk:%d\n",count, blockSize, card->func->cur_blksize);
#else	 
	size = count/4;
#endif
	size_l = size & 0xff;
	size_h = ((size >> 8) & 0x7f) | 0x80; //0x80 flags means lenght higer bytes

	sdio_claim_host(card->func);
	ret = wlan_write_byte(priv, IF_SDIO_SDIO2AHB_PKTLEN_L, size_l);
	if(ret){
		WLAN_ERRP("SDIO write size_l failed! \n");
		goto out;
	}

	ret = wlan_write_byte(priv, IF_SDIO_SDIO2AHB_PKTLEN_H, size_h);
	if(ret){
		WLAN_ERRP("SDIO write size_h failed! \n");
		goto out;
	}

	bytes_left = count;
	while(bytes_left){
		batch = bytes_left > card->func->cur_blksize?card->func->cur_blksize:bytes_left;
#ifdef RDA_ANDROID_PLATFORM
		{
			packet_to_send = buf + offset;

			if (((u32)packet_to_send >> PAGE_SHIFT) != 
					(((u32)packet_to_send + batch - 1) >> PAGE_SHIFT)){
				
				pg = alloc_page(GFP_KERNEL);
				if(!pg){
					ret = -1;
					break;
				}
				memcpy(page_address(pg), packet_to_send, batch);
				packet_to_send = page_address(pg);

				ret = sdio_writesb(card->func, addr, packet_to_send, batch);
				__free_page(pg);
				WLAN_ERRP("wlan data cross page boundary addr:%x size:%x \n", (u32)(buf + offset), batch);
			}else
				ret = sdio_writesb(card->func, addr, packet_to_send, batch);
		}
#else
		ret = sdio_writesb(card->func, addr, buf + offset, batch);
#endif
		if (ret){
			WLAN_ERRP("SDIO sdio_writesb failed! ret=%d len:%d \n", ret, size*4);
			break;
		}
		offset += batch;
		bytes_left -= batch;
	}
#ifdef WLAN_SDIO_RESET_DEBUG
	priv->debug_count++;
#endif
out:	
	sdio_release_host(card->func);
#ifdef WLAN_SDIO_RESET_DEBUG	
	WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "########### len %d debug_count:%d #######********\n", size * 4, priv->debug_count);
	if(priv->debug_count > 300)
		ret = -5;
#endif	
	if (ret)
	check_sdio_status(priv, ret);
	return ret;
}

int wlan_wake_up_card(wlan_private * priv)
{
	int ret = 0;
#ifdef	WLAN_POWER_MANAGER
	wlan_sdio_card *card = (wlan_sdio_card*)priv->card;
#endif
	
	ENTER();
#ifdef	WLAN_POWER_MANAGER
	if (!rda_combo_wifi_in_test_mode()){ 
		atomic_set(&priv->CardNeedSleep, FALSE);
		if(priv->CardInSleep) {
	WLAN_DBGLAP(WLAN_DA_PM, WLAN_DL_DEBUG, "wake up card \n");
	sdio_claim_host(card->func);
	ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_TO_DEV, 1);
	sdio_release_host(card->func);
	if(ret){
		WLAN_ERRP("wakeup card failed \n");
	}else{
		priv->CardInSleep = FALSE;
                        WLAN_DBGLAP(WLAN_DA_PM, WLAN_DL_DEBUG,
                                        "CardInSleep = FALSE \n");
		wlan_sched_timeout(10);
	}  
   
                if (!priv->CardSleepWakeLockOn) {
                        priv->CardSleepWakeLockOn = TRUE;
                        wake_lock(&priv->CardSleepTimerLock);
                }
        }
	}
#endif
	LEAVE();
	return ret;
}

void handle_card_to_sleep_cmd(wlan_private * priv)
{
	atomic_set(&priv->CardNeedSleep, TRUE);
	complete(&priv->TxThread.comp);
}
int wlan_card_enter_sleep(wlan_private * priv)
{
	int ret = 0;
#ifdef	WLAN_POWER_MANAGER
        wlan_sdio_card *card = (wlan_sdio_card *) priv->card;
#endif
	
	ENTER();

#ifdef	WLAN_POWER_MANAGER
        if (!rda_combo_wifi_in_test_mode() 
                        && priv->wlan_pm_enable 
                        && (priv->scan_running != WLAN_SCAN_RUNNING)
                        && (!priv->assoc_ongoing)) {
                if(atomic_read(&priv->CardNeedSleep)) {
                        sdio_claim_host(card->func);
                        ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, IF_SDIO_HOST_TX_FLAG);
                        sdio_release_host(card->func);
                        if (ret) {
                                WLAN_ERRP("enter sleep failed \n");
				wlan_mod_timer(&priv->CardToSleepTimer, CARD_ENTER_SLEEP_TIMER);
				return ret;
                        } else {
                                priv->CardInSleep = TRUE;
                                WLAN_DBGLAP(WLAN_DA_PM, WLAN_DL_DEBUG, "CardInSleep = TRUE \n");
                                atomic_set(&priv->CardNeedSleep, FALSE);
                        }
                        if (priv->CardSleepWakeLockOn) {
                                wake_unlock(&priv->CardSleepTimerLock);
                                priv->CardSleepWakeLockOn = FALSE;
			}
                                if(!(priv->CardToSleepTimer).timer_is_canceled)
                                        wlan_cancel_timer(&priv->CardToSleepTimer);
			
                        WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "card enter sleep \n");
                } else {
                                        wlan_mod_timer(&priv->CardToSleepTimer, CARD_ENTER_SLEEP_TIMER);
                                }
                        }

#endif
	LEAVE();
	return ret;
}
#ifdef WLAN_FLOW_CTRL_90E
int wlan_sdio_flow_ctrl_90(wlan_private * priv)
{
	int ret = 0;
	u8 status = 0;
	s32 int_sleep_count = 0;
	wlan_sdio_card *card = (wlan_sdio_card *) priv->card;

	ENTER();
	wlan_wake_up_card(priv);
	while ((!priv->CardRemoved) && is_sdio_patch_complete()) {
		sdio_claim_host(card->func);
		ret = wlan_read_byte(priv, IF_SDIO_FUN1_INT_PEND, &status);
		sdio_release_host(card->func);
		if (ret) {
			WLAN_ERRP("wlan read IF_SDIO_FUN1_INT_PEND failed \n");
			schedule();
			continue;
		}
		if ((status & IF_SDIO_INT_SLEEP) == 0) {	// If SDIO can't write
			if (int_sleep_count >= FLOW_CTRL_INT_SLEEP_RETRY_COUNT_90){
				WLAN_ERRP("Flow_ctrl:max int_sleep_count:%d\n",int_sleep_count);
				complete(&priv->RxThread.comp);
				msleep(100);
				break;
			} else {
				int_sleep_count++;
			}
		} else {
			if (int_sleep_count > 10)
				WLAN_ERRP("Flow_ctrl:int_sleep_count:%d\n",int_sleep_count);
			sdio_claim_host(card->func);
			ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, IF_SDIO_INT_SLEEP);
			sdio_release_host(card->func);
			break;
		}
		schedule();
	}
	LEAVE();
	return ret;
}
#else
//return 0 success
int wlan_sdio_flow_ctrl_90(wlan_private * priv)
{
	int ret = -1;
	u8 status = 0;
	s32 count = 0;
	wlan_sdio_card * card = (wlan_sdio_card*)priv->card;

	//first wake up card if needed
	wlan_wake_up_card(priv);


	ENTER();

	while(!priv->CardRemoved){
		sdio_claim_host(card->func);
		ret = wlan_read_byte(priv,IF_SDIO_FUN1_INT_PEND, &status);
		sdio_release_host(card->func);
		if(status & IF_SDIO_INT_RXCMPL)
			 WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "IF_SDIO_FUN1_INT_PEND:count count %d : %x \n",count,  status);
		if(!ret){
			if((status & IF_SDIO_INT_RXCMPL) == 0){ //flows ctrl failed
				if(count > FLOW_CTRL_RXCMPL_RETRY_COUNT_90){
					sdio_claim_host(card->func);
					wlan_write_byte( priv, IF_SDIO_FUN1_INT_PEND,  0x40);
					sdio_release_host(card->func);
					complete(&priv->RxThread.comp);
					msleep(100);
					ret = 0;
					WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_CRIT,
						    "flows ctrl RXCMPL failed, count:%d over,return back \n",
						    FLOW_CTRL_RXCMPL_RETRY_COUNT_90);
					break;
				} else {
					count ++ ;
				}
			}else{
				sdio_claim_host(card->func);
				ret = wlan_write_byte( priv, IF_SDIO_FUN1_INT_PEND,  0x40);
				sdio_release_host(card->func);
				if(!ret){
					break;
				}
			}
		}
		schedule();
	}

	LEAVE();
	return ret;
}
#endif

//return 0 success
int wlan_sdio_flow_ctrl_91(wlan_private * priv)
{
	int ret = 0;
	u8 status = 0;
	s32 int_sleep_count = 0;
	wlan_sdio_card * card = (wlan_sdio_card*)priv->card;

	ENTER();

	//first wake up card if needed
	wlan_wake_up_card(priv);



	while ((!priv->CardRemoved) && is_sdio_patch_complete()) {
			sdio_claim_host(card->func);
			ret = wlan_read_byte(priv,IF_SDIO_FUN1_INT_PEND, &status);
			sdio_release_host(card->func);
			if(ret){
				WLAN_ERRP("wlan read IF_SDIO_FUN1_INT_PEND failed \n");
				schedule();
				continue;
			}

		if ((status & IF_SDIO_INT_SLEEP) == 0) {	// If SDIO can't write
			if (int_sleep_count >= FLOW_CTRL_INT_SLEEP_RETRY_COUNT_91){
					break;
				} else {
				int_sleep_count++;
				}
			}else{
				sdio_claim_host(card->func);
			ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, IF_SDIO_INT_SLEEP);
				sdio_release_host(card->func);
					break;

			}
                if(int_sleep_count < 20)
                        udelay(10);
                else
                        msleep(1);
	}
	LEAVE();
	return ret;
}
#ifndef FLOWCTRL_91E
//return 0 success
//int seq_num =0;
int wlan_sdio_flow_ctrl_91e(wlan_private * priv)
{
	int ret = -1;
	u8 status = 0;
	s32 count = 0;
	wlan_sdio_card *card = (wlan_sdio_card *) priv->card;

	wlan_wake_up_card(priv);

	ENTER();

	while (!priv->CardRemoved) {
		sdio_claim_host(card->func);
		ret = wlan_read_byte(priv, IF_SDIO_FUN1_INT_PEND, &status);
		sdio_release_host(card->func);
		if (status & IF_SDIO_INT_RXCMPL)
			WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "IF_SDIO_FUN1_INT_PEND:count count %d : %x \n", count, status);
		if (!ret) {
			if ((status & IF_SDIO_INT_RXCMPL) == 0) {	//flows ctrl failed
				if (count > FLOW_CTRL_RXCMPL_RETRY_COUNT_91) {
					sdio_claim_host(card->func);
					wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, 0x40);
					sdio_release_host(card->func);
					complete(&priv->RxThread.comp);
					//msleep(100);
					ret = 0;
					//seq_num++;
					//if(count>4)
					//printk("count is %d,seq is %d,sleep_st is %d\n",count,seq_num,priv->CardInSleep);
					break;
				} else {
					count++;
				}
				//schedule();
				if(count < 20)
					udelay(2);
				else
					msleep(1);
			} else {
				sdio_claim_host(card->func);
				ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND,0x40);
				sdio_release_host(card->func);
				//seq_num++;
				//if(count>5)
				//printk("count is %d,seq is %d,sleep_st is %d\n",count,seq_num,priv->CardInSleep);
				if (!ret) {
					break;
				}
			}
		}
		//schedule();

	}

	LEAVE();
	return ret;
}
#else
//int seq_num =0;
//return 0 success
int wlan_sdio_flow_ctrl_91e(wlan_private * priv)
{
	int ret = 0;
	u8 status = 0;
	s32 int_sleep_count = 0;
	wlan_sdio_card *card = (wlan_sdio_card *) priv->card;

	ENTER();

	wlan_wake_up_card(priv);

	while ((!priv->CardRemoved) && is_sdio_patch_complete()) {
		sdio_claim_host(card->func);
		ret = wlan_read_byte(priv, IF_SDIO_FUN1_INT_PEND, &status);
		sdio_release_host(card->func);
		if (ret) {
			WLAN_ERRP("wlan read IF_SDIO_FUN1_INT_PEND failed \n");
			schedule();
			continue;
		}

		if ((status & IF_SDIO_INT_SLEEP) == 0) {	// If SDIO can't write
			if (int_sleep_count >= FLOW_CTRL_INT_SLEEP_RETRY_COUNT_91){
				//seq_num++;
				//printk("count is %d,seq is %d,sleep_st is %d\n",int_sleep_count,seq_num,priv->CardInSleep);
				break;
			} else {
				int_sleep_count++;
			}
		} else {

			//seq_num++;
			//if(int_sleep_count > 10)
			//	printk("count is %d,seq is %d,sleep_st is %d\n",int_sleep_count,seq_num,priv->CardInSleep);
			sdio_claim_host(card->func);
			ret = wlan_write_byte(priv, IF_SDIO_FUN1_INT_PEND, IF_SDIO_INT_SLEEP);
			sdio_release_host(card->func);
			break;
		}
		if(int_sleep_count < 20)
			udelay(10);
		else
			msleep(1);
	}
	LEAVE();
	return ret;
}
#endif

