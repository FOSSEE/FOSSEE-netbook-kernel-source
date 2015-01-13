#include "wlan_includes.h"

void wlan_clean_wid_node(wlan_wid_packet_node * widNode)
{
	if (!widNode)
		return;

	if (widNode->Buf)
		kfree(widNode->Buf);

	if (widNode->RspBuf)
		kfree(widNode->RspBuf);

	widNode->Buf = NULL;
	widNode->BufLen = 0;
	widNode->RspBuf = NULL;
	widNode->RspLen = 0;
	widNode->WidWaitOption = FALSE;
	widNode->WidCmd = 0;
	widNode->WidMsgId = 0;
}

wlan_wid_packet_node *wlan_get_wid_node_in_freeQ(wlan_private * priv)
{
	wlan_wid_packet_node *widNode = NULL;

	ENTER();

	if (priv->Suspend || priv->CardRemoved)
		return NULL;

	spin_lock(&priv->WidLock);
	if (!list_empty(&priv->WidFreeQ)) {
		widNode = (wlan_wid_packet_node *) priv->WidFreeQ.next;
		list_del(&widNode->List);
	}
	spin_unlock(&priv->WidLock);

	if (widNode)
		wlan_clean_wid_node(widNode);
	else
		WLAN_ERRP("no free wid node \n");

	LEAVE();
	return widNode;
}

void wlan_put_wid_node_in_freeQ(wlan_private * priv,
				wlan_wid_packet_node * widNode)
{
	if (!widNode || priv->CardRemoved)
		return;

	ENTER();
	spin_lock(&priv->WidLock);
	list_add_tail(&widNode->List, &priv->WidFreeQ);
	spin_unlock(&priv->WidLock);
	LEAVE();
}

int wlan_put_wid_node_in_pendingQ(wlan_private * priv,
				  wlan_wid_packet_node * widNode)
{
	int ret = 0;
	wlan_tx_packet_node *txPacketNode = NULL;

	if (!widNode || priv->CardRemoved)
		return -1;

	txPacketNode = (wlan_tx_packet_node *)kzalloc(sizeof(wlan_tx_packet_node), GFP_KERNEL);
	if (!txPacketNode) {
		WLAN_ERRP("no memory \n");
		return -ENOMEM;
	}

	ENTER();

	txPacketNode->type = WLAN_CMD;
	txPacketNode->wid_node = widNode;

	//add to pending queue   
	widNode->WidWaitOption = FALSE;

	//add to pending queue 
	spin_lock(&priv->WidLock);
	list_add_tail(&widNode->List, &priv->WidPendingQ);
	spin_unlock(&priv->WidLock);

	//add to tx queue , send to card
	spin_lock(&priv->TxLock);
	list_add_tail(&txPacketNode->List, &priv->TxQueue);
	spin_unlock(&priv->TxLock);

	complete(&priv->TxThread.comp);
	ret = wait_event_timeout(widNode->WidDone, widNode->WidWaitOption, HZ * 5);
	if (!ret) {
		WLAN_ERRP("wait event timeout \n");
		priv->EventErrorCount++;
	} else if (ret == -ERESTARTSYS) {
		WLAN_ERRP("wait event was interrupt by signal \n");
		priv->EventErrorCount++;
	} else {
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_put_wid_node_in_pendingQ received event success!! \n");
		priv->EventErrorCount=0;
	}

	//delete from pending queue do not add to freeQ now because we should get wid response
	spin_lock(&priv->WidLock);
	widNode->BufLen = 0;
	list_del(&widNode->List);
	spin_unlock(&priv->WidLock);

#ifdef CHECK_SDIO_STAUTS
	if (!rda_combo_wifi_in_test_mode() && is_sdio_init_complete() && priv->EventErrorCount > WLAN_EVENT_MAX_ERR) {
		WLAN_ERRP("Wait event timeout, push event : WLAN_EVENT_CHECK_SDIO \n");
		wlan_push_event(priv, WLAN_EVENT_CHECK_SDIO, priv, FALSE);
		priv->EventErrorCount = 0;
		priv->sdio_need_reset = 1;
	}
#endif

	if (ret > 0)
		ret = 0;
	else if (!ret)
		ret = -1;

	LEAVE();

	return ret;
}

//do not del wid node in this, after wid complete it will be delete
wlan_wid_packet_node *wlan_get_wid_node_in_pendingQ(wlan_private * priv)
{
	wlan_wid_packet_node *widNode = NULL;

	if (priv->Suspend || priv->CardRemoved)
		return NULL;

	spin_lock(&priv->WidLock);
	if (!list_empty(&priv->WidPendingQ)) {
		widNode = (wlan_wid_packet_node *) priv->WidPendingQ.next;
	}
	spin_unlock(&priv->WidLock);
	if (!widNode)
		WLAN_ERRP("no wid pendingQ \n");
	return widNode;
}

int wlan_alloc_wid_queue(wlan_private * priv)
{
	int ret = -1;
	u32 i = 0;
	wlan_wid_packet_node *widNode;

	for (i = 0; i < WLAN_CMD_QUEUE_NUM; i++) {
		widNode = kzalloc(sizeof(wlan_wid_packet_node), GFP_KERNEL);
		if (widNode) {
			init_waitqueue_head(&widNode->WidDone);
			spin_lock(&priv->WidLock);
			list_add_tail(&widNode->List, &priv->WidFreeQ);
			spin_unlock(&priv->WidLock);
			ret = 0;
		} else {
			WLAN_ERRP("kzalloc wlan_wid_packet_node memory failed!\n");
			ret =-1;
			break;
		}
	}
	return ret;
}

int wlan_release_wid_pending_queue(wlan_private * priv)
{
	wlan_wid_packet_node *widNode;
	struct list_head *qe, *qen;
	ENTER();

	spin_lock(&priv->WidLock);
	if (!list_empty(&priv->WidPendingQ)) {
		list_for_each_safe(qe, qen, &priv->WidPendingQ) {
			widNode = (wlan_wid_packet_node *) qe;
			list_del(&widNode->List);

			wake_up(&widNode->WidDone);
		}
	}
	spin_unlock(&priv->WidLock);
	LEAVE();
	return 0;
}

int wlan_free_wid_queue(wlan_private * priv)
{
	struct list_head *qe = NULL, *qen = NULL;
	wlan_wid_packet_node *widNode;

	ENTER();

	spin_lock(&priv->WidLock);
	if (!list_empty(&priv->WidFreeQ)) {
		list_for_each_safe(qe, qen, &priv->WidFreeQ) {
			widNode = (wlan_wid_packet_node *) qe;
			list_del(&widNode->List);

			if (widNode->Buf)
				kfree(widNode->Buf);
			widNode->Buf = NULL;

			if (widNode->RspBuf)
				kfree(widNode->RspBuf);
			widNode->RspBuf = NULL;

			kfree(widNode);
		}
	}
	spin_unlock(&priv->WidLock);
	LEAVE();
	return 0;
}

