#include "wlan_includes.h"

void wlan_push_event(wlan_private * priv, wlan_event_type type, void* para, u8 front)
{
	wlan_event *event = NULL;
	ENTER();

	event = kzalloc(sizeof(wlan_event), GFP_KERNEL);
    if (event)
        event->EventType = type;
	else {
		WLAN_ERRP("kzalloc wlan_event memory failed\n");
		return;
	}

	event->Para = para;

	spin_lock(&priv->EventLock);
	if(front)
		list_add(&event->list, &priv->EventQueue);
	else
		list_add_tail(&event->list, &priv->EventQueue);
	priv->EventQuNum ++;
	spin_unlock(&priv->EventLock);

		complete(&priv->EventThread.comp);
	
	LEAVE();
}

wlan_event* wlan_pull_event(wlan_private * priv)
{
	wlan_event* event;

	ENTER();
	spin_lock(&priv->EventLock);
	if (list_empty(&priv->EventQueue)) {
		spin_unlock(&priv->EventLock);
		return NULL;
	}
	event = (wlan_event*)priv->EventQueue.next;
	list_del(&event->list);
	priv->EventQuNum --;
	spin_unlock(&priv->EventLock);
	LEAVE();
	return event;
}

void wlan_timer_handler(unsigned long fcontext)
{
	PWLAN_DRV_TIMER timer = (PWLAN_DRV_TIMER) fcontext;

	if(timer->timer_function)
		timer->timer_function(timer->function_context);
	else {
		wlan_push_event((wlan_private *) timer->function_context, timer->EventType, timer->function_context, TRUE);
	}
		
	if (timer->timer_is_periodic == TRUE) {
		mod_timer(&timer->tl, jiffies + ((timer->time_period * HZ) / 1000));
	} else
		timer->timer_is_canceled = TRUE;
}


void wlan_mac_status(wlan_private *priv, 
		char *wid_status, unsigned short wid_status_len)
{
	char mac_status;

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG, "%s >>>\n", __func__);

#ifdef WLAN_FORCE_SUSPEND_SUPPORT
	if (priv->CardInSuspend == TRUE) {
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_TRACE, "%s ingnored in ap deep sleep mode.\n", __func__);
		return;
	}
#endif
	mac_status = wid_status[7];

	if (mac_status == MAC_CONNECTED) {

		
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_CRIT, "MAC CONNECTED\n");
		
		priv->connect_status = MAC_CONNECTED;
		netif_carrier_on(priv->netDev);
		netif_wake_queue(priv->netDev);

		wlan_cancel_timer(&priv->ReAssociationTimeOut);
		//report connect to supper layer
		wlan_indicate_connected(priv);	  
		{
			wlan_push_event(priv, WLAN_EVENT_SET_PHY_ERR_INT, priv, FALSE);
		}
		wake_lock_timeout(&priv->MacStatusLock, 6 * HZ);
	} else if (mac_status == MAC_DISCONNECTED) {
		WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_CRIT, "MAC_DISCONNECTED\n");
		
		wlan_cancel_timer(&priv->ReAssociationTimeOut);
		//for wep we should toggler from open to shared if auth failed
		if(priv->ToggalAssociation){
			wlan_re_assocication(priv);
		}else{
			wlan_cancel_timer(&priv->AssociationTimeOut);
			
			priv->connect_status = MAC_DISCONNECTED;
			priv->assoc_ongoing = FALSE;
			netif_stop_queue(priv->netDev);
			netif_carrier_off(priv->netDev);
			
			//report disconnect to supper layer
			if(!priv->IgnoreFisrtDisconnect){
				wlan_indicate_disconnected(priv);
				
				wake_lock_timeout(&priv->MacStatusLock, 6*HZ);
			}else{
				priv->IgnoreFisrtDisconnect = FALSE;
			}
		}		  
	} else {
		WLAN_ERRP("Invalid MAC Status 0x%02x\n", mac_status);
	}

	WLAN_DBGLAP(WLAN_DA_WID, WLAN_DL_DEBUG, "%s <<<\n", __func__);
}

void wlan_start_netif(wlan_private* priv)
{
	netif_wake_queue(priv->netDev);
}
void wlan_process_event(wlan_private * priv)
{
	int ret=0;
	wlan_event *event = NULL;

	event = wlan_pull_event(priv);
	if (!event)
		return;

	WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_TRACE, "type :%d \n", event->EventType);
	
	switch(event->EventType){
	case WLAN_EVENT_SCAN_RESULT_TIMEOUT:
		wlan_report_scan_result(priv);
		break;

	case WLAN_EVENT_START_ASSOC:
		wlan_assocication(priv);
		break;

	case WLAN_EVENT_ASSOC_TIMEOUT:
		wlan_assocication_timeout(priv);
		break;

	case WLAN_EVENT_REASSOC_TIMEOUT:
		wlan_re_assocication(priv);
		break;

	case WLAN_EVENT_CARD_TO_SLEEP:
		handle_card_to_sleep_cmd(priv);
		break;

	case WLAN_EVENT_CARD_CONTROL_INIT:
		ret=wlan_card_control_init(priv);
		if (ret) {
			WLAN_ERRP("Wlan Card Control Init Failed! \n");
		} else {
			WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_CRIT, "Wlan Card Control Init Success, Chip Version:%d \n", priv->version);
		}

		break;
	case WLAN_EVENT_CHECK_SDIO:
		wlan_card_check_sdio(priv);
		break;

	case WLAN_EVENT_START_NETIF:
		wlan_start_netif(priv);
		break;

	case WLAN_EVENT_SET_PHY_ERR_INT:
		wlan_set_phy_timeout(priv);
		break;

	case WLAN_EVENT_START_SCAN:
		wlan_set_scan_by_driver(priv);
		break;
	default:
		break;
	}
	
	kfree(event);
}


int wlan_event_thread(void *data)
{
	wlan_thread *thread = (wlan_thread *)data;
	wlan_private *priv = thread->priv;
	
	ENTER();

	wlan_activate_thread(thread);
	
	current->flags |= PF_NOFREEZE;

	while(1){
		
		wait_for_completion_interruptible(&thread->comp);

		if (kthread_should_stop()){
			WLAN_DBGLAP(WLAN_DA_MAIN, WLAN_DL_CRIT,"wlan_event_thread: break from main thread \n");
			break;
		}

		if(priv->CardRemoved){			
			break;
		}
		
				wlan_process_event(priv);
	}
	wlan_deactivate_thread(thread);

	LEAVE();
	return 0;
}

