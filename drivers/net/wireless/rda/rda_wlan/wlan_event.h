#ifndef __WLAN_EVENT_H__
#define __WLAN_EVENT_H__

typedef enum _wlan_event_type
{
	WLAN_EVENT_SCAN_RESULT_TIMEOUT = 0,
	WLAN_EVENT_START_ASSOC = 1,
	WLAN_EVENT_ASSOC_TIMEOUT = 2,
	WLAN_EVENT_REASSOC_TIMEOUT = 3,
	WLAN_EVENT_CARD_TO_SLEEP = 4, 
	WLAN_EVENT_CARD_CONTROL_INIT = 5,
	WLAN_EVENT_CHECK_SDIO = 6,
	WLAN_EVENT_START_NETIF = 7,
	WLAN_EVENT_SET_PHY_ERR_INT = 8,
	WLAN_EVENT_START_SCAN = 9,
}wlan_event_type;

typedef struct _wlan_event
{
	struct list_head   list;
	wlan_event_type EventType;
	void *				  Para;
}wlan_event;

void wlan_push_event(wlan_private * priv, wlan_event_type type, void* para, u8 front);
wlan_event* wlan_pull_event(wlan_private *priv);
void wlan_mac_status(wlan_private *priv, 
		char *wid_status, unsigned short wid_status_len);
int wlan_event_thread(void *data);
void wlan_timer_handler(unsigned long fcontext);
void wlan_indicate_connected(wlan_private *priv);
void wlan_indicate_disconnected(wlan_private *priv);
#endif