int wlan_read_wid_rsp_polling(wlan_private * priv)
{
	u8 status;
	int ret = 0;
	u8 size_l = 0, size_h = 0;
	u16 size = 0, rx_len = 0;
	struct sk_buff *skb = NULL;
	s16 count = 1000;
	u8 *payload = NULL, rx_type = 0, msg_type = 0;
	wlan_sdio_card *card = (wlan_sdio_card *) priv->card;

	ENTER();
	while (!is_sdio_init_complete() && !priv->CardRemoved && count--) {
		sdio_claim_host(card->func);
		ret = wlan_read_byte(priv, IF_SDIO_FUN1_INT_STAT, &status);
		if (!ret) {
			if (status & IF_SDIO_INT_AHB2SDIO) {
				ret = wlan_read_byte(priv, IF_SDIO_AHB2SDIO_PKTLEN_L, &size_l);
				if (ret) {
					sdio_release_host(card->func);
					break;
				}

				ret = wlan_read_byte(priv, IF_SDIO_AHB2SDIO_PKTLEN_H, &size_h);
				if (ret) {
					sdio_release_host(card->func);
					break;
				}

				size = (size_l | ((size_h & 0x7f) << 8)) * 4;
				WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_NORM, "size = %d \n", size);

				skb = dev_alloc_skb(size + NET_IP_ALIGN + 3);
				if (!skb) {
					sdio_release_host(card->func);
					ret = -1;
					break;
				}

				skb_reserve(skb, NET_IP_ALIGN);
				skb_align(skb, 4);

				if (wlan_read_bytes(priv, IF_SDIO_FUN1_FIFO_RD, skb->data, size)
				    || priv->CardRemoved) {
					dev_kfree_skb(skb);
					sdio_release_host(card->func);
					ret = -1;
					goto out;
				}
				sdio_release_host(card->func);
				skb_put(skb, size);

				payload = skb->data;

				rx_type = payload[1] & 0xf0;
				rx_len = (u16) (payload[0] + ((payload[1] & 0x0f) << 8));
				if (rx_type == HOST_MSG_CONFIGRSP) {
					msg_type = payload[2];
					if (msg_type == 'R') {
						wlan_wid_response(priv, payload + 2, rx_len - 2);
						dev_kfree_skb(skb);
						ret = 0;
						break;
					} else if (msg_type == 'I') {
						WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_NORM, "received a mac status \n");
					}
				}
				dev_kfree_skb(skb);
			} else
				sdio_release_host(card->func);
		} else
			sdio_release_host(card->func);
		udelay(10);
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_NORM, "IF_SDIO_FUN1_INT_STAT count:%d stats:0x%x\n", count, status);
	}

	if (count < 0) {
		ret = -1;
		WLAN_ERRP("polling wid rsp failed \n");
	} else {
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_NORM, "polling wid rsp success \n");
	}

out:
	LEAVE();
	return ret;
}

//return 0 success
int wlan_generic_get(wlan_private * priv,
		     u16 wid, u8 * val, u16 val_len, u32 * rspLen,
		     WID_TYPE_T type)
{
	int ret;
	u8 wid_msg_id;
	wlan_wid_packet_node *widNode = NULL;
	u8 *wid_req = NULL;
	u8 wid_req_len = 6;

	ENTER();

	if(priv->sdio_need_reset == 1)
		return -ENOMEM;

	widNode = wlan_get_wid_node_in_freeQ(priv);

	if (!widNode)
		return -ENOMEM;

	wid_msg_id = priv->wid_msg_id++;

	widNode->Buf = kmalloc(wid_req_len + WID_HEADER_LEN, GFP_KERNEL);
	widNode->BufLen = wid_req_len;

	widNode->RspBuf = kmalloc(val_len, GFP_KERNEL);
	widNode->RspLen = val_len;
	widNode->WidMsgId = wid_msg_id;
	widNode->WidCmd = wid;
	widNode->BufType = type;

	wid_req = widNode->Buf + WID_HEADER_LEN;

	wid_req[0] = 'Q';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	wid_req[4] = (char)(wid & 0x00FF);
	wid_req[5] = (char)((wid & 0xFF00) >> 8);

	ret = wlan_put_wid_node_in_pendingQ(priv, widNode);

	if (!ret) {
		memcpy(val, widNode->RspBuf, widNode->RspLen);
		if (rspLen)
			*rspLen = widNode->RspLen;
	}

	wlan_put_wid_node_in_freeQ(priv, widNode);
	LEAVE();
	return ret;
}

int wlan_generic_get_uchar(wlan_private * priv, u16 wid, u8 * val)
{
	int ret;
	ENTER();

	ret = wlan_generic_get(priv, wid, val, 1, NULL, WID_CHAR);

	LEAVE();
	return ret;
}

int wlan_generic_get_ushort(wlan_private * priv, u16 wid, u8 * val)
{
	int ret;
	ENTER();

	ret = wlan_generic_get(priv, wid, val, 2, NULL, WID_SHORT);

	LEAVE();
	return ret;
}

int wlan_generic_get_ulong(wlan_private * priv, u16 wid, u8 * val)
{
	int ret;
	ENTER();

	ret = wlan_generic_get(priv, wid, (u8 *) val, 4, NULL, WID_INT);

	LEAVE();
	return ret;
}

int wlan_generic_get_str(wlan_private * priv,
			 u16 wid, u8 * val, u32 len, u32 * rspLen)
{
	int ret;
	ENTER();

	ret = wlan_generic_get(priv, wid, val, len, rspLen, WID_STR);

	LEAVE();
	return ret;
}

//return 0 success
int wlan_send_wid_packet(wlan_private * priv, u8 * val, u16 val_len,
			 u8 wid_msg_id)
{
	int ret = 0;
	wlan_wid_packet_node *widNode = NULL;
	u8 *wid_req = NULL;
	u16 wid_req_len = val_len;

	ENTER();

	if(priv->sdio_need_reset == 1)
		return -ENOMEM;

	widNode = wlan_get_wid_node_in_freeQ(priv);
	if (!widNode)
		return -ENOMEM;

	widNode->Buf = kmalloc(wid_req_len + WID_HEADER_LEN, GFP_KERNEL);
	widNode->BufLen = wid_req_len;

	widNode->RspBuf = kmalloc(4, GFP_KERNEL);
	widNode->RspLen = 4;
	widNode->WidMsgId = wid_msg_id;
	widNode->WidCmd = WID_STATUS;
	widNode->BufType = WID_STR;

	wid_req = widNode->Buf + WID_HEADER_LEN;
	memcpy(wid_req, val, val_len);

	ret = wlan_put_wid_node_in_pendingQ(priv, widNode);
	//check wid status
	if (!ret) {
		if (widNode->RspBuf[0] != WID_STATUS_SUCCESS)
			ret = -EINVAL;
	}

	wlan_put_wid_node_in_freeQ(priv, widNode);
	LEAVE();
	return ret;
}

