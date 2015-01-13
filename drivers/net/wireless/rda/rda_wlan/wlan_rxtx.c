#include "wlan_includes.h"

void wlan_process_rx(wlan_private *priv)
{
	wlan_rx_packet_node * rxNode = NULL;
	u8 *payload = NULL, rx_type = 0, msg_type = 0;
	u16 rx_len = 0;
	struct sk_buff *skb = NULL;
	unsigned long flags;

	ENTER();

	spin_lock_irqsave(&priv->RxLock, flags);
	if (list_empty(&priv->RxQueue)) {
		priv->RxQuNum=0;
		spin_unlock_irqrestore(&priv->RxLock, flags);
		return;
	}
	rxNode = list_first_entry(&priv->RxQueue, struct _wlan_rx_packet_node, List);
	list_del(&rxNode->List);
	if (priv->RxQuNum > 0)
		priv->RxQuNum--;
	spin_unlock_irqrestore(&priv->RxLock, flags);

	skb = rxNode->Skb;
	payload = skb->data;

	rx_type = payload[1] & 0xf0;
	rx_len = skb->len;

	WLAN_DBGLAP(WLAN_DA_TXRX, WLAN_DL_TRACE,"payload len: %d type: %d \n", rx_len, rx_type);

	switch(rx_type){
	case HOST_MSG_CONFIGRSP:
		{
			msg_type = payload[2];

			switch (msg_type) {
				case 'I':
					wlan_mac_status(priv, payload + 2, rx_len - 2);
					break;

				case 'R':
					wlan_wid_response(priv, payload + 2,  rx_len - 2);
					break;

				case 'N':
					wlan_network_information(priv, payload + 2,  rx_len - 2);
					break;
					
				default:
					break;
			}
		}
		break;

	case HOST_MSG_DATAIN:
		{
			skb_pull(skb, WID_HEADER_LEN);
			skb->dev = priv->netDev;
			skb->protocol = eth_type_trans(skb, priv->netDev);
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			
			{
				struct ethhdr *eth;
				eth = eth_hdr(skb);
				//for wapi should verify packet length
				if (ntohs(eth->h_proto) == 0x88b4){
					rx_len = ((skb->data[6]) << 8) | skb->data[7];
					skb->len = rx_len;
				}
			}

			priv->stats.rx_packets++;
			priv->stats.rx_bytes += skb->len;

			WLAN_DBGLAP(WLAN_DA_TXRX, WLAN_DL_TRACE,
			"DATA: %02x %02x %02x %02x ... ... %02x %02x %02x %02x\n",
				    skb->data[0], skb->data[1], skb->data[2],
				    skb->data[3], skb->data[4], skb->data[5],
			skb->data[6], skb->data[7]);

			netif_rx_ni(skb);	
			goto out_keep_skb;
		}
		break;

	default:
		break;
	}
	
#ifdef WLAN_RAW_DATA_DEBUG
	WLAN_ERRP("%x %x %x %x %x %x %x %x\n", payload[0], payload[1],payload[2],payload[3],payload[4],payload[5],payload[6],payload[7]);
	payload += 8;
	WLAN_ERRP("%x %x %x %x %x %x %x %x rx_len:%d\n", payload[0], payload[1],payload[2],payload[3],payload[4],payload[5],payload[6],payload[7], rx_len);
#endif

	dev_kfree_skb(skb);
	
out_keep_skb:
	kfree(rxNode);
	LEAVE();
	return;
}


int wlan_rx_thread(void *data)
{
	wlan_thread *thread = (wlan_thread *)data;
	wlan_private *priv = thread->priv;
	
	ENTER();

	wlan_activate_thread(thread);
	
	current->flags |= PF_NOFREEZE;

	while(1){
		wait_for_completion_interruptible(&thread->comp);

		if (kthread_should_stop()){
			WLAN_DBGLAP(WLAN_DA_TXRX, WLAN_DL_CRIT,"wlan_rx_thread: break from main thread \n");
			break;
		}

		if(priv->CardRemoved)
			break;
		while (priv->RxQuNum)
		wlan_process_rx(priv);
	}

	wlan_deactivate_thread(thread);
	
	LEAVE();
	return 0;
}