int wlan_generic_set(wlan_private * priv,
		     u16 wid, u8 * val, u16 val_len, WID_TYPE_T type)
{
	int ret;
	u8 wid_msg_id;
	wlan_wid_packet_node *widNode = NULL;
	u8 *wid_req = NULL;
	u8 wid_req_len = 7 + val_len;

	ENTER();

	if(priv->sdio_need_reset == 1)
		return -ENOMEM;

	widNode = wlan_get_wid_node_in_freeQ(priv);
	if (!widNode)
		return -ENOMEM;

	wid_msg_id = priv->wid_msg_id++;

	if(type == WID_BIN_DATA)
		wid_req_len += 2; //1for length feild, 1 for crc

	widNode->Buf = kmalloc(wid_req_len + WID_HEADER_LEN, GFP_KERNEL);
	widNode->BufLen = wid_req_len;

	widNode->RspBuf = kmalloc(4, GFP_KERNEL);
	widNode->RspLen = 4;
	widNode->WidMsgId = wid_msg_id;
	widNode->WidCmd = wid;
	widNode->BufType = type;

	wid_req = widNode->Buf + WID_HEADER_LEN;

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	wid_req[4] = (char)(wid & 0x00FF);
	wid_req[5] = (char)((wid & 0xFF00) >> 8);

	if(type != WID_BIN_DATA){
		wid_req[6] = val_len;
		memcpy(&wid_req[7], val, val_len);
	}else{
		wid_req[6] = val_len & 0xff;
		wid_req[7] = (val_len & 0xff00) >> 8;
		memcpy(&wid_req[8], val, val_len);
	}

	ret = wlan_put_wid_node_in_pendingQ(priv, widNode);
	//check wid status
	if (!ret) {
		if (widNode->RspBuf[0] != WID_STATUS_SUCCESS)
			ret = -EINVAL;
	}

	wlan_put_wid_node_in_freeQ(priv, widNode);
	LEAVE();
	return ret;
}

int wlan_generic_set_uchar(wlan_private * priv, u16 wid, u8 val)
{
	int ret;
	u8 tPara = val;
	ENTER();

	ret = wlan_generic_set(priv, wid, &tPara, 1, WID_CHAR);

	LEAVE();
	return ret;
}

int wlan_generic_set_ushort(wlan_private * priv, u16 wid, u16 val)
{
	int ret;
	u16 tPara = val;
	ENTER();

	ret = wlan_generic_set(priv, wid, (u8 *) & tPara, 2, WID_SHORT);

	LEAVE();
	return ret;
}

int wlan_generic_set_ulong(wlan_private * priv, u16 wid, u32 val)
{
	int ret;
	u32 tPara = val;
	ENTER();

	ret = wlan_generic_set(priv, wid, (u8 *) & tPara, 4, WID_INT);

	LEAVE();
	return ret;
}

int wlan_generic_set_str(wlan_private * priv, u16 wid, u8 * val, u32 val_len)
{
	int ret;
	ENTER();

	ret = wlan_generic_set(priv, wid, val, val_len, WID_STR);

	LEAVE();
	return ret;
}

int wlan_generic_set_bin(wlan_private * priv, u16 wid, u8 * val, u32 val_len)
{
    int ret;
    ENTER();

    ret = wlan_generic_set(priv, wid, val, val_len, WID_BIN_DATA);

    LEAVE();
    return ret;
}

int wlan_set_core_init_patch(wlan_private * priv, const u32(*data)[2], u8 num)
{
	int ret, count = 0;
	u8 wid_msg_id;
	wlan_wid_packet_node *widNode = NULL;
	u8 *wid_req = NULL, *p_wid_req;
	u8 wid_req_len = 4 + 14 * num;
	u16 wid = WID_STATUS;

	ENTER();

	widNode = wlan_get_wid_node_in_freeQ(priv);

	if (!widNode)
		return -ENOMEM;

	wid_msg_id = priv->wid_msg_id++;

	widNode->Buf = kmalloc(wid_req_len + WID_HEADER_LEN + 4, GFP_KERNEL);
	widNode->BufLen = wid_req_len;

	widNode->RspBuf = kmalloc(4, GFP_KERNEL);
	widNode->RspLen = 4;
	widNode->WidMsgId = wid_msg_id;
	widNode->WidCmd = wid;
	widNode->BufType = WID_STR;
	wid_req = widNode->Buf + WID_HEADER_LEN;

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	p_wid_req = wid_req + 4;
	for (count = 0; count < num; count++) {
		wid = WID_MEMORY_ADDRESS;
		p_wid_req[0] = (char)(wid & 0x00FF);
		p_wid_req[1] = (char)((wid & 0xFF00) >> 8);

		p_wid_req[2] = (char)4;
		memcpy((u8 *) (p_wid_req + 3), (u8 *) (&data[count][0]), 4);

		wid = WID_MEMORY_ACCESS_32BIT;
		p_wid_req[7] = (char)(wid & 0x00FF);
		p_wid_req[8] = (char)((wid & 0xFF00) >> 8);

		p_wid_req[9] = (char)4;
		memcpy((u8 *) (p_wid_req + 10), (u8 *) (&data[count][1]), 4);
		p_wid_req += 14;
	}
	ret = wlan_put_wid_node_in_pendingQ(priv, widNode);
	//check wid status
	if (!ret) {
		if (widNode->RspBuf[0] != WID_STATUS_SUCCESS)
			ret = -EINVAL;
	}

	wlan_put_wid_node_in_freeQ(priv, widNode);

	LEAVE();
	return ret;
}