int wlan_tx_thread(void *data)
{
	u32 payloadLen = 0;
	u8* payload = 0;
	wlan_wid_packet_node * widNode = NULL;
	wlan_tx_packet_node *	txNode = NULL;
	wlan_thread *thread = data;
	wlan_private *priv = thread->priv;
	unsigned long flags = 0;
	int tx_queue_max_num =0;
	
#ifdef WLAN_RAW_DATA_DEBUG		
	u8* printStr = NULL, 
#endif	   
	ENTER();	

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E){
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_90;
	} else if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F) {
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_91;
	} else {
		tx_queue_max_num = WLAN_TX_QUEUE_NUM_90;
	}

	wlan_activate_thread(thread);
	
	current->flags |= PF_NOFREEZE;

	while(1) {
		
		wait_for_completion_interruptible(&thread->comp);
		
		if (kthread_should_stop() || priv->CardRemoved) {
			WLAN_DBGLAP(WLAN_DA_TXRX, WLAN_DL_CRIT,"wlan_tx_thread: break from main thread \n");
			break;
		}

		WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "Tx Thread wakeup \n");

		//wid_header_len ---data +++ data_len: whole buf_len= wid_header_len + skb->len
		//send data 
		while(1){
			spin_lock_irqsave(&priv->TxLock, flags);	
			if(list_empty(&priv->TxQueue)){
				spin_unlock_irqrestore(&priv->TxLock, flags);
				wlan_card_enter_sleep(priv);
				break;
			}
			txNode = (wlan_tx_packet_node*)priv->TxQueue.next;
			if(txNode){
				list_del(&txNode->List);
				spin_unlock_irqrestore(&priv->TxLock, flags);
			}else{				
				WLAN_ERRP("should not happend!! \n");
				spin_unlock_irqrestore(&priv->TxLock, flags);
				break;
			}
			
			if(txNode->type == WLAN_DATA){
				payloadLen = txNode->Skb->len + WID_HEADER_LEN;
				payload = txNode->Skb->data - WID_HEADER_LEN;
				//the first 2 byte was reserved from hard_header_len
				payload[0] = (char)((payloadLen)&0xFF);
				payload[1] = (char)(((payloadLen)>>8)&0x0F);
				payload[1] |= 0x10;  // for DataOut 0x1 				 
			}else if(txNode->type == WLAN_CMD){
				widNode = txNode->wid_node;
				if(!widNode->BufLen || !widNode->Buf){
					WLAN_ERRP("WLAN_CMD widNode->BufLen is 0 or widNode->Buf is NULL! \n");
					kfree(txNode);
					break;
				}
				payloadLen = widNode->BufLen + WID_HEADER_LEN;				
				payload = widNode->Buf;
				payload[0] = (char)((payloadLen)&0xFF);
				payload[1] = (char)(((payloadLen)>>8)&0x0F);
				payload[1] |= 0x40;  // for WidRequest 0x40
				if (is_sdio_init_complete())
					init_completion(&priv->widComp);
			} else
				break;

#ifdef WLAN_RAW_DATA_DEBUG
			printStr = payload;
			WLAN_ERRP("&&&send a data Packet to chip length:%d \n", payloadLen);
			WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1], printStr[2], printStr[3], printStr[4], printStr[5], printStr[6], printStr[7]);
			printStr += 8;
			WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1], printStr[2], printStr[3], printStr[4], printStr[5], printStr[6], printStr[7]);
			printStr += 8;
			WLAN_ERRP("%x %x %x %x %x %x %x %x\n", printStr[0], printStr[1], printStr[2], printStr[3], printStr[4], printStr[5], printStr[6], printStr[7]);
#endif
			
		   //for sdio must 4 bytes align
			payloadLen = ((payloadLen + 3)/4)*4; // 4 byte align

			if(wlan_write_sdio_2_ahb(priv, IF_SDIO_FUN1_FIFO_WR, payload, payloadLen)){
				if(txNode->type == WLAN_DATA){
					priv->stats.tx_dropped++;
					priv->stats.tx_errors++;
				}
				WLAN_ERRP("wlan_write_bytes:  Tx thread write data failed payload:%d \n", payloadLen);
			}else{
				 WLAN_DBGLAP(WLAN_DA_TXRX, WLAN_DL_DEBUG,"&&& send payload:%d type:%d \n", payloadLen, txNode->type);

				if(txNode->type == WLAN_DATA){
					priv->stats.tx_packets++;
					priv->stats.tx_bytes += txNode->Skb->len;
				}else{
					int left = 0;
					if (!is_sdio_init_complete()) {
                    	if (wlan_read_wid_rsp_polling(priv)) {
                    	}
                	}else{
						left = wait_for_completion_timeout(&priv->widComp, 5*HZ);
						if(!left)
							WLAN_ERRP("&&& wait wid cmd time out \n");
					}
				}
			}
			
			if(txNode->type == WLAN_DATA){
				if(atomic_read(&(priv->TxQuNum)) > 0){
					atomic_sub(1, &(priv->TxQuNum)); 
				}
				if ((atomic_read(&(priv->netifQuStop)) == 1)
				    && (atomic_read(&(priv->TxQuNum)) < tx_queue_max_num)) {
					atomic_set(&(priv->netifQuStop), 0);
					wlan_push_event(priv, WLAN_EVENT_START_NETIF, priv, FALSE);
				}

			}

			//no matter it's send success or not we should free the skb buffer	 
			if(txNode->Skb)
				dev_kfree_skb(txNode->Skb);
			kfree(txNode);
			//do not send multi packet in one complete event
			break;
		}
		if(priv->CardInSleep == false)
			wlan_mod_timer(&priv->CardToSleepTimer, CARD_ENTER_SLEEP_TIMER);

			  
		WLAN_DBGLAP(WLAN_DA_SDIO, WLAN_DL_DEBUG, "Tx Thread sleep \n");
	}

	wlan_deactivate_thread(thread);

	LEAVE();
	return 0;
}