int wlan_set_core_patch(wlan_private * priv, const u8(*patch)[2], u8 num)
{
	int ret, count = 0;
	u8 wid_msg_id;
	wlan_wid_packet_node *widNode = NULL;
	u8 *wid_req = NULL, *p_wid_req;
	u8 wid_req_len = 4 + 8 * num;
	u16 wid = WID_STATUS;

	ENTER();

	widNode = wlan_get_wid_node_in_freeQ(priv);

	if (!widNode)
		return -ENOMEM;

	wid_msg_id = priv->wid_msg_id++;

	widNode->Buf = kmalloc(wid_req_len + WID_HEADER_LEN + 4, GFP_KERNEL);
	widNode->BufLen = wid_req_len;

	widNode->RspBuf = kmalloc(4, GFP_KERNEL);
	widNode->RspLen = 4;
	widNode->WidMsgId = wid_msg_id;
	widNode->WidCmd = wid;
	widNode->BufType = WID_STR;
	wid_req = widNode->Buf + WID_HEADER_LEN;

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	p_wid_req = wid_req + 4;

	for (count = 0; count < num; count++) {
		wid = WID_PHY_ACTIVE_REG;
		p_wid_req[0] = (char)(wid & 0x00FF);
		p_wid_req[1] = (char)((wid & 0xFF00) >> 8);

		p_wid_req[2] = (char)(0x01);
		p_wid_req[3] = (char)patch[count][0];

		wid = WID_PHY_ACTIVE_REG_VAL;
		p_wid_req[4] = (char)(wid & 0x00FF);
		p_wid_req[5] = (char)((wid & 0xFF00) >> 8);

		p_wid_req[6] = (char)(0x01);
		p_wid_req[7] = (char)patch[count][1];
		p_wid_req += 8;
	}
	ret = wlan_put_wid_node_in_pendingQ(priv, widNode);
	//check wid status
	if (!ret) {
		if (widNode->RspBuf[0] != WID_STATUS_SUCCESS)
			ret = -EINVAL;
	}

	wlan_put_wid_node_in_freeQ(priv, widNode);

	LEAVE();
	return ret;
}

void wlan_wid_response(wlan_private * priv, u8 * wid_rsp, u16 wid_rsp_len)
{
	u16 rsp_len;
	u16 rsp_wid;
	u8 msg_id = 0;
	u8 *payload = NULL, payload_len = 0;
	struct list_head *qe, *qen;
	wlan_wid_packet_node *widNode = NULL;

	if (wid_rsp[0] != 'R') {
		WLAN_ERRP("wid_rsp[0] != 'R'\n");
		goto err;
	}

	if (wid_rsp_len < 4) {
		WLAN_ERRP("wid_rsp_len < 4\n");
		goto err;
	}

	rsp_len = wid_rsp[2] | (wid_rsp[3] << 8);
	if (wid_rsp_len != rsp_len) {
		WLAN_ERRP("wid_rsp_len not match, %d != %d\n", wid_rsp_len, rsp_len);
		goto err;
	}

	if (wid_rsp_len < 7) {
		WLAN_ERRP("wid_rsp_len < 7\n");
		goto err;
	}

	msg_id = wid_rsp[1];
	rsp_wid = wid_rsp[4] | (wid_rsp[5] << 8);
	payload_len = wid_rsp[6];
	payload = &wid_rsp[7];

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG,"msg_id:%d rsp_wid:%d \n", msg_id, rsp_wid);
	spin_lock(&priv->WidLock);
	list_for_each_safe(qe, qen, &priv->WidPendingQ) {
		widNode = (wlan_wid_packet_node *) qe;
		//process wid write
		if (rsp_wid == WID_STATUS) {
			if (widNode->WidMsgId == msg_id) {
				payload_len = widNode->RspLen > payload_len ? payload_len : widNode->RspLen;
				memcpy(widNode->RspBuf, payload, payload_len);
				widNode->WidWaitOption = TRUE;
				wake_up(&widNode->WidDone);
				if (is_sdio_init_complete())
					complete(&priv->widComp);
			}
		} else {	//process wid query
			if (widNode->WidMsgId == msg_id) {
				rsp_len = widNode->BufLen > payload_len ? payload_len : widNode->BufLen;
				memcpy(widNode->RspBuf, payload, rsp_len);
				widNode->WidWaitOption = TRUE;
				widNode->RspLen = (u32) rsp_len;
				wake_up(&widNode->WidDone);
				if (is_sdio_init_complete())
					complete(&priv->widComp);
			}
		}
	}
	spin_unlock(&priv->WidLock);

err:

	return;
}

int wlan_set_scan_timeout(wlan_private * priv)
{
	int ret;
	char wid_req[24];
	u16 wid_req_len = 19;
	u8 wid_msg_id = priv->wid_msg_id++;
	u16 wid = 0;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_set_scan_timeout <<< \n");

	wid = WID_SITE_SURVEY_SCAN_TIME;

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	wid_req[4] = (char)(wid & 0x00FF);
	wid_req[5] = (char)((wid & 0xFF00) >> 8);
	wid_req[6] = 2;
	wid_req[7] = SCAN_TIME_AT_EACH_CHANNEL;	//50 ms one channel
	wid_req[8] = 0;

	wid = WID_ACTIVE_SCAN_TIME;
	wid_req[9] = (char)(wid & 0x00FF);
	wid_req[10] = (char)((wid & 0xFF00) >> 8);
	wid_req[11] = 2;
	wid_req[12] = SCAN_TIME_AT_EACH_CHANNEL;	//50 ms one channel
	wid_req[13] = 0;

	wid = WID_PASSIVE_SCAN_TIME;
	wid_req[14] = (char)(wid & 0x00FF);
	wid_req[15] = (char)((wid & 0xFF00) >> 8);
	wid_req[16] = 2;
	wid_req[17] = SCAN_TIME_AT_EACH_CHANNEL;	//50 ms one channel
	wid_req[18] = 0;

	ret = wlan_send_wid_packet(priv, wid_req, wid_req_len, wid_msg_id);

	return ret;
}

int wlan_start_scan_enable_network_info(wlan_private * priv)
{
	int ret;
	char wid_req[255], *pWid_req;
	u16 wid_req_len = 16;
	u16 wid;
	u8 wid_msg_id = 0;
	ENTER();

	wid_msg_id = priv->wid_msg_id++;

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	wid = WID_SITE_SURVEY;
	wid_req[4] = (char)(wid & 0x00FF);
	wid_req[5] = (char)((wid & 0xFF00) >> 8);

	wid_req[6] = (char)(0x01);
	wid_req[7] = (char)(0x01);

	wid = WID_START_SCAN_REQ;
	wid_req[8] = (char)(wid & 0x00FF);
	wid_req[9] = (char)((wid & 0xFF00) >> 8);

	wid_req[10] = (char)(0x01);
	wid_req[11] = (char)(0x01);

	wid = WID_NETWORK_INFO_EN;
	wid_req[12] = (char)(wid & 0x00FF);
	wid_req[13] = (char)((wid & 0xFF00) >> 8);

	wid_req[14] = (char)(0x01);
	wid_req[15] = (char)(0x01);	// 0x01 scan network info

	wid_req_len = 16;

	if (priv->version == WLAN_VERSION_90_D || priv->version == WLAN_VERSION_90_E) {
		int i = 0;
		wid_req_len = wid_req_len;
		pWid_req = &wid_req[wid_req_len];

		while (i <= priv->scan_ssid_len) {
			wid = WID_MEMORY_ADDRESS;
			pWid_req[0] = (char)(wid & 0x00FF);
			pWid_req[1] = (char)((wid & 0xFF00) >> 8);
			pWid_req[2] = 4;
			pWid_req[3] = 0x80 + i;
			pWid_req[4] = 0x81;
			pWid_req[5] = 0x10;
			pWid_req[6] = 0x00;
			wid_req_len += 7;
			pWid_req += 7;

			wid = WID_MEMORY_ACCESS_32BIT;
			pWid_req[0] = (char)(wid & 0x00FF);
			pWid_req[1] = (char)((wid & 0xFF00) >> 8);
			pWid_req[2] = 4;
			pWid_req[3] = priv->scan_ssid[i + 0];
			pWid_req[4] = priv->scan_ssid[i + 1];
			pWid_req[5] = priv->scan_ssid[i + 2];
			pWid_req[6] = priv->scan_ssid[i + 3];
			wid_req_len += 7;
			pWid_req += 7;
			i += 4;
		}
	} else if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F) {
		pWid_req = &wid_req[wid_req_len];
		wid = WID_HIDE_SSID;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = priv->scan_ssid_len;
		memcpy(pWid_req + 3, priv->scan_ssid, priv->scan_ssid_len);
		wid_req_len += 3 + priv->scan_ssid_len;
		pWid_req += 3 + priv->scan_ssid_len;
	}

	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG,
		    "wid_req_len : %d ssid_len:%d \n", wid_req_len,
		    priv->scan_ssid_len);

	ret = wlan_send_wid_packet(priv, wid_req, wid_req_len, wid_msg_id);
	if (ret) {
		WLAN_ERRP("wlan_send_wid_packet failed!\n");
	}
	LEAVE();
	return ret;
}

int wlan_start_join(wlan_private * priv)
{
	int ret = 0;
	char wid_req[255];
	u16 wid_req_len = 0;
	u16 wid = 0;
	u8 wid_msg_id = priv->wid_msg_id++;
	u16 i = 0;
	u8 key_str_len = 0;
	u8 key_str[26 + 1], *key, *pWid_req;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG,
		    "%s <<< mode:0x%x authtype:%d ssid:%s\n", __func__,
		    priv->imode, priv->authtype, priv->assoc_ssid);

	print_mac(priv->assoc_bssid);

	pWid_req = wid_req;
	pWid_req[0] = 'W';
	pWid_req[1] = wid_msg_id;
	wid_req_len = 4;
	pWid_req += 4;

	wid = WID_802_11I_MODE;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 1;
	if(priv->imode == 0x9 && (priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F)){//huanglei add for wps
		wid_req[3] = 0x49;
	}
	else
		pWid_req[3] = (priv->imode == 0x05) ? 0x07 : priv->imode;	//for wep104 need set imode 0x07 firmware problem
	wid_req_len += 4;
	pWid_req += 4;

	wid = WID_AUTH_TYPE;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 1;
	pWid_req[3] = priv->authtype;
	wid_req_len += 4;
	pWid_req += 4;

	wid = WID_NETWORK_INFO_EN;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 1;
	pWid_req[3] = 0;
	wid_req_len += 4;
	pWid_req += 4;

	wid = WID_CURRENT_TX_RATE;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 1;
	pWid_req[3] = 1;
	wid_req_len += 4;
	pWid_req += 4;

    if ((priv->version == WLAN_VERSION_90_D) || (priv->version == WLAN_VERSION_90_E)) {
	    //      bssid
	    wid = WID_MEMORY_ADDRESS;
	    pWid_req[0] = (char)(wid & 0x00FF);
	    pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	    pWid_req[2] = 4;
	    pWid_req[3] = 0xec;
	    pWid_req[4] = 0x81;
	    pWid_req[5] = 0x10;
	    pWid_req[6] = 0x00;
	    wid_req_len += 7;
	    pWid_req += 7;

	    wid = WID_MEMORY_ACCESS_32BIT;
	    pWid_req[0] = (char)(wid & 0x00FF);
	    pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	    pWid_req[2] = 4;
	    pWid_req[3] = priv->assoc_bssid[0];
	    pWid_req[4] = priv->assoc_bssid[1];
	    pWid_req[5] = priv->assoc_bssid[2];
	    pWid_req[6] = priv->assoc_bssid[3];
	    wid_req_len += 7;
	    pWid_req += 7;

	    wid = WID_MEMORY_ADDRESS;
	    pWid_req[0] = (char)(wid & 0x00FF);
	    pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	    pWid_req[2] = 4;
	    pWid_req[3] = 0xf0;
	    pWid_req[4] = 0x81;
	    pWid_req[5] = 0x10;
	    pWid_req[6] = 0x00;
	    wid_req_len += 7;
	    pWid_req += 7;

	    wid = WID_MEMORY_ACCESS_32BIT;
	    pWid_req[0] = (char)(wid & 0x00FF);
	    pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	    pWid_req[2] = 4;
	    pWid_req[3] = priv->assoc_bssid[4];
	    pWid_req[4] = priv->assoc_bssid[5];
	    pWid_req[5] = 0;
	    pWid_req[6] = 0;
	    wid_req_len += 7;
	    pWid_req += 7;
    }

	if ((priv->version == WLAN_VERSION_90_D) || (priv->version == WLAN_VERSION_90_E)) {
		//huanglei add begin
		wid = WID_MEMORY_ADDRESS;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 4;
		pWid_req[3] = 0x04;
		pWid_req[4] = 0x01;
		pWid_req[5] = 0x00;
		pWid_req[6] = 0x50;
		wid_req_len += 7;
		pWid_req += 7;

		wid = WID_MEMORY_ACCESS_16BIT;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 2;
		pWid_req[3] = 0x1;//(cmax << 4) | (cmin) 00010001
		pWid_req[4] = 0x1;
		wid_req_len += 5;
		pWid_req += 5;

		wid = WID_MEMORY_ADDRESS;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 4;
		pWid_req[3] = 0x08;
		pWid_req[4] = 0x01;
		pWid_req[5] = 0x00;
		pWid_req[6] = 0x50;
		wid_req_len += 7;
		pWid_req += 7;

		wid = WID_MEMORY_ACCESS_16BIT;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 2;
		pWid_req[3] = 0x1;//(cmax << 4) | (cmin) 00010001
		pWid_req[4] = 0x1;
		wid_req_len += 5;
		pWid_req += 5;

		wid = WID_MEMORY_ADDRESS;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 4;
		pWid_req[3] = 0x0C;
		pWid_req[4] = 0x01;
		pWid_req[5] = 0x00;
		pWid_req[6] = 0x50;
		wid_req_len += 7;
		pWid_req += 7;

		wid = WID_MEMORY_ACCESS_16BIT;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 2;
		pWid_req[3] = 0x1;//(cmax << 4) | (cmin) 00010001
		pWid_req[4] = 0x1;
		wid_req_len += 5;
		pWid_req += 5;

		wid = WID_MEMORY_ADDRESS;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 4;
		pWid_req[3] = 0x10;
		pWid_req[4] = 0x01;
		pWid_req[5] = 0x00;
		pWid_req[6] = 0x50;
		wid_req_len += 7;
		pWid_req += 7;

		wid = WID_MEMORY_ACCESS_16BIT;
		pWid_req[0] = (char)(wid & 0x00FF);
		pWid_req[1] = (char)((wid & 0xFF00) >> 8);
		pWid_req[2] = 2;
		pWid_req[3] = 0x1;//(cmax << 4) | (cmin) 00010001
		pWid_req[4] = 0x1;
		wid_req_len += 5;
		pWid_req += 5;
		//huanglei add end
	}

	//bssid
	wid = WID_BSSID;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 6;
	memcpy(&pWid_req[3], priv->assoc_bssid, 6);
	wid_req_len += 9;
	pWid_req += 9;

	//      ssid
	wid = WID_SSID;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = priv->assoc_ssid_len;
	memcpy(pWid_req + 3, priv->assoc_ssid, priv->assoc_ssid_len);
	wid_req_len += 3 + priv->assoc_ssid_len;
	pWid_req += 3 + priv->assoc_ssid_len;

	wid = WID_START_SCAN_REQ;
	pWid_req[0] = (char)(wid & 0x00FF);
	pWid_req[1] = (char)((wid & 0xFF00) >> 8);
	pWid_req[2] = 1;
	pWid_req[3] = 0;
	wid_req_len += 4;
	pWid_req += 4;


	wid = WID_WEP_KEY_VALUE0;
	//write wep key
	if (priv->imode == 3 || priv->imode == 5) {
		for (i = 0; i < 4; i++) {
			key = priv->wep_keys[i].key;

			if (priv->wep_keys[i].len == 0)
				continue;

			if (priv->wep_keys[i].len == KEY_LEN_WEP_40) {
				sprintf(key_str, "%02x%02x%02x%02x%02x\n",
					key[0], key[1], key[2], key[3], key[4]);
				key_str_len = 10;
				key_str[key_str_len] = '\0';
			} else if (priv->wep_keys[i].len == KEY_LEN_WEP_104) {
				sprintf(key_str, "%02x%02x%02x%02x%02x"
					"%02x%02x%02x%02x%02x"
					"%02x%02x%02x\n",
					key[0], key[1], key[2], key[3], key[4],
					key[5], key[6], key[7], key[8], key[9],
					key[10], key[11], key[12]);
				key_str_len = 26;
				key_str[key_str_len] = '\0';
			} else
				continue;

			pWid_req[0] = (char)((wid + i) & 0x00FF);
			pWid_req[1] = (char)(((wid + i) & 0xFF00) >> 8);

			pWid_req[2] = key_str_len;
			memcpy(pWid_req + 3, key_str, key_str_len);

			pWid_req += 3 + key_str_len;
			wid_req_len += 3 + key_str_len;
		}
	}



	wid_req[2] = (char)(wid_req_len & 0x00FF);
	wid_req[3] = (char)((wid_req_len & 0xFF00) >> 8);

	ret = wlan_send_wid_packet(priv, wid_req, wid_req_len, wid_msg_id);
	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG,
		    "%s >>> ret = %d req len %d mod:0x%x auth_type:0x%x \n",
		    __func__, ret, wid_req_len, priv->imode, priv->authtype);

	return ret;
}

int wlan_disconnect_silent(wlan_private * priv)
{
	int ret = 0;
	char null_data[6];	
	char wid_req[255], *pWid_req = NULL;
	u16 wid_req_len = 0;
	u16 wid = 0;
	u8	wid_msg_id = priv->wid_msg_id++;

	memset(null_data, 0 , 6);

	wid_req[0] = 'W';
	wid_req[1] = wid_msg_id;

	wid_req_len += 4;
	pWid_req = wid_req + 4;

	wid = WID_BSSID;
	pWid_req[0] = (char)(wid&0x00FF);
	pWid_req[1] = (char)((wid&0xFF00) >> 8);
	pWid_req[2] = 6;
	memcpy(&pWid_req[3], null_data, 6);
	wid_req_len += 9;
	pWid_req += 9;

	wid = WID_SSID;
	pWid_req[0] = (char)(wid&0x00FF);
	pWid_req[1] = (char)((wid&0xFF00) >> 8);
	pWid_req[2] = 6;
	memcpy(&pWid_req[3], null_data, 6);
	wid_req_len += 9;
	pWid_req += 9;

	wid_req[2] = (char)(wid_req_len&0x00FF);
	wid_req[3] = (char)((wid_req_len&0xFF00) >> 8);

	ret = wlan_send_wid_packet(priv, wid_req, wid_req_len, wid_msg_id);
	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG, 	"%s >>> ret = %d \n", __func__, ret);
	return ret;
}

int wlan_set_txrate(wlan_private * priv, u8 mbps)
{
	int ret;
	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_set_txrate <<< \n");
	ret = wlan_generic_set_uchar(priv, WID_CURRENT_TX_RATE, mbps);	//O FOR AUTO 1FOR 1MBPS

	if ((priv->version == WLAN_VERSION_90_D) || (priv->version == WLAN_VERSION_90_E)) {
		//huanglei add begin
		ret = wlan_generic_set_ulong(priv, WID_MEMORY_ADDRESS, 0x50000104);
		ret = wlan_generic_set_ushort(priv, WID_MEMORY_ACCESS_16BIT, 0x0101);
		ret = wlan_generic_set_ulong(priv, WID_MEMORY_ADDRESS, 0x50000108);
		ret = wlan_generic_set_ushort(priv, WID_MEMORY_ACCESS_16BIT, 0x0101);
		ret = wlan_generic_set_ulong(priv, WID_MEMORY_ADDRESS, 0x5000010C);
		ret = wlan_generic_set_ushort(priv, WID_MEMORY_ACCESS_16BIT, 0x0101);
		ret = wlan_generic_set_ulong(priv, WID_MEMORY_ADDRESS, 0x50000110);
		ret = wlan_generic_set_ushort(priv, WID_MEMORY_ACCESS_16BIT, 0x0101);
		//huanglei add end
	}

	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}
	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "wlan_set_txrate success >>> \n");

out:
	return ret;
}

int wlan_get_fw_ver(wlan_private * priv, u32 * fw_ver)
{
	int ret = wlan_generic_get_ulong(priv, WID_SYS_FW_VER, (u8 *) fw_ver);

	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Get FW_VER 0x%04x\n", *fw_ver);
out:
	return ret;
}

int wlan_get_mac_addr(wlan_private * priv, u8 * mac_addr)
{
	int ret = wlan_generic_get_str(priv, WID_MAC_ADDR, mac_addr, ETH_ALEN, NULL);

	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "STA MAC Address [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		    mac_addr[0], mac_addr[1], mac_addr[2],
		    mac_addr[3], mac_addr[4], mac_addr[5]);
out:
	return ret;
}

int wlan_get_bssid(wlan_private * priv, u8 * bssid)
{
	int ret = 0;

	memcpy(bssid, priv->curbssparams.bssid, ETH_ALEN);

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Get BSSID [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

	return ret;
}

int wlan_get_channel(wlan_private * priv, u8 * channel)
{
	int ret;

	ret = wlan_generic_get_uchar(priv, WID_CURRENT_CHANNEL, channel);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Get Channel %d\n", *channel);
out:
	return ret;
}

int wlan_get_rssi(wlan_private *priv, u8 *rssi)
{
	int ret = 0;
	u8 gRssi = 0;

	ret = wlan_generic_get_uchar(priv, WID_RSSI, &gRssi);
	if(!ret)
		wlan_update_aver_rssi(priv, priv->curbssparams.bssid, gRssi);

	gRssi = (u8)wlan_get_aver_rssi(priv, priv->curbssparams.bssid);
	*rssi = gRssi;
	return ret;
}


int wlan_set_mac_addr(wlan_private * priv, u8 * mac_addr)
{
	int ret;

	ret = wlan_generic_set_str(priv, WID_MAC_ADDR, mac_addr, ETH_ALEN);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set STA MAC Address [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		    mac_addr[0], mac_addr[1], mac_addr[2],
		    mac_addr[3], mac_addr[4], mac_addr[5]);
out:
	return ret;
}

int wlan_set_preamble(wlan_private * priv, u8 preamble)
{
	int ret;

	ret = wlan_generic_set_uchar(priv, WID_PREAMBLE, preamble);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_set_preamble \n");
out:
	return ret;
}

int wlan_set_scan_complete(wlan_private * priv)
{
	int ret;

	ret = wlan_generic_set_uchar(priv, WID_NETWORK_INFO_EN, 0);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

out:
	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "wlan_set_scan_complete  ret=%d \n", ret);
	return ret;
}

int wlan_set_ssid(wlan_private * priv, u8 * ssid, u8 ssid_len)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set SSID: %s, len = %d\n", ssid, ssid_len);

	ret = wlan_generic_set_str(priv, WID_SSID, ssid, ssid_len);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set SSID Done\n");

out:
	return ret;
}

int wlan_get_ssid(wlan_private * priv, u8 * ssid, u8 * ssid_len)
{
	int ret;
	u32 len = 0;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Get SSID \n");

	ret = wlan_generic_get_str(priv, WID_SSID, ssid, 32, &len);
	*ssid_len = len;
	if (*ssid_len > 0)
		ssid[*ssid_len] = '\0';

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Get SSID Done len:%d %s\n", *ssid_len,
		    (*ssid_len > 1) ? ssid : (u8 *) "NULL");

	return ret;
}

int wlan_set_bssid(wlan_private * priv, u8 * bssid)
{
	int ret;

	ret = wlan_generic_set_str(priv, WID_BSSID, bssid, ETH_ALEN);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set BSSID [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
out:
	return ret;
}

int wlan_disconnect(wlan_private * priv)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_disconnect \n");

	wlan_remove_tx_data_queue(priv);
	ret = wlan_generic_set_uchar(priv, WID_DISCONNECT, 0);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "wlan_disconnect  Done\n");

out:
	return ret;
}

int wlan_set_imode(wlan_private * priv, u8 imode)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set IMode 0x%02x\n", imode);

	ret = wlan_generic_set_uchar(priv, WID_802_11I_MODE, imode);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set IMode Done\n");

out:
	return ret;
}

int wlan_set_authtype(wlan_private * priv, u8 authtype)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set AuthType 0x%02x\n", authtype);

	ret = wlan_generic_set_uchar(priv, WID_AUTH_TYPE, authtype);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set AuthType Done\n");

out:
	return ret;
}

int wlan_set_listen_interval(wlan_private * priv, u8 interval)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set wlan_set_listen_interval 0x%02x\n", interval);

	ret = wlan_generic_set_uchar(priv, WID_LISTEN_INTERVAL, interval);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set wlan_set_listen_interval Done\n");
out:
	return ret;
}

int wlan_set_link_loss_threshold(wlan_private * priv, u8 threshold)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set wlan_set_link_loss_threshold 0x%02x\n", threshold);

	ret = wlan_generic_set_uchar(priv, WID_LINK_LOSS_THRESHOLD, threshold);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set wlan_set_link_loss_threshold Done\n");

out:
	return ret;
}

int wlan_set_power_save(wlan_private * priv)
{
	int ret = 0;
	if (priv->version == WLAN_VERSION_91 || priv->version == WLAN_VERSION_91_E|| priv->version == WLAN_VERSION_91_F) {
		ret = wlan_generic_set_uchar(priv, WID_POWER_SAVE, 0x30);
		if(ret){
			WLAN_ERRP("wlan_generic_set_uchar WID_POWER_SAVE failed! \n");
		}
	}
	return ret;

}

int wlan_set_wepkey(wlan_private * priv, u16 index, u8 * key, u8 key_len)
{
	int ret;
	u8 key_str[26 + 1];	// plus 1 for debug print
	u8 key_str_len;

	if (key_len == KEY_LEN_WEP_40) {
		sprintf(key_str, "%02x%02x%02x%02x%02x\n",
			key[0], key[1], key[2], key[3], key[4]);
		key_str_len = 10;
		key_str[key_str_len] = '\0';
	} else if (key_len == KEY_LEN_WEP_104) {
		sprintf(key_str, "%02x%02x%02x%02x%02x"
			"%02x%02x%02x%02x%02x"
			"%02x%02x%02x\n",
			key[0], key[1], key[2], key[3], key[4],
			key[5], key[6], key[7], key[8], key[9],
			key[10], key[11], key[12]);
		key_str_len = 26;
		key_str[key_str_len] = '\0';
	} else {
		WLAN_ERRP("Error in WEP Key length %d\n", key_len);
		ret = -EINVAL;
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set WEP KEY[%d]: %s\n", index, key_str);

	ret = wlan_generic_set_str(priv,
				   (WID_WEP_KEY_VALUE0 + index), key_str,
				   key_str_len);
	if (ret) {
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set WEP KEY[%d] Done\n", index);

out:
	return ret;
}

static void dump_key(u8 * key, u8 key_len)
{
	WLAN_DBGP("%02x %02x %02x %02x	%02x %02x %02x %02x\n",
		  key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
	WLAN_DBGP("%02x %02x %02x %02x	%02x %02x %02x %02x\n", 
		key[8], key[9], key[10], key[11], key[12], key[13], key[14], key[15]);
	if (key_len > 16)
		WLAN_DBGP("%02x %02x %02x %02x	%02x %02x %02x %02x\n",
			  key[16], key[17], key[18], key[19], key[20], key[21], key[22], key[23]);
	if (key_len > 24)
		WLAN_DBGP("%02x %02x %02x %02x	%02x %02x %02x %02x\n",
			  key[24], key[25], key[26], key[27], key[28], key[29], key[30], key[31]);
}

int wlan_set_ptk(wlan_private * priv, u8 * key, u8 key_len)
{
	int ret;
	u8 key_str[32 + ETH_ALEN + 1];
	u8 key_str_len = key_len + ETH_ALEN + 1;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set PTK: len = %d\n", key_len);

	if (WLAN_DBGLA(WLAN_DA_WID, WLAN_DL_VERB))
		dump_key(key, key_len);

	if (priv->connect_status != MAC_CONNECTED) {
		WLAN_ERRP("Adding PTK while not connected\n");
		ret = -EINVAL;
		goto out;
	}

	/*----------------------------------------*/
	/*        STA Addr      | KeyLength |   Key       */
	/*----------------------------------------*/
	/*               6              |         1     |  KeyLength  */
	/*----------------------------------------*/

	/*---------------------------------------------------------*/
	/*                                              key                                                        */
	/*---------------------------------------------------------*/
	/* Temporal Key    | Rx Micheal Key    |   Tx Micheal Key  */
	/*---------------------------------------------------------*/
	/*        16 bytes         |      8 bytes          |       8 bytes         */
	/*---------------------------------------------------------*/

	memcpy(key_str, priv->curbssparams.bssid, ETH_ALEN);
	key_str[6] = key_len;
	memcpy(key_str + 7, key, 16);

	/* swap TX MIC and RX MIC, wlan need RX MIC to be ahead */
	if (key_len > 16) {
		memcpy(key_str + 7 + 16, key + 24, 8);
		memcpy(key_str + 7 + 24, key + 16, 8);
	}

	if (priv->is_wapi)
		ret = wlan_generic_set_str(priv,
					   WID_ADD_WAPI_PTK, key_str,
					   key_str_len);
	else
		ret = wlan_generic_set_str(priv,
					   WID_ADD_PTK, key_str, key_str_len);
	if (ret) {
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set PTK Done\n");

out:
	return ret;
}

int wlan_set_gtk(wlan_private * priv, u8 key_id,
		 u8 * key_rsc, u8 key_rsc_len, u8 * key, u8 key_len)
{
	int ret;
	u8 key_str[32 + ETH_ALEN + 8 + 2];
	u8 key_str_len = key_len + ETH_ALEN + 8 + 2;

	/*---------------------------------------------------------*/
	/*        STA Addr      | KeyRSC | KeyID | KeyLength |   Key       */
	/*---------------------------------------------------------*/
	/*               6              |       8        |       1       |         1     |      KeyLength  */
	/*---------------------------------------------------------*/

	/*-------------------------------------*/
	/*                                              key                */
	/*-------------------------------------*/
	/* Temporal Key    | Rx Micheal Key    */
	/*-------------------------------------*/
	/*        16 bytes         |      8 bytes          */
	/*-------------------------------------*/

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set GTK: len = %d\n", key_len);
	if (WLAN_DBGLA(WLAN_DA_WID, WLAN_DL_VERB))
		dump_key(key, key_len);

	if (priv->connect_status != MAC_CONNECTED) {
		WLAN_ERRP("Adding GTK while not connected\n");
		ret = -EINVAL;
		goto out;
	}

	memcpy(key_str, priv->curbssparams.bssid, ETH_ALEN);
	memcpy(key_str + 6, key_rsc, key_rsc_len);
	key_str[14] = key_id;
	key_str[15] = key_len;
	memcpy(key_str + 16, key, 16);

	/* swap TX MIC and RX MIC, wlan need RX MIC to be ahead */
	if (key_len > 16) {
		//memcpy(key_str + 16 + 16, key + 16, key_len - 16);
		memcpy(key_str + 16 + 16, key + 24, 8);
		memcpy(key_str + 16 + 24, key + 16, 8);
	}

	if (priv->is_wapi)
		ret = wlan_generic_set_str(priv,
					   WID_ADD_WAPI_RX_GTK, key_str,
					   key_str_len);
	else
		ret = wlan_generic_set_str(priv,
					   WID_ADD_RX_GTK, key_str,
					   key_str_len);
	if (ret) {
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set GTK Done\n");
out:
	return ret;
}

int wlan_set_pm_mode(wlan_private * priv, u8 pm_mode)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set PM Mode 0x%02x\n", pm_mode);

	ret = wlan_generic_set_uchar(priv, WID_POWER_MANAGEMENT, pm_mode);
	if (ret) {
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set PM Mode Done\n");

out:
	return ret;
}

int wlan_set_preasso_sleep(wlan_private * priv, u32 preasso_sleep)
{
	int ret;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE,
		    "Set Preasso Sleep 0x%08x\n", preasso_sleep);

	ret = wlan_generic_set_ulong(priv, WID_PREASSO_SLEEP, preasso_sleep);
	if (ret) {
		WLAN_ERRP("failed \n");
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "Set Preasso Sleep Done\n");

out:
	return ret;
}

int rda5890_set_preamble(wlan_private * priv, unsigned char preamble)
{
	int ret;

	ret = wlan_generic_set_uchar(priv, WID_PREAMBLE, preamble);
	if (ret) {
		goto out;
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "rda5890_set_preamble \n");
out:
	return ret;
}

int wlan_set_pta(wlan_private * priv, struct pta_param_s* param)
{
	int ret;
	
	ret = wlan_generic_set_bin(priv, WID_PTA_PARAMETER, (u8*)param, sizeof(struct pta_param_s));
	if (ret) {
		WLAN_ERRP("set WID_PTA_PARAMETER failed \n");
	}
	
	return ret;
}
