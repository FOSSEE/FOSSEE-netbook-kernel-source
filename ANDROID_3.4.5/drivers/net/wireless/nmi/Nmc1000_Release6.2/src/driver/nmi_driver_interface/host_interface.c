#include "host_interface.h"
#include "NMI_OsWrapper/include/NMI_OSWrapper.h"
#include "itypes.h"
#include "CoreConfigurator.h"

extern NMI_Sint32 TransportInit(void);
extern NMI_Sint32 TransportDeInit(void);

/*****************************************************************************/
/*								Macros										 */
/*****************************************************************************/ 

/* Message types of the Host IF Message Queue*/
#define HOST_IF_MSG_SCAN					((NMI_Uint16)0)
#define HOST_IF_MSG_CONNECT					((NMI_Uint16)1)
#define HOST_IF_MSG_RCVD_GNRL_ASYNC_INFO	((NMI_Uint16)2)				
#define HOST_IF_MSG_KEY						((NMI_Uint16)3)
#define HOST_IF_MSG_RCVD_NTWRK_INFO			((NMI_Uint16)4)
#define HOST_IF_MSG_RCVD_SCAN_COMPLETE		((NMI_Uint16)5)
#define HOST_IF_MSG_CFG_PARAMS				((NMI_Uint16)6)
#define HOST_IF_MSG_SET_CHANNEL				((NMI_Uint16)7)
#define HOST_IF_MSG_DISCONNECT				((NMI_Uint16)8)
#define HOST_IF_MSG_GET_RSSI				((NMI_Uint16)9)
#define HOST_IF_MSG_GET_CHNL				((NMI_Uint16)10)
#define HOST_IF_MSG_ADD_BEACON				((NMI_Uint16)11)
#define HOST_IF_MSG_DEL_BEACON				((NMI_Uint16)12)
#define HOST_IF_MSG_ADD_STATION				((NMI_Uint16)13)
#define HOST_IF_MSG_DEL_STATION				((NMI_Uint16)14)
#define HOST_IF_MSG_EDIT_STATION			((NMI_Uint16)15)
#define HOST_IF_MSG_SCAN_TIMER_FIRED			((NMI_Uint16)16)
#define HOST_IF_MSG_CONNECT_TIMER_FIRED		((NMI_Uint16)17)
#define HOST_IF_MSG_POWER_MGMT				((NMI_Uint16)18)
#define HOST_IF_MSG_GET_INACTIVETIME			((NMI_Uint16)19)


#define HOST_IF_MSG_EXIT					((NMI_Uint16)100)

#define HOST_IF_SCAN_TIMEOUT		4000
#define HOST_IF_CONNECT_TIMEOUT	9500

#define BA_SESSION_DEFAULT_BUFFER_SIZE	16
#define BA_SESSION_DEFAULT_TIMEOUT		1000
/*****************************************************************************/
/*								Type Definitions							 */
/*****************************************************************************/

/*!
*  @struct 		tstrHostIFCfgParamAttr
*  @brief		Structure to hold Host IF CFG Params Attributes
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		02 April 2012
*  @version		1.0
*/
typedef struct _tstrHostIFCfgParamAttr
{
	tstrCfgParamVal  pstrCfgParamVal;

}tstrHostIFCfgParamAttr;

/*!
*  @struct 		tstrHostIFwpaAttr
*  @brief		Structure to hold Host IF Scan Attributes
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		25 March 2012
*  @version		1.0
*/
typedef struct _tstrHostIFwpaAttr
{
	NMI_Uint8* pu8key;
	const NMI_Uint8* pu8macaddr;
	NMI_Uint8* pu8seq;
	NMI_Uint8 u8seqlen;
	NMI_Uint8 u8keyidx;
	NMI_Uint8 u8Keylen;	
	NMI_Uint8 u8Ciphermode;
} tstrHostIFwpaAttr;


/*!
*  @struct 		tstrHostIFwepAttr
*  @brief		Structure to hold Host IF Scan Attributes
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		25 March 2012
*  @version		1.0
*/
typedef struct _tstrHostIFwepAttr
{
	NMI_Uint8* 	pu8WepKey;
	NMI_Uint8 	u8WepKeylen;
	NMI_Uint8	u8Wepidx;
	NMI_Uint8       u8mode;
	AUTHTYPE_T 	tenuAuth_type;
	
} tstrHostIFwepAttr;

/*!
*  @struct 		tuniHostIFkeyAttr
*  @brief		Structure to hold Host IF Scan Attributes
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		25 March 2012
*  @version		1.0
*/
typedef union _tuniHostIFkeyAttr
{
	tstrHostIFwepAttr  	strHostIFwepAttr;
	tstrHostIFwpaAttr  	strHostIFwpaAttr;
	tstrHostIFpmkidAttr strHostIFpmkidAttr;
} tuniHostIFkeyAttr;

/*!
*  @struct 		tstrHostIFkeyAttr
*  @brief		Structure to hold Host IF Scan Attributes
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		25 March 2012
*  @version		1.0
*/
typedef struct _tstrHostIFkeyAttr
{
	tenuKeyType 		enuKeyType;
	NMI_Uint8 			u8KeyAction;
	tuniHostIFkeyAttr	uniHostIFkeyAttr;
} tstrHostIFkeyAttr;


/*!
*  @struct 		tstrHostIFscanAttr
*  @brief		Structure to hold Host IF Scan Attributes
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef struct _tstrHostIFscanAttr
{
	NMI_Uint8 u8ScanSource;
	NMI_Uint8 u8ScanType;
	NMI_Uint8* pu8ChnlFreqList;
	NMI_Uint8 u8ChnlListLen;
	NMI_Uint8* pu8IEs;
	size_t IEsLen;
	tNMIpfScanResult pfScanResult;
	void* pvUserArg;
	/*BugID_4189*/
	tstrHiddenNetwork   	strHiddenNetwork;
	
} tstrHostIFscanAttr;

/*!
*  @struct 		tstrHostIFconnectAttr
*  @brief		Structure to hold Host IF Connect Attributes
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef struct _tstrHostIFconnectAttr
{
	NMI_Uint8* pu8bssid;
	NMI_Uint8* pu8ssid;
	size_t ssidLen;
	NMI_Uint8* pu8IEs;
	size_t IEsLen;
	NMI_Uint8 u8security;
	tNMIpfConnectResult pfConnectResult;
	void* pvUserArg;
	AUTHTYPE_T tenuAuth_type;
	NMI_Uint8 u8channel;
} tstrHostIFconnectAttr;

/*!
*  @struct 		tstrRcvdGnrlAsyncInfo
*  @brief		Structure to hold Received General Asynchronous info
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef struct _tstrRcvdGnrlAsyncInfo
{
	NMI_Uint8* pu8Buffer;
	NMI_Uint32 u32Length;
} tstrRcvdGnrlAsyncInfo;

/*!
*  @struct 		tstrHostIFSetChan
*  @brief		Set Channel  message body
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		25 March 2012
*  @version		1.0
*/
typedef struct _tstrHostIFSetChan
{
	NMI_Uint8	u8SetChan;	
} tstrHostIFSetChan;

/*!
*  @struct 		tstrHostIFSetChan
*  @brief		Get Channel  message body
*  @details
*  @todo
*  @sa
*  @author		Mai Daftedar
*  @date		01 Jule 2012
*  @version		1.0
*/
typedef struct _tstrHostIFGetChan
{
	NMI_Uint8	u8GetChan;
} tstrHostIFGetChan;

/*bug3819: Add Scan acomplete notification to host*/
/*!
*  @struct 		tstrScanComplete
*  @brief			hold received Async. Scan Complete message body
*  @details		
*  @todo		
*  @sa			
*  @author		zsalah
*  @date		25 March 2012
*  @version		1.0 
*/
/*typedef struct _tstrScanComplete
{
	NMI_Uint8* pu8Buffer;
	NMI_Uint32 u32Length;	
} tstrScanComplete;*/

/*!
*  @struct 		tstrHostIFSetBeacon
*  @brief		Set Beacon  message body
*  @details
*  @todo
*  @sa
*  @author		Adham Abozaeid
*  @date		10 July 2012
*  @version		1.0
*/
typedef struct _tstrHostIFSetBeacon
{
	NMI_Uint32 u32Interval;			/*!< Beacon Interval. Period between two successive beacons on air  */
	 NMI_Uint32 u32DTIMPeriod;		/*!< DTIM Period. Indicates how many Beacon frames
 										(including the current frame) appear before the next DTIM		*/
	 NMI_Uint32 u32HeadLen;			/*!< Length of the head buffer in bytes		*/
	 NMI_Uint8* pu8Head;			/*!< Pointer to the beacon's head buffer. Beacon's head	is the part
	 							from the beacon's start till the TIM element, NOT including the TIM		*/
	 NMI_Uint32 u32TailLen;			/*!< Length of the tail buffer in bytes	*/
	 NMI_Uint8* pu8Tail;			/*!< Pointer to the beacon's tail buffer. Beacon's tail	starts just
	 									after the TIM inormation element	*/
 }tstrHostIFSetBeacon;	



/*!
*  @struct 		tstrHostIFDelBeacon
*  @brief		Del Beacon  message body
*  @details
*  @todo
*  @sa
*  @author		Adham Abozaeid
*  @date		15 July 2012
*  @version		1.0
*/
typedef struct _tstrHostIFDelBeacon
{
	NMI_Uint8	u8dummy;
 }tstrHostIFDelBeacon;
/*!
*  @struct 		tstrHostIFDelSta
*  @brief		Delete station message body
*  @details		
*  @todo		
*  @sa			
*  @author		Adham Abozaeid
*  @date		15 July 2012
*  @version		1.0 Description
*/

typedef struct 
{
	NMI_Uint8 au8MacAddr[ETH_ALEN];
}tstrHostIFDelSta;

/*!
*  @struct 		tstrTimerCb
*  @brief		Timer callback message body
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef struct _tstrTimerCb
{
	void*	pvUsrArg;		/*!< Private data passed at timer start */
} tstrTimerCb;

/*!
*  @struct 	tstrHostIfPowerMgmtParam
*  @brief		Power management message body
*  @details		
*  @todo		
*  @sa			
*  @author		Adham Abozaeid
*  @date		24 November 2012
*  @version		1.0 
*/
typedef struct
{

	NMI_Bool bIsEnabled;
	NMI_Uint32 u32Timeout;
}tstrHostIfPowerMgmtParam;


/*!
*  @struct 	tstrHostIfStaInactiveT
*  @brief		Get station message body
*  @details		
*  @todo		
*  @sa			
*  @author	    Mai Daftedar
*  @date		16 April 2013
*  @version		1.0 
*/
typedef struct
{
	NMI_Uint8 mac[6];

}tstrHostIfStaInactiveT;
/**/
/*!
*  @union 		tuniHostIFmsgBody
*  @brief		Message body for the Host Interface message_q
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef union _tuniHostIFmsgBody
{
	tstrHostIFscanAttr		strHostIFscanAttr;		/*!< Host IF Scan Request Attributes message body */
	tstrHostIFconnectAttr	strHostIFconnectAttr;	/*!< Host IF Connect Request Attributes message body */
	tstrRcvdNetworkInfo 	strRcvdNetworkInfo;		/*!< Received Asynchronous Network Info message body */
	tstrRcvdGnrlAsyncInfo	strRcvdGnrlAsyncInfo;	/*!< Received General Asynchronous Info message body */
	tstrHostIFkeyAttr		strHostIFkeyAttr;		/*!<>*/
	tstrHostIFCfgParamAttr		strHostIFCfgParamAttr;   /*! <CFG Parameter message Body> */
	tstrHostIFSetChan		strHostIFSetChan;
	tstrHostIFGetChan		strHostIFGetChan;
	tstrHostIFSetBeacon	strHostIFSetBeacon;		/*!< Set beacon message body */
	tstrHostIFDelBeacon	strHostIFDelBeacon;		/*!< Del beacon message body */
	tstrNMI_AddStaParam	strAddStaParam;			/*!< Add station message body */
	tstrHostIFDelSta		strDelStaParam;			/*!< Del Station message body */
	tstrNMI_AddStaParam	strEditStaParam;			/*!< Edit station message body */
	//tstrScanComplete		strScanComplete;		/*Received Async. Scan Complete message body*/
	tstrTimerCb			strTimerCb;				/*!< Timer callback message body */
	tstrHostIfPowerMgmtParam strPowerMgmtparam; 	/*!< Power Management message body */
	tstrHostIfStaInactiveT  strHostIfStaInactiveT;
	
} tuniHostIFmsgBody;

/*!
*  @struct 		tstrHostIFmsg
*  @brief		Host Interface message
*  @details		
*  @todo		
*  @sa			
*  @author		Mostafa Abu Bakr
*  @date		25 March 2012
*  @version		1.0 
*/
typedef struct _tstrHostIFmsg
{
	NMI_Uint16 u16MsgId;						/*!< Message ID */
	tuniHostIFmsgBody uniHostIFmsgBody;		/*!< Message body */
}tstrHostIFmsg;

#ifdef CONNECT_DIRECT
typedef struct _tstrWidJoinReqExt
{
	NMI_Char   SSID[MAX_SSID_LEN];
	NMI_Uint8   u8channel;
	NMI_Uint8   BSSID[6];
}tstrWidJoinReqExt;
#endif

/*Bug4218: Parsing Join Param*/
#ifdef NMI_PARSE_SCAN_IN_HOST
/*Struct containg joinParam of each AP*/
typedef struct _tstrJoinBssParam
{	
	BSSTYPE_T	bss_type;
  	NMI_Uint8   	dtim_period;
   	NMI_Uint16   	beacon_period;
   	NMI_Uint16   	cap_info;
	NMI_Uint8 	au8bssid[6];
	NMI_Char     	ssid[MAX_SSID_LEN];
	NMI_Uint8	ssidLen;
    	NMI_Uint8    	supp_rates[MAX_RATES_SUPPORTED + 1];
	NMI_Uint8	ht_capable;
	NMI_Uint8	wmm_cap;
	NMI_Uint8	uapsd_cap;
	NMI_Bool	rsn_found;
	NMI_Uint8    rsn_grp_policy;
	NMI_Uint8    mode_802_11i;
	NMI_Uint8    rsn_pcip_policy[3];
	NMI_Uint8    rsn_auth_policy[3];
	NMI_Uint8    rsn_cap[2];
	struct _tstrJoinParam* nextJoinBss;
}tstrJoinBssParam;
/*Bug4218: Parsing Join Param*/
/*a linked list table containing needed join parameters entries for each AP found in most recent scan*/
typedef struct _tstrBssTable
{
	NMI_Uint8 u8noBssEntries;
	tstrJoinBssParam* head;
	tstrJoinBssParam* tail;
}tstrBssTable;
#endif /*NMI_PARSE_SCAN_IN_HOST*/

typedef enum
{
	SCAN_TIMER = 0,
	CONNECT_TIMER 	= 1,
       SCAN_CONNECT_TIMER_FORCE_32BIT = 0xFFFFFFFF
}tenuScanConnTimer;

/*****************************************************************************/
/*																			 */
/*							Global Variabls	 								 */
/*																			 */
/*****************************************************************************/

NMI_SemaphoreHandle gtOsCfgValuesSem;

tstrNMI_WFIDrv* gWFiDrvHandle = NMI_NULL;

static NMI_TimerHandle hScanTimer;
static NMI_TimerHandle hConnectTimer;
//static NMI_SemaphoreHandle timer_synch_sem;
//static NMI_SemaphoreHandle hSemTimerThrdEnd;
//static NMI_ThreadHandle TimerThreadHandler;
//static NMI_Bool bTerminate_TimerThread = NMI_FALSE;

static NMI_ThreadHandle HostIFthreadHandler;
static NMI_MsgQueueHandle gMsgQHostIF;
static NMI_SemaphoreHandle hSemHostIFthrdEnd;

static NMI_SemaphoreHandle hSemTestKeyBlock;
static NMI_SemaphoreHandle hSemTestDisconnectBlock;
static NMI_SemaphoreHandle hSemGetRSSI;
static NMI_SemaphoreHandle hSemGetCHNL;
static NMI_SemaphoreHandle hSemInactiveTime;

#ifndef CONNECT_DIRECT
static NMI_Uint8 gapu8RcvdSurveyResults[2][MAX_SURVEY_RESULT_FRAG_SIZE];
#endif

static NMI_Uint8 gapu8RcvdAssocResp[MAX_ASSOC_RESP_FRAME_SIZE];

NMI_Bool gbScanWhileConnected = NMI_FALSE;

static NMI_Sint8 gs8Rssi = 0;
static NMI_Uint8 gu8Chnl = 0;
#ifdef NMI_AP_EXTERNAL_MLME
static NMI_Uint32 gu32InactiveTime = 0;
#endif
#ifndef SIMULATION
static NMI_Uint32 gu32WidConnRstHack = 0;
#endif

/*Bug4218: Parsing Join Param*/
#ifdef NMI_PARSE_SCAN_IN_HOST	
/*Table containing needed join parameters for each AP found in most recent scan*/
static tstrBssTable gtstrBssTable;
/*Bug4218: Parsing Join Param*/
static void host_int_addJoinBssParam(tstrNetworkInfo* ptstrNetworkInfo);
/*Bug4218: Parsing Join Param*/
static tstrJoinBssParam* host_int_getJoinBssParam(NMI_Uint8* ssid,NMI_Uint8 ssidLen);
#endif /*NMI_PARSE_SCAN_IN_HOST*/

extern void chip_sleep_manually(NMI_Uint32 u32SleepTime);

/**
*  @brief Handle_SetChannel
*  @details 	Sending config packet to firmware to set channel
*  @param[in]   tstrHostIFSetChan* pstrHostIFSetChan
*  @return 	Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_SetChannel(tstrHostIFSetChan* pstrHostIFSetChan)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID	strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	/*prepare configuration packet*/
	strWID.u16WIDid = (NMI_Uint16)WID_CURRENT_CHANNEL;
	strWID.enuWIDtype= WID_CHAR;
	strWID.ps8WidVal = (NMI_Char*)&(pstrHostIFSetChan->u8SetChan);
	strWID.s32ValueSize = sizeof(NMI_Char);

	PRINT_D(HOSTINF_DBG,"Setting channel\n");
	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error == NMI_SUCCESS)
	{
		PRINT_ER("Failed to set channel\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
		
	}
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;



}

/**
*  @brief Handle_CfgParam
*  @details 	Sending config packet to firmware to set CFG params 
*  @param[in]   tstrHostIFCfgParamAttr* strHostIFCfgParamAttr
*  @return 	Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_CfgParam(tstrHostIFCfgParamAttr* strHostIFCfgParamAttr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID	 strWIDList[32];
	NMI_Uint8 u8WidCnt= 0;
	NMI_SemaphoreAcquire(&gtOsCfgValuesSem,NULL);


	PRINT_D(HOSTINF_DBG,"Setting CFG params\n");

	if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & BSS_TYPE)
	{
			/*----------------------------------------------------------*/
			/*Input Value:	INFRASTRUCTURE = 1,							*/
			/*				INDEPENDENT= 2,								*/
			/*				ANY_BSS= 3									*/
			/*----------------------------------------------------------*/
			  //validate input then copy>> need to check value 4 and 5
		if(strHostIFCfgParamAttr->pstrCfgParamVal.bss_type<6)
		{
			strWIDList[u8WidCnt].u16WIDid = WID_BSS_TYPE;
			strWIDList[u8WidCnt].ps8WidVal =(NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.bss_type;
			strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
			strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
			gWFiDrvHandle->strCfgValues.bss_type = (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.bss_type;
		}
		else
		{
			NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
		}
			  u8WidCnt++;
		}
		if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & AUTH_TYPE)
		  {
			/*------------------------------------------------------*/
			/*Input Values: OPEN_SYSTEM     = 0,					*/
			/*				SHARED_KEY      = 1,					*/
			/*				ANY             = 2						*/
			/*------------------------------------------------------*/
			  /*validate Possible values*/
			  if((strHostIFCfgParamAttr->pstrCfgParamVal.auth_type)==1 || (strHostIFCfgParamAttr->pstrCfgParamVal.auth_type)==2 || (strHostIFCfgParamAttr->pstrCfgParamVal.auth_type)==5)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_AUTH_TYPE;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.auth_type;
				  strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				  gWFiDrvHandle->strCfgValues.auth_type= (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.auth_type;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & AUTHEN_TIMEOUT)
		 {
			   //range is 1 to 65535.
			 if(strHostIFCfgParamAttr->pstrCfgParamVal.auth_timeout>0  && strHostIFCfgParamAttr->pstrCfgParamVal.auth_timeout<65536)
			 {
				 strWIDList[u8WidCnt].u16WIDid = WID_AUTH_TIMEOUT;
				 strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.auth_timeout;
				 strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				 strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				 gWFiDrvHandle->strCfgValues.auth_timeout = strHostIFCfgParamAttr->pstrCfgParamVal.auth_timeout;
			 }
			 else
			 {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			 u8WidCnt++;
		  }
		if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & POWER_MANAGEMENT)
		  {
			/*-----------------------------------------------------------*/
			/*Input Values:	NO_POWERSAVE     = 0,						*/
			/*				MIN_FAST_PS      = 1,						*/
			/*				MAX_FAST_PS      = 2,						*/
			/*				MIN_PSPOLL_PS    = 3,						*/
			/*				MAX_PSPOLL_PS    = 4						*/
			/*----------------------------------------------------------*/
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.power_mgmt_mode<5)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_POWER_MANAGEMENT;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.power_mgmt_mode;
				  strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				  gWFiDrvHandle->strCfgValues.power_mgmt_mode = (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.power_mgmt_mode;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & RETRY_SHORT)
		  {
			//range from 1 to 256
			if((strHostIFCfgParamAttr->pstrCfgParamVal.short_retry_limit>0) && (strHostIFCfgParamAttr->pstrCfgParamVal.short_retry_limit<256))
			{
				strWIDList[u8WidCnt].u16WIDid = WID_SHORT_RETRY_LIMIT;
				strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.short_retry_limit;
				strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				gWFiDrvHandle->strCfgValues.short_retry_limit = strHostIFCfgParamAttr->pstrCfgParamVal.short_retry_limit;
			}
			else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			 u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & RETRY_LONG)
		  {
			  //range from 1 to 256
			if((strHostIFCfgParamAttr->pstrCfgParamVal.long_retry_limit>0) && (strHostIFCfgParamAttr->pstrCfgParamVal.long_retry_limit<256))
			{
				strWIDList[u8WidCnt].u16WIDid = WID_LONG_RETRY_LIMIT;
				strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.long_retry_limit;

				strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				gWFiDrvHandle->strCfgValues.long_retry_limit = strHostIFCfgParamAttr->pstrCfgParamVal.long_retry_limit;
			}
			else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			 u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & FRAG_THRESHOLD)
		  {
			 
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.frag_threshold>255 && strHostIFCfgParamAttr->pstrCfgParamVal.frag_threshold<7937)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_FRAG_THRESHOLD;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.frag_threshold;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.frag_threshold = strHostIFCfgParamAttr->pstrCfgParamVal.frag_threshold;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & RTS_THRESHOLD)
		 {
			 //range 256 to 65535
			if(strHostIFCfgParamAttr->pstrCfgParamVal.rts_threshold >255  && strHostIFCfgParamAttr->pstrCfgParamVal.rts_threshold<65536)
			{
				strWIDList[u8WidCnt].u16WIDid = WID_RTS_THRESHOLD;
				strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.rts_threshold;
				strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				gWFiDrvHandle->strCfgValues.rts_threshold = strHostIFCfgParamAttr->pstrCfgParamVal.rts_threshold;
			}
			else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			 u8WidCnt++;
		 }
		 if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & PREAMBLE)
		  {
			 /*-----------------------------------------------------*/
			/*Input Values: Short= 0,								*/
			/*				Long= 1,                                */
			/*				Auto= 2									*/
			/*------------------------------------------------------*/
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.preamble_type< 3)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_PREAMBLE;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.preamble_type;
				  strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				  gWFiDrvHandle->strCfgValues.preamble_type = strHostIFCfgParamAttr->pstrCfgParamVal.preamble_type;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & SHORT_SLOT_ALLOWED )
		  {
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.short_slot_allowed<2)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_SHORT_SLOT_ALLOWED;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.short_slot_allowed;
				  strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				  gWFiDrvHandle->strCfgValues.short_slot_allowed = (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.short_slot_allowed;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & TXOP_PROT_DISABLE)
		 {
			/*Description:	used to Disable RTS-CTS protection for TXOP burst*/
			/*transmission when the acknowledgement policy is No-Ack or Block-Ack	*/
			/* this information is useful for external supplicant 					*/
			/*Input Values: 1 for enable and 0 for disable.							*/
			if(strHostIFCfgParamAttr->pstrCfgParamVal.txop_prot_disabled<2)
			{
				strWIDList[u8WidCnt].u16WIDid = WID_11N_TXOP_PROT_DISABLE;
				strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.txop_prot_disabled;
				strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				gWFiDrvHandle->strCfgValues.txop_prot_disabled = (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.txop_prot_disabled;
			}
			else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			 u8WidCnt++;
		 }
		 if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & BEACON_INTERVAL)
		  {
			  //range is 1 to 65535.
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.beacon_interval>0  && strHostIFCfgParamAttr->pstrCfgParamVal.beacon_interval<65536)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_BEACON_INTERVAL;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.beacon_interval;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.beacon_interval = strHostIFCfgParamAttr->pstrCfgParamVal.beacon_interval;
			  }
			  else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & DTIM_PERIOD)
		  {
			  //range is 1 to 255.
			   if(strHostIFCfgParamAttr->pstrCfgParamVal.dtim_period>0  && strHostIFCfgParamAttr->pstrCfgParamVal.dtim_period<256)
			  {
				   strWIDList[u8WidCnt].u16WIDid = WID_DTIM_PERIOD;
				   strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.dtim_period;
				   strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				   strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				   gWFiDrvHandle->strCfgValues.dtim_period = strHostIFCfgParamAttr->pstrCfgParamVal.dtim_period;
			  }
			   else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			   u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & SITE_SURVEY)
		  {
			/*----------------------------------------------------------------------*/
			/*Input Values: SITE_SURVEY_1CH    = 0, i.e.: currently set channel		*/
			/*				SITE_SURVEY_ALL_CH = 1,									*/
			/*				SITE_SURVEY_OFF    = 2									*/
			/*----------------------------------------------------------------------*/
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_enabled<3)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_SITE_SURVEY;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_enabled;
				  strWIDList[u8WidCnt].enuWIDtype= WID_CHAR;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Char);
				  gWFiDrvHandle->strCfgValues.site_survey_enabled = (NMI_Uint8)strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_enabled;
			  }
				else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		}
		if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & SITE_SURVEY_SCAN_TIME)
		  {
			  //range is 1 to 65535.
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_scan_time>0  && strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_scan_time<65536)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_SITE_SURVEY_SCAN_TIME;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_scan_time;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.site_survey_scan_time = strHostIFCfgParamAttr->pstrCfgParamVal.site_survey_scan_time;
			  }
			   else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & ACTIVE_SCANTIME)
		  {
			  //range is 1 to 65535.
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.active_scan_time>0  && strHostIFCfgParamAttr->pstrCfgParamVal.active_scan_time<65536)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_ACTIVE_SCAN_TIME;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.active_scan_time;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.active_scan_time = strHostIFCfgParamAttr->pstrCfgParamVal.active_scan_time;
			  }
			   else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & PASSIVE_SCANTIME)
		  {
			   //range is 1 to 65535.
			  if(strHostIFCfgParamAttr->pstrCfgParamVal.passive_scan_time>0  && strHostIFCfgParamAttr->pstrCfgParamVal.passive_scan_time<65536)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_PASSIVE_SCAN_TIME;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&strHostIFCfgParamAttr->pstrCfgParamVal.passive_scan_time;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.passive_scan_time = strHostIFCfgParamAttr->pstrCfgParamVal.passive_scan_time;
			  }
			   else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  if(strHostIFCfgParamAttr->pstrCfgParamVal.u32SetCfgFlag & CURRENT_TX_RATE)
		  {
			  CURRENT_TX_RATE_T curr_tx_rate = strHostIFCfgParamAttr->pstrCfgParamVal.curr_tx_rate;
			/*----------------------------------------------------------------------*/
			/*Rates:		1   2   5.5   11   6  9  12  18  24  36  48   54  Auto	*/
			/*InputValues:	1   2     3    4   5  6   7   8   9  10  11   12  0		*/
			/*----------------------------------------------------------------------*/
			  //validate rate
			  if(curr_tx_rate==AUTORATE ||curr_tx_rate==MBPS_1
					  ||curr_tx_rate==MBPS_2 || curr_tx_rate==MBPS_5_5
					  ||curr_tx_rate== MBPS_11 ||curr_tx_rate==MBPS_6
					  ||curr_tx_rate==MBPS_9 || curr_tx_rate==MBPS_12
					  ||curr_tx_rate== MBPS_18|| curr_tx_rate==MBPS_24
					  ||curr_tx_rate==MBPS_36 ||curr_tx_rate==MBPS_48 || curr_tx_rate==MBPS_54)
			  {
				  strWIDList[u8WidCnt].u16WIDid = WID_CURRENT_TX_RATE;
				  strWIDList[u8WidCnt].ps8WidVal = (NMI_Sint8*)&curr_tx_rate;
				  strWIDList[u8WidCnt].enuWIDtype= WID_SHORT;
				  strWIDList[u8WidCnt].s32ValueSize = sizeof(NMI_Uint16);
				  gWFiDrvHandle->strCfgValues.curr_tx_rate = (NMI_Uint8)curr_tx_rate;
			  }
			   else
			  {
				NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
			  }
			  u8WidCnt++;
		  }
		  s32Error = SendConfigPkt(SET_CFG, strWIDList, u8WidCnt, NMI_FALSE);

		  if(s32Error)
		  {
			  PRINT_ER("Error in setting CFG params\n");

		  }
		  NMI_CATCH(s32Error)
		  {
		  }
		  NMI_SemaphoreRelease(&gtOsCfgValuesSem, NULL);
		  return s32Error;
}

/**
*  @brief Handle_Scan
*  @details 	   Sending config packet to firmware to set the scan params 
*  @param[in]    tstrHostIFscanAttr* pstrHostIFscanAttr
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_Scan(tstrHostIFscanAttr* pstrHostIFscanAttr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWIDList[5];	
	NMI_Uint32 u32WidsCount = 0;	
	#ifdef NMI_PARSE_SCAN_IN_HOST
	tstrJoinBssParam * ptstrJoinBssParam;
	#endif
	NMI_Uint32 i;
	NMI_Uint8* pu8Buffer;
	NMI_Uint8 valuesize=0;
	
	PRINT_D(HOSTINF_DBG,"Setting SCAN params\n");
	PRINT_D(HOSTINF_DBG,"Scanning: In [%d] state \n", gWFiDrvHandle->enuHostIFstate);
	
	gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult = pstrHostIFscanAttr->pfScanResult;
	gWFiDrvHandle->strNMI_UsrScanReq.u32UserScanPvoid = pstrHostIFscanAttr->pvUserArg;

	if((gWFiDrvHandle->enuHostIFstate >= HOST_IF_SCANNING) && (gWFiDrvHandle->enuHostIFstate < HOST_IF_CONNECTED))
	{
	    /* here we either in HOST_IF_SCANNING, HOST_IF_WAITING_CONN_REQ or HOST_IF_WAITING_CONN_RESP */
		PRINT_D(GENERIC_DBG,"Don't scan we are already in [%d] state\n",gWFiDrvHandle->enuHostIFstate);
		NMI_ERRORREPORT(s32Error, NMI_BUSY);
	}
	
	PRINT_D(HOSTINF_DBG,"Setting SCAN params\n");

	gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount = 0;	
	
	/*BugID_4189*/
	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_SSID_PROBE_REQ;
	strWIDList[u32WidsCount].enuWIDtype = WID_STR;
	
	for(i=0;i<pstrHostIFscanAttr->strHiddenNetwork.u8ssidnum;i++)
	{
		valuesize+=((pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo[i].u8ssidlen)+1);
	}
	strWIDList[u32WidsCount].ps8WidVal = NMI_MALLOC(valuesize+1);
	if(strWIDList[u32WidsCount].ps8WidVal != NMI_NULL)
	{
		pu8Buffer = strWIDList[u32WidsCount].ps8WidVal;

		*pu8Buffer++ = pstrHostIFscanAttr->strHiddenNetwork.u8ssidnum;

		PRINT_D(HOSTINF_DBG,"In Handle_ProbeRequest number of ssid %d\n",pstrHostIFscanAttr->strHiddenNetwork.u8ssidnum);
		
		for(i=0;i<pstrHostIFscanAttr->strHiddenNetwork.u8ssidnum;i++)
		{
			*pu8Buffer++=pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo[i].u8ssidlen;
			NMI_memcpy(pu8Buffer, pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo[i].pu8ssid, pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo[i].u8ssidlen);
			pu8Buffer+=pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo[i].u8ssidlen;
		}
		
			
		
		strWIDList[u32WidsCount].s32ValueSize =  (NMI_Sint32)(valuesize+1);
		u32WidsCount++;
	}
	
	/*filling cfg param array*/

	//if((pstrHostIFscanAttr->pu8IEs != NULL) && (pstrHostIFscanAttr->IEsLen != 0))
	{
	/* IEs to be inserted in Probe Request */
		strWIDList[u32WidsCount].u16WIDid = WID_INFO_ELEMENT_PROBE;
		strWIDList[u32WidsCount].enuWIDtype = WID_BIN_DATA;
		strWIDList[u32WidsCount].ps8WidVal = pstrHostIFscanAttr->pu8IEs;
		strWIDList[u32WidsCount].s32ValueSize = pstrHostIFscanAttr->IEsLen;
		u32WidsCount++;
	}
	
	/*Scan Type*/
	strWIDList[u32WidsCount].u16WIDid = WID_SCAN_TYPE;
	strWIDList[u32WidsCount].enuWIDtype= WID_CHAR;
	strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
	strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(&(pstrHostIFscanAttr->u8ScanType));
	u32WidsCount++;
	
	/*list of channels to be scanned*/
	strWIDList[u32WidsCount].u16WIDid = WID_SCAN_CHANNEL_LIST;
	strWIDList[u32WidsCount].enuWIDtype = WID_BIN_DATA;

	/* Bug 4648: Convert channel numbers to start from 0 not 1. */
	if(pstrHostIFscanAttr->pu8ChnlFreqList != NULL && pstrHostIFscanAttr->u8ChnlListLen > 0) 
	{
		int i;

		for(i=0; i<pstrHostIFscanAttr->u8ChnlListLen; i++) 
		{
			if(pstrHostIFscanAttr->pu8ChnlFreqList[i] > 0) {
				pstrHostIFscanAttr->pu8ChnlFreqList[i] = pstrHostIFscanAttr->pu8ChnlFreqList[i] - 1;
			}
		}
	}

	strWIDList[u32WidsCount].ps8WidVal = pstrHostIFscanAttr->pu8ChnlFreqList;
	strWIDList[u32WidsCount].s32ValueSize = pstrHostIFscanAttr->u8ChnlListLen;
	u32WidsCount++;
	
	/*Scan Request*/ 
	strWIDList[u32WidsCount].u16WIDid = WID_START_SCAN_REQ; 
	strWIDList[u32WidsCount].enuWIDtype= WID_CHAR;
	strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
	strWIDList[u32WidsCount].ps8WidVal=(NMI_Sint8*)(&(pstrHostIFscanAttr->u8ScanSource));
	u32WidsCount++;	

	/*keep the state as is , no need to change it*/
	//gWFiDrvHandle->enuHostIFstate = HOST_IF_SCANNING;

	if(gWFiDrvHandle->enuHostIFstate == HOST_IF_CONNECTED)
	{
		gbScanWhileConnected = NMI_TRUE;				
	}
	else if(gWFiDrvHandle->enuHostIFstate == HOST_IF_IDLE)
	{
		gbScanWhileConnected = NMI_FALSE;				
	}
	/*Bug4218: Parsing Join Param*/
	//cleaning JoinTable
	#ifdef NMI_PARSE_SCAN_IN_HOST	
	while(gtstrBssTable.u8noBssEntries!=0)
	{
		//PRINT_D(HOSTINF_DBG,"Freeing Join Parameters Table\n");
		ptstrJoinBssParam = gtstrBssTable.head;
		gtstrBssTable.head = (tstrJoinBssParam*)gtstrBssTable.head->nextJoinBss;
		NMI_FREE(ptstrJoinBssParam);
		gtstrBssTable.u8noBssEntries-=1;
	}
	#endif/*NMI_PARSE_SCAN_IN_HOST*/
	s32Error = SendConfigPkt(SET_CFG, strWIDList, u32WidsCount, NMI_FALSE);
	
	if(s32Error)
	{
		PRINT_ER("Failed to send scan paramters config packet\n");
		NMI_ERRORREPORT(s32Error, s32Error);
	}
	else
	{		
		PRINT_D(HOSTINF_DBG,"Successfully sent SCAN params config packet\n");
	}
	
	NMI_CATCH(s32Error)
	{
		NMI_TimerStop(&hScanTimer, NMI_NULL);
		/*if there is an ongoing scan request*/
		Handle_ScanDone(SCAN_EVENT_ABORTED);
	}

	/* Deallocate pstrHostIFscanAttr->u8ChnlListLen which was prevoisuly allocated by the sending thread */
	if(pstrHostIFscanAttr->pu8ChnlFreqList != NULL)
	{
		NMI_FREE(pstrHostIFscanAttr->pu8ChnlFreqList);
		pstrHostIFscanAttr->pu8ChnlFreqList = NULL;
	}

	/* Deallocate pstrHostIFscanAttr->pu8IEs which was previously allocated by the sending thread */
	if(pstrHostIFscanAttr->pu8IEs != NULL)
	{
		NMI_FREE(pstrHostIFscanAttr->pu8IEs);
		pstrHostIFscanAttr->pu8IEs = NULL;
	}
	if(pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo != NULL)
	{
		NMI_FREE(pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo);
		pstrHostIFscanAttr->strHiddenNetwork.pstrHiddenNetworkInfo = NULL;
	}

	/* Deallocate pstrHostIFscanAttr->u8ChnlListLen which was prevoisuly allocated by the sending thread */
	if(pstrHostIFscanAttr->pu8ChnlFreqList != NULL)
	{
		NMI_FREE(pstrHostIFscanAttr->pu8ChnlFreqList);
		pstrHostIFscanAttr->pu8ChnlFreqList = NULL;
	}
	
	return s32Error;
}

/**
*  @brief Handle_ScanDone
*  @details 	   Call scan notification callback function
*  @param[in]    NONE
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_ScanDone(tenuScanEvent enuEvent)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;

	PRINT_D(HOSTINF_DBG,"in Handle_ScanDone()\n");	
	
	/*if there is an ongoing scan request*/
	if(gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult)
	{		
		gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult(enuEvent, NMI_NULL,
																 gWFiDrvHandle->strNMI_UsrScanReq.u32UserScanPvoid);
		/*delete current scan request*/
		gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult= NULL;		
	}	
		
	return s32Error;
}

/**
*  @brief Handle_Connect
*  @details 	   Sending config packet to firmware to starting connection
*  @param[in]    tstrHostIFconnectAttr* pstrHostIFconnectAttr
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/

static NMI_Sint32 Handle_Connect(tstrHostIFconnectAttr* pstrHostIFconnectAttr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrWID strWIDList[5];
	NMI_Uint32 u32WidsCount = 0;	
	//char passphrase[] = "12345678";
	#ifndef CONNECT_DIRECT
	NMI_Sint32 s32Err = NMI_SUCCESS;
	NMI_Uint32 i;
	NMI_Uint8 u8bssDscListIndex;
	wid_site_survey_reslts_s* pstrSurveyResults = NMI_NULL;
	#else
	NMI_Uint8* pu8CurrByte = NMI_NULL;
	/*Bug4218: Parsing Join Param*/
	#ifdef NMI_PARSE_SCAN_IN_HOST	
	tstrJoinBssParam* ptstrJoinBssParam;
	#endif/*NMI_PARSE_SCAN_IN_HOST*/
	
	#endif	
	
	PRINT_D(HOSTINF_DBG,"Handling connect request\n");	

	#ifndef CONNECT_DIRECT
	NMI_memset(gapu8RcvdSurveyResults[0], 0, MAX_SURVEY_RESULT_FRAG_SIZE);
	NMI_memset(gapu8RcvdSurveyResults[1], 0, MAX_SURVEY_RESULT_FRAG_SIZE);

	
	PRINT_D(HOSTINF_DBG,"Getting site survey results\n");
	s32Err = host_int_get_site_survey_results((NMI_WFIDrvHandle)gWFiDrvHandle,
									gapu8RcvdSurveyResults, 
									MAX_SURVEY_RESULT_FRAG_SIZE);
	if(s32Err)
	{
		PRINT_ER("Failed to get site survey results\n");
		NMI_ERRORREPORT(s32Error,NMI_FAIL);

	}
	s32Err = ParseSurveyResults(gapu8RcvdSurveyResults, &pstrSurveyResults, 
								     &gWFiDrvHandle->u32SurveyResultsCount);
		

	if(s32Err == NMI_SUCCESS)
	{								
		/* use the parsed info in pstrSurveyResults, then deallocate it */
		PRINT_D(HOSTINF_DBG,"Copying site survey results in global structure, then deallocate\n");
		for(i = 0; i < gWFiDrvHandle->u32SurveyResultsCount; i++)
		{
			NMI_memcpy(&gWFiDrvHandle->astrSurveyResults[i], &pstrSurveyResults[i],
							  sizeof(wid_site_survey_reslts_s));
		}			

		DeallocateSurveyResults(pstrSurveyResults);
	}
	else
	{
		NMI_ERRORREPORT(s32Error,NMI_FAIL);
		PRINT_ER("ParseSurveyResults() Error(%d) \n", s32Err);
	}		

	
	for(i = 0; i < gWFiDrvHandle->u32SurveyResultsCount; i++)
	{		
		if(NMI_memcmp(gWFiDrvHandle->astrSurveyResults[i].SSID, 
						pstrHostIFconnectAttr->pu8ssid, 
						pstrHostIFconnectAttr->ssidLen) == 0)
		{
			PRINT_INFO(HOSTINF_DBG,"Network with required SSID is found %s\n",pstrHostIFconnectAttr->pu8ssid);
			if(pstrHostIFconnectAttr->pu8bssid == NULL)
			{
				/* BSSID is not passed from the user, so decision of matching
				 * is done by SSID only */
				PRINT_INFO(HOSTINF_DBG,"BSSID is not passed from the user\n");
				break;
			}
			else
			{
				/* BSSID is also passed from the user, so decision of matching
				 * should consider also this passed BSSID */

				if(NMI_memcmp(gWFiDrvHandle->astrSurveyResults[i].BSSID, 
							       pstrHostIFconnectAttr->pu8bssid, 
							       6) == 0)
				{
					PRINT_INFO(HOSTINF_DBG,"BSSID is passed from the user and matched\n");
					break;
				}
			}
		}
	}			

	if(i < gWFiDrvHandle->u32SurveyResultsCount)
	{
		u8bssDscListIndex = i;		

		PRINT_INFO(HOSTINF_DBG,"Connecting to network of Bss Idx %d and SSID %s and channel %d \n",
					u8bssDscListIndex, gWFiDrvHandle->astrSurveyResults[u8bssDscListIndex].SSID,
					gWFiDrvHandle->astrSurveyResults[u8bssDscListIndex].Channel);		
		
		PRINT_INFO(HOSTINF_DBG,"Saving connection parameters in global structure\n");

		if(pstrHostIFconnectAttr->pu8bssid != NULL)
		{
			gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = (NMI_Uint8*)NMI_MALLOC(6);
			NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid, pstrHostIFconnectAttr->pu8bssid, 6);			
		}		

		gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = pstrHostIFconnectAttr->ssidLen;
		if(pstrHostIFconnectAttr->pu8ssid != NULL)
		{									
			gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = (NMI_Uint8*)NMI_MALLOC(pstrHostIFconnectAttr->ssidLen+1);
			NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid, pstrHostIFconnectAttr->pu8ssid, 
						  pstrHostIFconnectAttr->ssidLen);
			gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid[pstrHostIFconnectAttr->ssidLen] = '\0';			
		}

		gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = pstrHostIFconnectAttr->IEsLen;
		if(pstrHostIFconnectAttr->pu8IEs != NULL)
		{					
			gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = (NMI_Uint8*)NMI_MALLOC(pstrHostIFconnectAttr->IEsLen);
			NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs, pstrHostIFconnectAttr->pu8IEs, 
						  pstrHostIFconnectAttr->IEsLen);			
		}

		gWFiDrvHandle->strNMI_UsrConnReq.u8security= pstrHostIFconnectAttr->u8security;
		gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type= pstrHostIFconnectAttr->tenuAuth_type;
		gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult = pstrHostIFconnectAttr->pfConnectResult;
		gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid = pstrHostIFconnectAttr->pvUserArg;				
		
		
		//if((gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL) && 
		  // (gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen != 0))
		{
			/* IEs to be inserted in Association Request */
			strWIDList[u32WidsCount].u16WIDid = WID_INFO_ELEMENT_ASSOCIATE;
			strWIDList[u32WidsCount].enuWIDtype = WID_BIN_DATA;
			strWIDList[u32WidsCount].ps8WidVal = gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs;
			strWIDList[u32WidsCount].s32ValueSize = gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen;
			u32WidsCount++;
		}
		strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_11I_MODE;
		strWIDList[u32WidsCount].enuWIDtype = WID_CHAR;
		strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
		strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(&(gWFiDrvHandle->strNMI_UsrConnReq.u8security));
		u32WidsCount++;
		
		PRINT_INFO(HOSTINF_DBG,"Encrypt Mode = %x\n",gWFiDrvHandle->strNMI_UsrConnReq.u8security);
		
		strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_AUTH_TYPE;
		strWIDList[u32WidsCount].enuWIDtype = WID_CHAR;
		strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
		strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(&gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type);
		u32WidsCount++;
		
		PRINT_INFO(HOSTINF_DBG,"Authentication Type = %x\n",gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type);
		/*
		strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_11I_PSK;
		strWIDList[u32WidsCount].enuWIDtype = WID_STR;
		strWIDList[u32WidsCount].s32ValueSize = sizeof(passphrase);
		strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(passphrase);
		u32WidsCount++;
		*/

		strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_JOIN_REQ;
		strWIDList[u32WidsCount].enuWIDtype = WID_CHAR;
		strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
		strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)&u8bssDscListIndex;
		u32WidsCount++;		

		#ifndef SIMULATION
		/* A temporary workaround to avoid handling the misleading MAC_DISCONNECTED raised from the 
		     firmware at chip reset when processing the WIDs of the Connect Request.
		     (This workaround should be removed in the future when the Chip reset of the Connect WIDs is disabled) */
		////////////////////////
		gu32WidConnRstHack = 0;
		////////////////////////
		#endif
		
		s32Error = SendConfigPkt(SET_CFG, strWIDList, u32WidsCount, NMI_FALSE);
		if(s32Error)
		{
			PRINT_ER("Handle_Connect()] failed to send config packet\n");
			NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
		}
		else
		{
			gWFiDrvHandle->enuHostIFstate = HOST_IF_WAITING_CONN_RESP;
		}

	}
	else
	{
		PRINT_ER("Required BSSID not found\n");
		NMI_ERRORREPORT(s32Error,NMI_NOT_FOUND);
	}	
	
	#else		
	
	PRINT_INFO(HOSTINF_DBG,"Saving connection parameters in global structure\n");

	/*Bug4218: Parsing Join Param*/
	#ifdef NMI_PARSE_SCAN_IN_HOST	
	ptstrJoinBssParam = host_int_getJoinBssParam(pstrHostIFconnectAttr->pu8ssid,pstrHostIFconnectAttr->ssidLen);
	if(ptstrJoinBssParam==NULL)
	{
		PRINT_ER("Required BSSID not found\n");
		NMI_ERRORREPORT(s32Error,NMI_NOT_FOUND);
	}
	#endif /*NMI_PARSE_SCAN_IN_HOST*/
	
	if(pstrHostIFconnectAttr->pu8bssid != NULL)
	{
		gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = (NMI_Uint8*)NMI_MALLOC(6);
		NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid, pstrHostIFconnectAttr->pu8bssid, 6);			
	}		

	gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = pstrHostIFconnectAttr->ssidLen;
	if(pstrHostIFconnectAttr->pu8ssid != NULL)
	{									
		gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = (NMI_Uint8*)NMI_MALLOC(pstrHostIFconnectAttr->ssidLen+1);
		NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid, pstrHostIFconnectAttr->pu8ssid, 
					  pstrHostIFconnectAttr->ssidLen);
		gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid[pstrHostIFconnectAttr->ssidLen] = '\0';			
	}

	gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = pstrHostIFconnectAttr->IEsLen;
	if(pstrHostIFconnectAttr->pu8IEs != NULL)
	{					
		gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = (NMI_Uint8*)NMI_MALLOC(pstrHostIFconnectAttr->IEsLen);
		NMI_memcpy(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs, pstrHostIFconnectAttr->pu8IEs, 
					  pstrHostIFconnectAttr->IEsLen);			
	}

	gWFiDrvHandle->strNMI_UsrConnReq.u8security= pstrHostIFconnectAttr->u8security;
	gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type= pstrHostIFconnectAttr->tenuAuth_type;
	gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult = pstrHostIFconnectAttr->pfConnectResult;
	gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid = pstrHostIFconnectAttr->pvUserArg;				
	
	
	//if((gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL) && 
	  // (gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen != 0))
	{
		/* IEs to be inserted in Association Request */
		strWIDList[u32WidsCount].u16WIDid = WID_INFO_ELEMENT_ASSOCIATE;
		strWIDList[u32WidsCount].enuWIDtype = WID_BIN_DATA;
		strWIDList[u32WidsCount].ps8WidVal = gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs;
		strWIDList[u32WidsCount].s32ValueSize = gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen;
		u32WidsCount++;
	}
	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_11I_MODE;
	strWIDList[u32WidsCount].enuWIDtype = WID_CHAR;
	strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
	strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(&(gWFiDrvHandle->strNMI_UsrConnReq.u8security));
	u32WidsCount++;
	
	PRINT_INFO(HOSTINF_DBG,"Encrypt Mode = %x\n",gWFiDrvHandle->strNMI_UsrConnReq.u8security);

	
	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_AUTH_TYPE;
	strWIDList[u32WidsCount].enuWIDtype = WID_CHAR;
	strWIDList[u32WidsCount].s32ValueSize = sizeof(NMI_Char);
	strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(&gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type);
	u32WidsCount++;
	
	PRINT_INFO(HOSTINF_DBG,"Authentication Type = %x\n",gWFiDrvHandle->strNMI_UsrConnReq.tenuAuth_type);
	/*
	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_11I_PSK;
	strWIDList[u32WidsCount].enuWIDtype = WID_STR;
	strWIDList[u32WidsCount].s32ValueSize = sizeof(passphrase);
	strWIDList[u32WidsCount].ps8WidVal = (NMI_Sint8*)(passphrase);
	u32WidsCount++;
	*/

	PRINT_D(HOSTINF_DBG,"Connecting to network of SSID %s on channel %d\n", 
				  gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid, pstrHostIFconnectAttr->u8channel);		
	

#ifndef NMI_PARSE_SCAN_IN_HOST	
	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_JOIN_REQ_EXTENDED;
	strWIDList[u32WidsCount].enuWIDtype = WID_STR;
	strWIDList[u32WidsCount].s32ValueSize = MAX_SSID_LEN+7;
	strWIDList[u32WidsCount].ps8WidVal = NMI_MALLOC(strWIDList[u32WidsCount].s32ValueSize);

	if(strWIDList[u32WidsCount].ps8WidVal == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_NO_MEM);
	}

	pu8CurrByte = strWIDList[u32WidsCount].ps8WidVal;

	if(pstrHostIFconnectAttr->pu8ssid != NULL)
	{		
		NMI_memcpy(pu8CurrByte, pstrHostIFconnectAttr->pu8ssid, pstrHostIFconnectAttr->ssidLen);
		pu8CurrByte[pstrHostIFconnectAttr->ssidLen] = '\0';	
	}
	pu8CurrByte += MAX_SSID_LEN;
	if((pstrHostIFconnectAttr->u8channel >= 1) && (pstrHostIFconnectAttr->u8channel <= 14))
	{
		*(pu8CurrByte++) = pstrHostIFconnectAttr->u8channel;
	}
	else
	{
		PRINT_ER("Channel out of range\n");
		*(pu8CurrByte++) = 0xFF;
	}
	if(pstrHostIFconnectAttr->pu8bssid != NULL)
	{		
		NMI_memcpy(pu8CurrByte, pstrHostIFconnectAttr->pu8bssid, 6);			
	}	
	pu8CurrByte += 6;

	/* keep the buffer at the start of the allocated pointer to use it with the free*/
	pu8CurrByte = strWIDList[u32WidsCount].ps8WidVal ;

	#else

	strWIDList[u32WidsCount].u16WIDid = (NMI_Uint16)WID_JOIN_REQ_EXTENDED;
	strWIDList[u32WidsCount].enuWIDtype = WID_STR;
	strWIDList[u32WidsCount].s32ValueSize = 79;
	strWIDList[u32WidsCount].ps8WidVal = NMI_MALLOC(strWIDList[u32WidsCount].s32ValueSize);

	if(strWIDList[u32WidsCount].ps8WidVal == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_NO_MEM);
	}

	pu8CurrByte = strWIDList[u32WidsCount].ps8WidVal;


	if(pstrHostIFconnectAttr->pu8ssid != NULL)
	{		
		NMI_memcpy(pu8CurrByte, pstrHostIFconnectAttr->pu8ssid, pstrHostIFconnectAttr->ssidLen);
		pu8CurrByte[pstrHostIFconnectAttr->ssidLen] = '\0';	
	}
	pu8CurrByte += MAX_SSID_LEN;

	/* BSS type*/
	*(pu8CurrByte++) = INFRASTRUCTURE;
	/* Channel*/
	if((pstrHostIFconnectAttr->u8channel >= 1) && (pstrHostIFconnectAttr->u8channel <= 14))
	{
		*(pu8CurrByte++) = pstrHostIFconnectAttr->u8channel;
	}
	else
	{
		PRINT_ER("Channel out of range\n");
		*(pu8CurrByte++) = 0xFF;
	}
	/* Cap Info*/
	*(pu8CurrByte++)  = ( ptstrJoinBssParam->cap_info) & 0xFF;
	*(pu8CurrByte++)  = (( ptstrJoinBssParam->cap_info) >>8) & 0xFF;
	PRINT_D(HOSTINF_DBG,"* Cap Info %0x*\n",(*(pu8CurrByte-2) | ((*(pu8CurrByte-1))<<8)) );

	/* sa*/
	if(pstrHostIFconnectAttr->pu8bssid != NULL)
	{		
		NMI_memcpy(pu8CurrByte, pstrHostIFconnectAttr->pu8bssid, 6);			
	}
	pu8CurrByte += 6;

	/* bssid*/
	if(pstrHostIFconnectAttr->pu8bssid != NULL)
	{		
		NMI_memcpy(pu8CurrByte, pstrHostIFconnectAttr->pu8bssid, 6);			
	}	
	pu8CurrByte += 6;

	/* Beacon Period*/
	*(pu8CurrByte++)  = (ptstrJoinBssParam->beacon_period) & 0xFF;
	*(pu8CurrByte++)  = ((ptstrJoinBssParam->beacon_period) >>8) & 0xFF;
	PRINT_D(HOSTINF_DBG,"* Beacon Period %d*\n",(*(pu8CurrByte-2) | ((*(pu8CurrByte-1))<<8) ));
	/* DTIM Period*/
	*(pu8CurrByte++)  =  ptstrJoinBssParam->dtim_period;
	PRINT_D(HOSTINF_DBG,"* DTIM Period %d*\n",(*(pu8CurrByte-1)));
	/* Supported rates*/
	NMI_memcpy(pu8CurrByte, ptstrJoinBssParam->supp_rates, MAX_RATES_SUPPORTED+1);
	pu8CurrByte += (MAX_RATES_SUPPORTED+1);

	/* wmm cap*/
	*(pu8CurrByte++)  =  ptstrJoinBssParam->wmm_cap;
	PRINT_D(HOSTINF_DBG,"* wmm cap%d*\n",(*(pu8CurrByte-1)));
	/* uapsd cap*/
	*(pu8CurrByte++)  = ptstrJoinBssParam->uapsd_cap;

	/* ht cap*/
	*(pu8CurrByte++)  = ptstrJoinBssParam->ht_capable;
	//copy this information to the user request
	gWFiDrvHandle->strNMI_UsrConnReq.IsHTCapable = ptstrJoinBssParam->ht_capable;

	/* rsn found*/
	*(pu8CurrByte++)  =  ptstrJoinBssParam->rsn_found;
	PRINT_D(HOSTINF_DBG,"* rsn found %d*\n",*(pu8CurrByte-1));
	/* rsn group policy*/
	*(pu8CurrByte++)  =  ptstrJoinBssParam->rsn_grp_policy; 
	PRINT_D(HOSTINF_DBG,"* rsn group policy %0x*\n",(*(pu8CurrByte-1)));
	/* mode_802_11i*/
	*(pu8CurrByte++) =  ptstrJoinBssParam->mode_802_11i;
	PRINT_D(HOSTINF_DBG,"* mode_802_11i %d*\n",(*(pu8CurrByte-1)));
	/* rsn pcip policy*/
	NMI_memcpy(pu8CurrByte, ptstrJoinBssParam->rsn_pcip_policy, sizeof(ptstrJoinBssParam->rsn_pcip_policy));
	pu8CurrByte += sizeof(ptstrJoinBssParam->rsn_pcip_policy);

	/* rsn auth policy*/
	NMI_memcpy(pu8CurrByte, ptstrJoinBssParam->rsn_auth_policy, sizeof(ptstrJoinBssParam->rsn_auth_policy));
	pu8CurrByte += sizeof(ptstrJoinBssParam->rsn_auth_policy);

	/* rsn auth policy*/
	NMI_memcpy(pu8CurrByte, ptstrJoinBssParam->rsn_cap, sizeof( ptstrJoinBssParam->rsn_cap));
	pu8CurrByte += sizeof( ptstrJoinBssParam->rsn_cap);

	/* keep the buffer at the start of the allocated pointer to use it with the free*/
	pu8CurrByte = strWIDList[u32WidsCount].ps8WidVal ;
	
	
	#endif /* #ifdef NMI_PARSE_SCAN_IN_HOST*/
	u32WidsCount++;	

	#ifndef SIMULATION
	/* A temporary workaround to avoid handling the misleading MAC_DISCONNECTED raised from the 
	     firmware at chip reset when processing the WIDs of the Connect Request.
	     (This workaround should be removed in the future when the Chip reset of the Connect WIDs is disabled) */
	////////////////////////
	gu32WidConnRstHack = 0;
	////////////////////////
	#endif
	
	s32Error = SendConfigPkt(SET_CFG, strWIDList, u32WidsCount, NMI_FALSE);
	if(s32Error)
	{
		PRINT_ER("Handle_Connect()] failed to send config packet\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	else
	{
		gWFiDrvHandle->enuHostIFstate = HOST_IF_WAITING_CONN_RESP;
	}		
	#endif
	
	NMI_CATCH(s32Error)
	{
		tstrConnectInfo strConnectInfo;

		NMI_TimerStop(&hConnectTimer, NMI_NULL);
		
		PRINT_D(HOSTINF_DBG,"could not start connecting to the required network\n");

		NMI_memset(&strConnectInfo, 0, sizeof(tstrConnectInfo));

		if(pstrHostIFconnectAttr->pfConnectResult != NULL)
		{
			if(pstrHostIFconnectAttr->pu8bssid != NULL)
			{
				NMI_memcpy(strConnectInfo.au8bssid, pstrHostIFconnectAttr->pu8bssid, 6);				
			}				

			if(pstrHostIFconnectAttr->pu8IEs != NULL)
			{
				strConnectInfo.ReqIEsLen = pstrHostIFconnectAttr->IEsLen;
				strConnectInfo.pu8ReqIEs = (NMI_Uint8*)NMI_MALLOC(pstrHostIFconnectAttr->IEsLen);
				NMI_memcpy(strConnectInfo.pu8ReqIEs,
							  pstrHostIFconnectAttr->pu8IEs,
							  pstrHostIFconnectAttr->IEsLen);
			}			
		
			pstrHostIFconnectAttr->pfConnectResult(CONN_DISCONN_EVENT_CONN_RESP,
												   &strConnectInfo,
												   MAC_DISCONNECTED,
												   NULL,
												   pstrHostIFconnectAttr->pvUserArg);
			/*Change state to idle*/
			gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;
			/* Deallocation */
			if(strConnectInfo.pu8ReqIEs != NULL)
			{
				NMI_FREE(strConnectInfo.pu8ReqIEs);
				strConnectInfo.pu8ReqIEs = NULL;
			}
			
		}
		else
		{
			PRINT_ER("Connect callback function pointer is NULL \n");
		}	
	}

	PRINT_D(HOSTINF_DBG,"Deallocating connection parameters\n");
	/* Deallocate pstrHostIFconnectAttr->pu8bssid which was prevoisuly allocated by the sending thread */
	if(pstrHostIFconnectAttr->pu8bssid != NULL)
	{
		NMI_FREE(pstrHostIFconnectAttr->pu8bssid);
		pstrHostIFconnectAttr->pu8bssid = NULL;
	}

	/* Deallocate pstrHostIFconnectAttr->pu8ssid which was prevoisuly allocated by the sending thread */
	if(pstrHostIFconnectAttr->pu8ssid != NULL)
	{		
		NMI_FREE(pstrHostIFconnectAttr->pu8ssid);
		pstrHostIFconnectAttr->pu8ssid = NULL;
	}

	/* Deallocate pstrHostIFconnectAttr->pu8IEs which was prevoisuly allocated by the sending thread */
	if(pstrHostIFconnectAttr->pu8IEs != NULL)
	{		
		NMI_FREE(pstrHostIFconnectAttr->pu8IEs);
		pstrHostIFconnectAttr->pu8IEs = NULL;
	}	

	if(pu8CurrByte != NMI_NULL)
	{
		NMI_FREE(pu8CurrByte);
	}
	return s32Error;
}

/**
*  @brief 		   Handle_ConnectTimeout
*  @details 	   Call connect notification callback function indicating connection failure
*  @param[in]    NONE
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_ConnectTimeout(void)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrConnectInfo strConnectInfo;	
	tstrWID strWID;	
	NMI_Uint16 u16DummyReasonCode = 0;
	
	gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;

	gbScanWhileConnected = NMI_FALSE;			

	
	NMI_memset(&strConnectInfo, 0, sizeof(tstrConnectInfo));	

	/* First, we will notify the upper layer with the Connection failure {through the Connect Callback function}, 
	     then we will notify our firmware also with the Connection failure {through sending to it Cfg packet carrying 
	     WID_DISCONNECT} */	
	if(gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult != NULL)
	{
		if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
		{
			NMI_memcpy(strConnectInfo.au8bssid, 
						  gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid, 6);				
		}				

		if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
		{
			strConnectInfo.ReqIEsLen = gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen;
			strConnectInfo.pu8ReqIEs = (NMI_Uint8*)NMI_MALLOC(gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen);
			NMI_memcpy(strConnectInfo.pu8ReqIEs,
						  gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs,
						  gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen);
		}		
		
		gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult(CONN_DISCONN_EVENT_CONN_RESP,
														      &strConnectInfo,
														      MAC_DISCONNECTED,
														      NULL,
														      gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid);		
		
		/* Deallocation of strConnectInfo.pu8ReqIEs */
		if(strConnectInfo.pu8ReqIEs != NULL)
		{
			NMI_FREE(strConnectInfo.pu8ReqIEs);
			strConnectInfo.pu8ReqIEs = NULL;
		}
	}
	else
	{
		PRINT_ER("Connect callback function pointer is NULL \n");		
	}				

	/* Here we will notify our firmware also with the Connection failure {through sending to it Cfg packet carrying 
	     WID_DISCONNECT} */
	strWID.u16WIDid = (NMI_Uint16)WID_DISCONNECT;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8*)&u16DummyReasonCode;
	strWID.s32ValueSize = sizeof(NMI_Char);

	PRINT_D(HOSTINF_DBG,"Sending disconnect request\n");

	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);	
	if(s32Error)
	{
		PRINT_ER("Failed to send dissconect config packet\n");		
	}

	/* Deallocation of the Saved Connect Request in the global Handle */	
	gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = 0;
	if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid != NULL)
	{
		NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid);
		gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = NULL;
	}

	if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
	{
		NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid);
		gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = NULL;
	}

	gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = 0;
	if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
	{
		NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs);
		gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = NULL;
	}		
	
	return s32Error;
}

/**
*  @brief Handle_RcvdNtwrkInfo
*  @details 	   Handling received network information 
*  @param[in]    tstrRcvdNetworkInfo* pstrRcvdNetworkInfo
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_RcvdNtwrkInfo(tstrRcvdNetworkInfo* pstrRcvdNetworkInfo)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNetworkInfo * pstrNetworkInfo = NULL;	
	NMI_Uint32 i;

	PRINT_INFO(HOSTINF_DBG,"Handling received network info\n");	

	/*if there is a an ongoing scan request*/
	if(gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult)
	{
		PRINT_D(HOSTINF_DBG,"State: Scanning, parsing network information received\n");
		ParseNetworkInfo(pstrRcvdNetworkInfo->pu8Buffer, &pstrNetworkInfo);
		if((pstrNetworkInfo == NULL) 
		    ||(gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult == NMI_NULL))
		{
			NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
		}		
		
		/* check whether this network is discovered before */
		for(i = 0; i < gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount; i++)
		{

			if((gWFiDrvHandle->strNMI_UsrScanReq.astrFoundNetworkInfo[i].au8bssid != NULL) &&
			   (pstrNetworkInfo->au8bssid != NULL))
			{
				if(NMI_memcmp(gWFiDrvHandle->strNMI_UsrScanReq.astrFoundNetworkInfo[i].au8bssid,
								  pstrNetworkInfo->au8bssid, 6) == 0)
				{
					/* network is already discovered before */
					PRINT_D(HOSTINF_DBG,"Network previously discovered\n");
					break;
				}
			}
		}

		if(i == gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount)
		{
			/* here it is confirmed that it is a new discovered network,
			 * so add its record then call the User CallBack function */

			PRINT_D(HOSTINF_DBG,"New network found\n");

			if(gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount <= MAX_NUM_SCANNED_NETWORKS)
			{
				if((gWFiDrvHandle->strNMI_UsrScanReq.astrFoundNetworkInfo[gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount].au8bssid != NULL)
					&& (pstrNetworkInfo->au8bssid != NULL))
				{
					NMI_memcpy(gWFiDrvHandle->strNMI_UsrScanReq.astrFoundNetworkInfo[gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount].au8bssid,
							   pstrNetworkInfo->au8bssid, 6);

					gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount++;

					gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult(SCAN_EVENT_NETWORK_FOUND, pstrNetworkInfo,
																		  gWFiDrvHandle->strNMI_UsrScanReq.u32UserScanPvoid);					
				/*Bug4218: Parsing Join Param*/
				//add new BSS to JoinBssTable
				#ifdef NMI_PARSE_SCAN_IN_HOST	
				host_int_addJoinBssParam(pstrNetworkInfo);
				#endif/*NMI_PARSE_SCAN_IN_HOST*/
				}
			}
			else
			{
				PRINT_WRN(HOSTINF_DBG,"Discovered networks exceeded max. limit \n");
			}
		}
	}
	

	NMI_CATCH(s32Error)
	{

	}

	/* Deallocate pstrRcvdNetworkInfo->pu8Buffer which was prevoisuly allocated by the sending thread */
	if(pstrRcvdNetworkInfo->pu8Buffer != NULL)
	{
		NMI_FREE(pstrRcvdNetworkInfo->pu8Buffer);
		pstrRcvdNetworkInfo->pu8Buffer = NULL;
	}
	
	/*free structure allocated*/
	if(pstrNetworkInfo != NMI_NULL)
	{
		DeallocateNetworkInfo(pstrNetworkInfo);
		pstrNetworkInfo = NMI_NULL;
	}
		
	return s32Error;
}

/**
*  @brief Handle_RcvdGnrlAsyncInfo
*  @details 	   Handling received asynchrous general network information 
*  @param[in]    tstrRcvdGnrlAsyncInfo* pstrRcvdGnrlAsyncInfo
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_RcvdGnrlAsyncInfo(tstrRcvdGnrlAsyncInfo* pstrRcvdGnrlAsyncInfo)
{
	//TODO: mostafa: till now, this function just handles only the received mac status msg, 
	//				 which carries only 1 WID which have WID ID = WID_STATUS
	NMI_Sint32 s32Error = NMI_SUCCESS;
	NMI_Uint8  u8MsgType = 0;
	NMI_Uint8  u8MsgID = 0;
    	NMI_Uint16 u16MsgLen = 0;    
	NMI_Uint16 u16WidID = (NMI_Uint16)WID_NIL;
	NMI_Uint8 u8WidLen  = 0;
	NMI_Uint8 u8MacStatus;
	tstrConnectInfo strConnectInfo;
	tstrDisconnectNotifInfo strDisconnectNotifInfo;
	NMI_Sint32 s32Err = NMI_SUCCESS;
	
	PRINT_D(HOSTINF_DBG,"Current State = %d,Received state = %d\n",gWFiDrvHandle->enuHostIFstate,
		pstrRcvdGnrlAsyncInfo->pu8Buffer[7]);
	
	if((gWFiDrvHandle->enuHostIFstate == HOST_IF_WAITING_CONN_RESP) ||
	   (gWFiDrvHandle->enuHostIFstate == HOST_IF_CONNECTED))
	{		
		if((pstrRcvdGnrlAsyncInfo->pu8Buffer == NULL) || 
		   (gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult == NMI_NULL))
		{
			NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
		}				

		u8MsgType = pstrRcvdGnrlAsyncInfo->pu8Buffer[0];

		/* Check whether the received message type is 'I' */
		if('I' != u8MsgType)
		{
			PRINT_ER("Received Message format incorrect.\n");
			NMI_ERRORREPORT(s32Error, NMI_FAIL);
		}

		/* Extract message ID */
		u8MsgID = pstrRcvdGnrlAsyncInfo->pu8Buffer[1];
	
		/* Extract message Length */
		u16MsgLen = MAKE_WORD16(pstrRcvdGnrlAsyncInfo->pu8Buffer[2], pstrRcvdGnrlAsyncInfo->pu8Buffer[3]);
	
		/* Extract WID ID [expected to be = WID_STATUS] */
		u16WidID = MAKE_WORD16(pstrRcvdGnrlAsyncInfo->pu8Buffer[4], pstrRcvdGnrlAsyncInfo->pu8Buffer[5]);

		/* Extract WID Length [expected to be = 1] */
		u8WidLen = pstrRcvdGnrlAsyncInfo->pu8Buffer[6];

		/* get the WID value [expected to be one of two values: either MAC_CONNECTED = (1) or MAC_DISCONNECTED = (0)] */
		u8MacStatus  = pstrRcvdGnrlAsyncInfo->pu8Buffer[7];

		if(gWFiDrvHandle->enuHostIFstate == HOST_IF_WAITING_CONN_RESP)
		{
			/* our station had sent Association Request frame, so here it will get the Association Response frame then parse it */
			NMI_Uint32 u32RcvdAssocRespInfoLen;
			tstrConnectRespInfo* pstrConnectRespInfo = NULL;
			
			PRINT_D(HOSTINF_DBG,"MAC status = %d\n",u8MacStatus);
			
			NMI_memset(&strConnectInfo, 0, sizeof(tstrConnectInfo));
			
			NMI_memset(gapu8RcvdAssocResp, 0, MAX_ASSOC_RESP_FRAME_SIZE);
			
			host_int_get_assoc_res_info((NMI_WFIDrvHandle)gWFiDrvHandle, 
										gapu8RcvdAssocResp, 
										MAX_ASSOC_RESP_FRAME_SIZE, 
										&u32RcvdAssocRespInfoLen);								

			PRINT_INFO(HOSTINF_DBG,"Received association response with length = %d\n",u32RcvdAssocRespInfoLen);
			
			if(u32RcvdAssocRespInfoLen != 0)
			{
			
				PRINT_D(HOSTINF_DBG,"Parsing association response\n");
				s32Err = ParseAssocRespInfo(gapu8RcvdAssocResp, u32RcvdAssocRespInfoLen, 
									      &pstrConnectRespInfo);
				if(s32Err)
				{
					PRINT_ER("ParseAssocRespInfo() returned error %d \n", s32Err);
				}
				else
				{					
					/* use the necessary parsed Info from the Received Association Response */
					strConnectInfo.u16ConnectStatus = pstrConnectRespInfo->u16ConnectStatus;

					if(strConnectInfo.u16ConnectStatus == SUCCESSFUL_STATUSCODE)
					{
						PRINT_INFO(HOSTINF_DBG,"Association response received : Successful connection status\n");
						if(pstrConnectRespInfo->pu8RespIEs != NULL)
						{	
							strConnectInfo.u16RespIEsLen = pstrConnectRespInfo->u16RespIEsLen;
							strConnectInfo.pu8RespIEs = (NMI_Uint8*)NMI_MALLOC(pstrConnectRespInfo->u16RespIEsLen);
							NMI_memcpy(strConnectInfo.pu8RespIEs, pstrConnectRespInfo->pu8RespIEs,
									   pstrConnectRespInfo->u16RespIEsLen);
						}
					}

					/* deallocate the Assoc. Resp. parsed structure as it is not needed anymore */
					if(pstrConnectRespInfo != NULL)
					{
						DeallocateAssocRespInfo(pstrConnectRespInfo);
						pstrConnectRespInfo = NULL;
					}
				}
			}
			
			/* The station has just received mac status and it also received assoc. response which 
			     it was waiting for. 
			     So check first the matching between the received mac status and the received status code in Asoc Resp */
			if((u8MacStatus == MAC_CONNECTED) && 
			    (strConnectInfo.u16ConnectStatus != SUCCESSFUL_STATUSCODE))
			{
				NMI_ERROR("Received MAC status is MAC_CONNECTED while the received status code in Asoc Resp is not SUCCESSFUL_STATUSCODE \n");
			}
			else if((u8MacStatus == MAC_DISCONNECTED) && 
			    	    (strConnectInfo.u16ConnectStatus == SUCCESSFUL_STATUSCODE))
			{
				NMI_ERROR("Received MAC status is MAC_DISCONNECTED\n");
			}
			
			//TODO: mostafa: correct BSSID should be retrieved from actual BSSID received from AP
			//               through a structure of type tstrConnectRespInfo
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
			{
				PRINT_D(HOSTINF_DBG,"Retrieving actual BSSID from AP\n");
				NMI_memcpy(strConnectInfo.au8bssid, gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid, 6);

				if((u8MacStatus == MAC_CONNECTED) &&
			    	    (strConnectInfo.u16ConnectStatus == SUCCESSFUL_STATUSCODE))


				{
					NMI_memcpy(gWFiDrvHandle->au8AssociatedBSSID,
							   gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid, ETH_ALEN);
				}
			}

				
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
			{
				strConnectInfo.ReqIEsLen = gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen;
				strConnectInfo.pu8ReqIEs = (NMI_Uint8*)NMI_MALLOC(gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen);
				NMI_memcpy(strConnectInfo.pu8ReqIEs, 
							  gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs, 
							  gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen);
			}						

			
			NMI_TimerStop(&hConnectTimer, NMI_NULL);					
			gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult(CONN_DISCONN_EVENT_CONN_RESP, 
															      &strConnectInfo,
															      u8MacStatus,
															      NULL,
															      gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid);												
			

			/* if received mac status is MAC_CONNECTED and 
			    received status code in Asoc Resp is SUCCESSFUL_STATUSCODE, change state to CONNECTED 
			    else change state to IDLE */
			if((u8MacStatus == MAC_CONNECTED) && 
			   (strConnectInfo.u16ConnectStatus == SUCCESSFUL_STATUSCODE))
			{
				PRINT_D(HOSTINF_DBG,"MAC status : CONNECTED and Connect Status : Successful\n");
				gWFiDrvHandle->enuHostIFstate = HOST_IF_CONNECTED;
				#ifdef NMI_PARSE_SCAN_IN_HOST				
				//open a BA session if possible
				//if(gWFiDrvHandle->strNMI_UsrConnReq.IsHTCapable)
				
				#endif
				
				//host_int_addBASession(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid,0,
				//	BA_SESSION_DEFAULT_BUFFER_SIZE,BA_SESSION_DEFAULT_TIMEOUT);
			}
			else
			{
				PRINT_D(HOSTINF_DBG,"MAC status : %d and Connect Status : %d\n",u8MacStatus,strConnectInfo.u16ConnectStatus);
				gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;
				gbScanWhileConnected = NMI_FALSE;
			}

			/* Deallocation */								
			if(strConnectInfo.pu8RespIEs != NULL)
			{
				NMI_FREE(strConnectInfo.pu8RespIEs);
				strConnectInfo.pu8RespIEs = NULL;
			}				

			if(strConnectInfo.pu8ReqIEs != NULL)
			{
				NMI_FREE(strConnectInfo.pu8ReqIEs);
				strConnectInfo.pu8ReqIEs = NULL;
			}


			gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = 0;
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = NULL;
			}

			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = NULL;
			}

			gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = 0;
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = NULL;
			}
			
		}
		else if((u8MacStatus == MAC_DISCONNECTED) &&
			    (gWFiDrvHandle->enuHostIFstate == HOST_IF_CONNECTED))
		{
			/* Disassociation or Deauthentication frame has been received */
			PRINT_D(HOSTINF_DBG,"Received MAC_DISCONNECTED from the FW\n");

			NMI_memset(&strDisconnectNotifInfo, 0, sizeof(tstrDisconnectNotifInfo));
			
			strDisconnectNotifInfo.u16reason = 0;
			strDisconnectNotifInfo.ie = NULL;
			strDisconnectNotifInfo.ie_len = 0;
			
			if(gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult != NULL)
			{
				gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult(CONN_DISCONN_EVENT_DISCONN_NOTIF, 
															      	      NULL,
															      	      0,
															      	      &strDisconnectNotifInfo,
															      	      gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid);
			}
			else
			{
				PRINT_ER("Connect result callback function is NULL \n");
			}

			NMI_memset(gWFiDrvHandle->au8AssociatedBSSID, 0, ETH_ALEN);

			
			/* Deallocation */

			/* if Information Elements were retrieved from the Received deauth/disassoc frame, then they 
			    should be deallocated here */
			/*
			if(strDisconnectNotifInfo.ie != NULL)
			{
				NMI_FREE(strDisconnectNotifInfo.ie);
				strDisconnectNotifInfo.ie = NULL;
			}		
			*/
			
			gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = 0;
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = NULL;
			}

			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = NULL;
			}

			gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = 0;
			if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
			{
				NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs);
				gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = NULL;
			}			

			gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;
			gbScanWhileConnected = NMI_FALSE;
			
		}
		
	}

	NMI_CATCH(s32Error)
	{

	}

	/* Deallocate pstrRcvdGnrlAsyncInfo->pu8Buffer which was prevoisuly allocated by the sending thread */
	if(pstrRcvdGnrlAsyncInfo->pu8Buffer != NULL)
	{
		NMI_FREE(pstrRcvdGnrlAsyncInfo->pu8Buffer);
		pstrRcvdGnrlAsyncInfo->pu8Buffer = NULL;
	}	
	
	return s32Error;
}

/**
*  @brief Handle_Key
*  @details 	   Sending config packet to firmware to set key  
*  @param[in]    tstrHostIFkeyAttr* pstrHostIFkeyAttr
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
static int Handle_Key(tstrHostIFkeyAttr* pstrHostIFkeyAttr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	#ifdef NMI_AP_EXTERNAL_MLME
	tstrWID strWIDList[5];
	#endif
	NMI_Uint8 i;
	NMI_Uint8* pu8keybuf;
	NMI_Sint8 s8idxarray[1];	
	
	

	switch(pstrHostIFkeyAttr->enuKeyType)
	{

		
		case WEP:
			
			#ifdef NMI_AP_EXTERNAL_MLME
			if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY_AP)
			{
				
				PRINT_D(HOSTINF_DBG,"Handling WEP key\n");
				PRINT_D(GENERIC_DBG,"ID Hostint is %d\n",(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx));
				strWIDList[0].u16WIDid = (NMI_Uint16)WID_11I_MODE;
				strWIDList[0].enuWIDtype = WID_CHAR;
				strWIDList[0].s32ValueSize = sizeof(NMI_Char);
				strWIDList[0].ps8WidVal = (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8mode));

				strWIDList[1].u16WIDid     = WID_AUTH_TYPE;
    				strWIDList[1].enuWIDtype  = WID_CHAR;
				strWIDList[1].s32ValueSize = sizeof(NMI_Char);
    				strWIDList[1].ps8WidVal= (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.tenuAuth_type));

				strWIDList[2].u16WIDid	= (NMI_Uint16)WID_KEY_ID;
				strWIDList[2].enuWIDtype	= WID_CHAR;

				strWIDList[2].ps8WidVal	= (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx));
				strWIDList[2].s32ValueSize = sizeof(NMI_Char);
			
				
pu8keybuf = (NMI_Uint8*)NMI_MALLOC(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen);								
																				   

				if(pu8keybuf == NULL)
				{
						PRINT_ER("No buffer to send Key\n");
						return -1;
				}
				
				NMI_memcpy(pu8keybuf,pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey,
						pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen);


				NMI_FREE(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey);				
				
				strWIDList[3].u16WIDid = (NMI_Uint16)WID_WEP_KEY_VALUE;
				strWIDList[3].enuWIDtype = WID_STR;
				strWIDList[3].s32ValueSize = pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen;
				strWIDList[3].ps8WidVal = (NMI_Sint8*)pu8keybuf;

			
				s32Error = SendConfigPkt(SET_CFG, strWIDList, 4, NMI_TRUE);
				NMI_FREE(pu8keybuf);


			}
			#endif
			
			if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY)
			{
				PRINT_D(HOSTINF_DBG,"Handling WEP key\n");
				pu8keybuf = (NMI_Uint8*)NMI_MALLOC(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen + 2); 																											    	     
				if(pu8keybuf == NULL)
				{
						PRINT_ER("No buffer to send Key\n");
						return -1;
				}
				pu8keybuf[0] = pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx;

				NMI_memcpy(pu8keybuf+1, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen, 1 );

				NMI_memcpy(pu8keybuf+2,pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey,
						pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen);

				NMI_FREE(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey);				
				
				strWID.u16WIDid	= (NMI_Uint16)WID_ADD_WEP_KEY;
				strWID.enuWIDtype	= WID_STR;
				strWID.ps8WidVal	= (NMI_Sint8*)pu8keybuf;
				strWID.s32ValueSize = pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen + 2;
				
				s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
				NMI_FREE(pu8keybuf);
		}
		else if(pstrHostIFkeyAttr->u8KeyAction & REMOVEKEY)
		{

				PRINT_D(HOSTINF_DBG,"Removing key\n");
				strWID.u16WIDid	= (NMI_Uint16)WID_REMOVE_WEP_KEY;
				strWID.enuWIDtype	= WID_STR;

				s8idxarray[0] = (NMI_Sint8)pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx;				
				strWID.ps8WidVal = s8idxarray;
				strWID.s32ValueSize = 1;
				
				s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
		}
		else
		{
			strWID.u16WIDid	= (NMI_Uint16)WID_KEY_ID;
			strWID.enuWIDtype	= WID_CHAR;
			strWID.ps8WidVal	= (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx));
			strWID.s32ValueSize = sizeof(NMI_Char);
			
			PRINT_D(HOSTINF_DBG,"Setting default key index\n");

			s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
		}
		break;
		case WPARxGtk:
			#ifdef NMI_AP_EXTERNAL_MLME
			 if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY_AP)
			{
				pu8keybuf = (NMI_Uint8*)NMI_MALLOC(RX_MIC_KEY_MSG_LEN);
				if(pu8keybuf == NULL)
				{
					PRINT_ER("No buffer to send RxGTK Key\n");
					return -1;
				}
				
				NMI_memset(pu8keybuf, 0, RX_MIC_KEY_MSG_LEN);

				
				/*|----------------------------------------------------------------------------|
				  |Sta Address | Key RSC | KeyID | Key Length | Temporal Key	| Rx Michael Key |
				  |------------|---------|-------|------------|---------------|----------------|
				  |	6 bytes	 | 8 byte  |1 byte |  1 byte	|   16 bytes	|	  8 bytes	 |*/

				

				if(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8seq != NULL)
					NMI_memcpy(pu8keybuf+6, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8seq, 8 );

				
				NMI_memcpy(pu8keybuf+14,&pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8keyidx, 1);
				
				NMI_memcpy(pu8keybuf+15, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen, 1);
				
				NMI_memcpy(pu8keybuf+16, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
				pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen);
				//pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Ciphermode =  0X51;
				strWIDList[0].u16WIDid = (NMI_Uint16)WID_11I_MODE;
				strWIDList[0].enuWIDtype = WID_CHAR;
				strWIDList[0].s32ValueSize = sizeof(NMI_Char);
				strWIDList[0].ps8WidVal = (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Ciphermode));
				
				strWIDList[1].u16WIDid	= (NMI_Uint16)WID_ADD_RX_GTK;
				strWIDList[1].enuWIDtype	= WID_STR;
				strWIDList[1].ps8WidVal	= (NMI_Sint8*)pu8keybuf;
				strWIDList[1].s32ValueSize = RX_MIC_KEY_MSG_LEN;
		
				s32Error = SendConfigPkt(SET_CFG, strWIDList, 2, NMI_TRUE);

				NMI_FREE(pu8keybuf);

				////////////////////////////
				NMI_SemaphoreRelease(&hSemTestKeyBlock, NMI_NULL);
				///////////////////////////
			}
			 
			#endif
			if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY)
			{
				PRINT_D(HOSTINF_DBG,"Handling group key(Rx) function\n");				
								
				pu8keybuf = (NMI_Uint8*)NMI_MALLOC(RX_MIC_KEY_MSG_LEN);
				if(pu8keybuf == NULL)
				{
					PRINT_ER("No buffer to send RxGTK Key\n");
					return -1;
				}
				
				NMI_memset(pu8keybuf, 0, RX_MIC_KEY_MSG_LEN);

				
				/*|----------------------------------------------------------------------------|
				  |Sta Address | Key RSC | KeyID | Key Length | Temporal Key	| Rx Michael Key |
				  |------------|---------|-------|------------|---------------|----------------|
				  |	6 bytes	 | 8 byte  |1 byte |  1 byte	|   16 bytes	|	  8 bytes	 |*/

				if(gWFiDrvHandle->enuHostIFstate == HOST_IF_CONNECTED)
				{
					NMI_memcpy(pu8keybuf, gWFiDrvHandle->au8AssociatedBSSID, ETH_ALEN);
				}
				else
				{
					PRINT_ER("Couldn't handle WPARxGtk while enuHostIFstate is not HOST_IF_CONNECTED \n");
				}

				NMI_memcpy(pu8keybuf+6, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8seq, 8 );

				NMI_memcpy(pu8keybuf+14,&pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8keyidx, 1);

				NMI_memcpy(pu8keybuf+15, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen, 1);
				NMI_memcpy(pu8keybuf+16, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
				pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen);
				
				strWID.u16WIDid	= (NMI_Uint16)WID_ADD_RX_GTK;
				strWID.enuWIDtype	= WID_STR;
				strWID.ps8WidVal	= (NMI_Sint8*)pu8keybuf;
				strWID.s32ValueSize = RX_MIC_KEY_MSG_LEN;
				
				s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);

				NMI_FREE(pu8keybuf);

				////////////////////////////
				NMI_SemaphoreRelease(&hSemTestKeyBlock, NMI_NULL);
				///////////////////////////
}
		break;

	case WPAPtk:
		#ifdef NMI_AP_EXTERNAL_MLME	
		if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY_AP)
			{


			pu8keybuf = (NMI_Uint8*)NMI_MALLOC(PTK_KEY_MSG_LEN+1);					
			
			
			
		if(pu8keybuf == NULL)
			{
				PRINT_ER("No buffer to send PTK Key\n");
				return -1;
			}			
			
			/*|-----------------------------------------------------------------------------|
			  |Station address |   keyidx     |Key Length    |Temporal Key  | Rx Michael Key |Tx Michael Key |
			  |----------------|------------  |--------------|----------------|---------------|
			  |	6 bytes    |	1 byte    |   1byte	 |   16 bytes	 |	  8 bytes	  |	   8 bytes	  |
			  |-----------------------------------------------------------------------------|*/

			NMI_memcpy(pu8keybuf, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8macaddr, 6 );			/*1 bytes Key Length */
	
			NMI_memcpy(pu8keybuf+6, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8keyidx, 1 );
			NMI_memcpy(pu8keybuf+7, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen, 1 );
			/*16 byte TK*/
			NMI_memcpy(pu8keybuf+8, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
			pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen);
		
		
			strWIDList[0].u16WIDid = (NMI_Uint16)WID_11I_MODE;
			strWIDList[0].enuWIDtype = WID_CHAR;
			strWIDList[0].s32ValueSize = sizeof(NMI_Char);
			strWIDList[0].ps8WidVal = (NMI_Sint8*)(&(pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Ciphermode));

			strWIDList[1].u16WIDid	= (NMI_Uint16)WID_ADD_PTK;
			strWIDList[1].enuWIDtype	= WID_STR;
			strWIDList[1].ps8WidVal	= (NMI_Sint8 *)pu8keybuf;
			strWIDList[1].s32ValueSize = PTK_KEY_MSG_LEN+1;			
			
			s32Error = SendConfigPkt(SET_CFG, strWIDList, 2, NMI_TRUE);
			NMI_FREE(pu8keybuf);
			
			////////////////////////////
			NMI_SemaphoreRelease(&hSemTestKeyBlock, NMI_NULL);
			///////////////////////////
			}
		#endif
		if(pstrHostIFkeyAttr->u8KeyAction & ADDKEY)
			{

			
			pu8keybuf = (NMI_Uint8*)NMI_MALLOC(PTK_KEY_MSG_LEN);					
			
			
			
		if(pu8keybuf == NULL)
			{
				PRINT_ER("No buffer to send PTK Key\n");
				return -1;
			}			
			
			/*|-----------------------------------------------------------------------------|
			  |Station address | Key Length |	Temporal Key | Rx Michael Key |Tx Michael Key |
			  |----------------|------------|--------------|----------------|---------------|
			  |	6 bytes		 |	1byte	  |   16 bytes	 |	  8 bytes	  |	   8 bytes	  |
			  |-----------------------------------------------------------------------------|*/

			NMI_memcpy(pu8keybuf, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8macaddr, 6 );			/*1 bytes Key Length */
	
			NMI_memcpy(pu8keybuf+6, &pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen, 1 );
			/*16 byte TK*/
			NMI_memcpy(pu8keybuf+7, pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
			pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen);
			
			
			strWID.u16WIDid	= (NMI_Uint16)WID_ADD_PTK;
			strWID.enuWIDtype	= WID_STR;
			strWID.ps8WidVal	= (NMI_Sint8 *)pu8keybuf;
			strWID.s32ValueSize = PTK_KEY_MSG_LEN;			
			
			s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
			NMI_FREE(pu8keybuf);
			
			////////////////////////////
			NMI_SemaphoreRelease(&hSemTestKeyBlock, NMI_NULL);
			///////////////////////////
		}

			break;


	case PMKSA:

		PRINT_D(HOSTINF_DBG,"Handling PMKSA key\n");
		
		pu8keybuf = (NMI_Uint8*)NMI_MALLOC( (pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.numpmkid*PMKSA_KEY_LEN) + 1);
		if(pu8keybuf == NULL)
		{
			PRINT_ER("No buffer to send PMKSA Key\n");
			return -1;
		}		

		pu8keybuf[0] = pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.numpmkid;

		for(i = 0 ; i< pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.numpmkid;i++)
		{

			NMI_memcpy(pu8keybuf+((PMKSA_KEY_LEN * i)+1), pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.pmkidlist[i].bssid, ETH_ALEN);
			NMI_memcpy(pu8keybuf+((PMKSA_KEY_LEN * i)+ETH_ALEN+1), pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.pmkidlist[i].pmkid, PMKID_LEN);
		}

		strWID.u16WIDid	= (NMI_Uint16)WID_PMKID_INFO;
		strWID.enuWIDtype = WID_STR;
		strWID.ps8WidVal = (NMI_Sint8*)pu8keybuf;
		strWID.s32ValueSize = (pstrHostIFkeyAttr->uniHostIFkeyAttr.strHostIFpmkidAttr.numpmkid*PMKSA_KEY_LEN) + 1;
		
		s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);

		NMI_FREE(pu8keybuf);
		break;
	}

	if(s32Error)
		PRINT_ER("Failed to send key config packet\n");


	return s32Error;
}


/**
*  @brief Handle_Disconnect
*  @details 	   Sending config packet to firmware to disconnect
*  @param[in]    NONE
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_Disconnect(void)
{	
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;	
	NMI_Uint16 u16DummyReasonCode = 0;
	
	strWID.u16WIDid = (NMI_Uint16)WID_DISCONNECT;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8*)&u16DummyReasonCode;
	strWID.s32ValueSize = sizeof(NMI_Char);		


	PRINT_D(HOSTINF_DBG,"Sending disconnect request\n");

	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	
	if(s32Error)
	{
		PRINT_ER("Failed to send dissconect config packet\n");
		NMI_ERRORREPORT(s32Error,NMI_FAIL);
	}
	else
	{
		tstrDisconnectNotifInfo strDisconnectNotifInfo;		

		NMI_memset(&strDisconnectNotifInfo, 0, sizeof(tstrDisconnectNotifInfo));
			
		strDisconnectNotifInfo.u16reason = 0;
		strDisconnectNotifInfo.ie = NULL;
		strDisconnectNotifInfo.ie_len = 0;
		
		if(gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult != NULL)
		{			

			gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult(CONN_DISCONN_EVENT_DISCONN_NOTIF, NULL,
				0, &strDisconnectNotifInfo,gWFiDrvHandle->strNMI_UsrConnReq.u32UserConnectPvoid);
		}
		else
		{
			NMI_ERROR("strNMI_UsrConnReq.pfUserConnectResult = NULL \n");
		}

		gbScanWhileConnected = NMI_FALSE;
		
		gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;

		NMI_memset(gWFiDrvHandle->au8AssociatedBSSID, 0, ETH_ALEN);

		
		/* Deallocation */
		gWFiDrvHandle->strNMI_UsrConnReq.ssidLen = 0;
		if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid != NULL)
		{
			NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid);
			gWFiDrvHandle->strNMI_UsrConnReq.pu8ssid = NULL;
		}

		if(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid != NULL)
		{
			NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid);
			gWFiDrvHandle->strNMI_UsrConnReq.pu8bssid = NULL;
		}

		gWFiDrvHandle->strNMI_UsrConnReq.ConnReqIEsLen = 0;
		if(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs != NULL)
		{
			NMI_FREE(gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs);
			gWFiDrvHandle->strNMI_UsrConnReq.pu8ConnReqIEs = NULL;
		}			

		
	}

	NMI_CATCH(s32Error)
	{

	}

	////////////////////////////
	NMI_SemaphoreRelease(&hSemTestDisconnectBlock, NMI_NULL);
	///////////////////////////
	
}


/**
*  @brief Handle_GetChnl
*  @details 	   Sending config packet to get channel
*  @param[in]    NONE
*  @return 	    NONE

*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_GetChnl(void)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID	strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_CURRENT_CHANNEL;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8 *)&gu8Chnl;
	strWID.s32ValueSize = sizeof(NMI_Char);

	PRINT_D(HOSTINF_DBG,"Getting channel value\n");

	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);
	/*get the value by searching the local copy*/
	if(s32Error)
	{
		PRINT_ER("Failed to get channel number\n");
				NMI_ERRORREPORT(s32Error,NMI_FAIL);
	}


	NMI_CATCH(s32Error)
	{

	}
	NMI_SemaphoreRelease(&hSemGetCHNL, NMI_NULL);

	return s32Error;



}


/**
*  @brief Handle_GetRssi
*  @details 	   Sending config packet to get RSSI
*  @param[in]    NONE
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_GetRssi(void)
{	
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
		
	strWID.u16WIDid = (NMI_Uint16)WID_RSSI;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = &gs8Rssi;
	strWID.s32ValueSize = sizeof(NMI_Char);

	/*Sending Cfg*/
	PRINT_D(HOSTINF_DBG,"Getting RSSI value\n");

	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error)
	{
		PRINT_ER("Failed to get RSSI value\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{

	}
	NMI_SemaphoreRelease(&hSemGetRSSI, NMI_NULL);
	
	
}

#ifdef NMI_AP_EXTERNAL_MLME


/**
*  @brief Handle_Get_InActiveTime
*  @details 	   Sending config packet to set mac adddress for station and 
                   get inactive time
*  @param[in]    NONE
*  @return 	    NONE

*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Sint32 Handle_Get_InActiveTime(tstrHostIfStaInactiveT* strHostIfStaInactiveT)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	NMI_Uint8 *stamac;
	tstrWID	strWID;
	

	
	strWID.u16WIDid = (NMI_Uint16)WID_SET_STA_MAC_INACTIVE_TIME;
	strWID.enuWIDtype = WID_STR;
	strWID.s32ValueSize = ETH_ALEN;
	strWID.ps8WidVal = (NMI_Uint8 *)NMI_MALLOC(strWID.s32ValueSize);

	
	stamac = strWID.ps8WidVal;
	NMI_memcpy(stamac,strHostIfStaInactiveT->mac, ETH_ALEN);


	PRINT_D(CFG80211_DBG,"SETING STA inactive time\n");

	
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	/*get the value by searching the local copy*/
	if(s32Error)
	{
		PRINT_ER("Failed to SET incative time\n");
				NMI_ERRORREPORT(s32Error,NMI_FAIL);
	}	

	
	strWID.u16WIDid = (NMI_Uint16)WID_GET_INACTIVE_TIME;
	strWID.enuWIDtype = WID_INT;
	strWID.ps8WidVal = (NMI_Sint8*)&gu32InactiveTime;
	strWID.s32ValueSize = sizeof(NMI_Uint32);

	
	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);
	/*get the value by searching the local copy*/
	if(s32Error)
	{
		PRINT_ER("Failed to get incative time\n");
				NMI_ERRORREPORT(s32Error,NMI_FAIL);
	}


	PRINT_D(CFG80211_DBG,"Getting inactive time : %d\n",gu32InactiveTime);
	
	NMI_SemaphoreRelease(&hSemInactiveTime, NMI_NULL);
	NMI_CATCH(s32Error)
	{

	}
	

	return s32Error;



}


/**
*  @brief Handle_AddBeacon
*  @details 	   Sending config packet to add beacon
*  @param[in]    tstrHostIFSetBeacon* pstrSetBeaconParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_AddBeacon(tstrHostIFSetBeacon* pstrSetBeaconParam)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Uint8* pu8CurrByte;

	PRINT_D(HOSTINF_DBG,"Adding BEACON\n");

	strWID.u16WIDid = (NMI_Uint16)WID_ADD_BEACON;
	strWID.enuWIDtype = WID_BIN;
	strWID.s32ValueSize = pstrSetBeaconParam->u32HeadLen+pstrSetBeaconParam->u32TailLen+16;
	strWID.ps8WidVal = NMI_MALLOC(strWID.s32ValueSize);
	if(strWID.ps8WidVal == NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_NO_MEM);
	}

	pu8CurrByte = strWID.ps8WidVal;
	*pu8CurrByte++ = (pstrSetBeaconParam->u32Interval & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32Interval >> 8) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32Interval >> 16) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32Interval >> 24) & 0xFF);

	*pu8CurrByte++ = (pstrSetBeaconParam->u32DTIMPeriod & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32DTIMPeriod >> 8) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32DTIMPeriod >> 16) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32DTIMPeriod >> 24) & 0xFF);

	*pu8CurrByte++ = (pstrSetBeaconParam->u32HeadLen & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32HeadLen >> 8) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32HeadLen >> 16) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32HeadLen >> 24) & 0xFF);

	memcpy(pu8CurrByte, pstrSetBeaconParam->pu8Head, pstrSetBeaconParam->u32HeadLen);
	pu8CurrByte += pstrSetBeaconParam->u32HeadLen;
	
	*pu8CurrByte++ = (pstrSetBeaconParam->u32TailLen& 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32TailLen >> 8) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32TailLen >> 16) & 0xFF);
	*pu8CurrByte++ = ((pstrSetBeaconParam->u32TailLen >> 24) & 0xFF);

	/* Bug 4599 : if tail length = 0 skip copying */ 
	if(pstrSetBeaconParam->pu8Tail > 0)
		memcpy(pu8CurrByte, pstrSetBeaconParam->pu8Tail, pstrSetBeaconParam->u32TailLen);
	pu8CurrByte += pstrSetBeaconParam->u32TailLen;
	

	
	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	if(s32Error)
	{
		PRINT_ER("Failed to send add beacon config packet\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{
	}
	NMI_FREE_IF_TRUE(strWID.ps8WidVal);
	NMI_FREE_IF_TRUE(pstrSetBeaconParam->pu8Head);
	NMI_FREE_IF_TRUE(pstrSetBeaconParam->pu8Tail);
}


/**
*  @brief Handle_AddBeacon
*  @details 	   Sending config packet to delete beacon
*  @param[in]   tstrHostIFDelBeacon* pstrDelBeacon
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_DelBeacon(tstrHostIFDelBeacon* pstrDelBeacon)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Uint8* pu8CurrByte;

	strWID.u16WIDid = (NMI_Uint16)WID_DEL_BEACON;
	strWID.enuWIDtype = WID_CHAR;
	strWID.s32ValueSize = 0;

	if(strWID.ps8WidVal == NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_NO_MEM);
	}

	pu8CurrByte = strWID.ps8WidVal;

	PRINT_D(HOSTINF_DBG,"Deleting BEACON\n");
	/* TODO: build del beacon message*/

	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	if(s32Error)
	{
	
		PRINT_ER("Failed to send delete beacon config packet\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{
	}
}


/**
*  @brief NMI_HostIf_PackStaParam
*  @details 	   Handling packing of the station params in a buffer
*  @param[in]   NMI_Uint8* pu8Buffer, tstrNMI_AddStaParam* pstrStationParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static NMI_Uint32 NMI_HostIf_PackStaParam(NMI_Uint8* pu8Buffer, tstrNMI_AddStaParam* pstrStationParam)
{
	NMI_Uint8* pu8CurrByte;

	pu8CurrByte = pu8Buffer;

	PRINT_D(HOSTINF_DBG,"Packing STA params\n");
	NMI_memcpy(pu8CurrByte, pstrStationParam->au8BSSID, ETH_ALEN);
	pu8CurrByte +=  ETH_ALEN;

	*pu8CurrByte++ = pstrStationParam->u16AssocID & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u16AssocID >> 8) & 0xFF;

	*pu8CurrByte++ = pstrStationParam->u8NumRates;
	if(pstrStationParam->u8NumRates > 0)
	{
		NMI_memcpy(pu8CurrByte, pstrStationParam->pu8Rates, pstrStationParam->u8NumRates);
	}
	pu8CurrByte += pstrStationParam->u8NumRates;

	*pu8CurrByte++ = pstrStationParam->bIsHTSupported;
	*pu8CurrByte++ = pstrStationParam->u16HTCapInfo & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u16HTCapInfo >> 8) & 0xFF;

	*pu8CurrByte++ = pstrStationParam->u8AmpduParams;
	NMI_memcpy(pu8CurrByte, pstrStationParam->au8SuppMCsSet, NMI_SUPP_MCS_SET_SIZE);
	*pu8CurrByte++ = NMI_SUPP_MCS_SET_SIZE;

	*pu8CurrByte++ = pstrStationParam->u16HTExtParams & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u16HTExtParams >> 8) & 0xFF;

	*pu8CurrByte++ = pstrStationParam->u32TxBeamformingCap & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u32TxBeamformingCap >> 8) & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u32TxBeamformingCap >> 16) & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u32TxBeamformingCap >> 24) & 0xFF;

	*pu8CurrByte++ = pstrStationParam->u8ASELCap;

	*pu8CurrByte++ = pstrStationParam->u16FlagsMask & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u16FlagsMask >>8) & 0xFF;

	*pu8CurrByte++ = pstrStationParam->u16FlagsSet & 0xFF;
	*pu8CurrByte++ = (pstrStationParam->u16FlagsSet >>8) & 0xFF;

	return pu8CurrByte - pu8Buffer;
}

/**
*  @brief Handle_AddStation
*  @details 	   Sending config packet to add station 
*  @param[in]   tstrNMI_AddStaParam* pstrStationParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_AddStation(tstrNMI_AddStaParam* pstrStationParam)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Uint8* pu8CurrByte;

	PRINT_D(HOSTINF_DBG,"Handling add station\n");
	strWID.u16WIDid = (NMI_Uint16)WID_ADD_STA;
	strWID.enuWIDtype = WID_BIN;
	strWID.s32ValueSize =NMI_ADD_STA_LENGTH + pstrStationParam->u8NumRates;

	strWID.ps8WidVal = NMI_MALLOC(strWID.s32ValueSize);
	if(strWID.ps8WidVal == NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_NO_MEM);
	}

	pu8CurrByte = strWID.ps8WidVal;
	pu8CurrByte += NMI_HostIf_PackStaParam(pu8CurrByte, pstrStationParam);

	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	if(s32Error != NMI_SUCCESS)
	{
	
		PRINT_ER("Failed to send add station config packet\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{
	}
	NMI_FREE_IF_TRUE(pstrStationParam->pu8Rates);
	NMI_FREE_IF_TRUE(strWID.ps8WidVal);
}


/**
*  @brief Handle_DelStation
*  @details 	    Sending config packet to delete station 
*  @param[in]   tstrHostIFDelSta* pstrDelStaParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_DelStation(tstrHostIFDelSta* pstrDelStaParam)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Uint8* pu8CurrByte;

	strWID.u16WIDid = (NMI_Uint16)WID_REMOVE_STA;
	strWID.enuWIDtype = WID_BIN;
	strWID.s32ValueSize =ETH_ALEN;

	PRINT_D(HOSTINF_DBG,"Handling delete station \n");
	
	strWID.ps8WidVal = NMI_MALLOC(strWID.s32ValueSize);
	if(strWID.ps8WidVal == NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_NO_MEM);
	}

	pu8CurrByte = strWID.ps8WidVal;

	NMI_memcpy(pu8CurrByte, pstrDelStaParam->au8MacAddr, ETH_ALEN);

	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	if(s32Error)
	{
	
		PRINT_ER("Failed to send add station config packe\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{
	}
	NMI_FREE_IF_TRUE(strWID.ps8WidVal);
}


/**
*  @brief Handle_EditStation
*  @details 	    Sending config packet to edit station 
*  @param[in]   tstrNMI_AddStaParam* pstrStationParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_EditStation(tstrNMI_AddStaParam* pstrStationParam)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Uint8* pu8CurrByte;

	strWID.u16WIDid = (NMI_Uint16)WID_EDIT_STA;
	strWID.enuWIDtype = WID_BIN;
	strWID.s32ValueSize =NMI_ADD_STA_LENGTH + pstrStationParam->u8NumRates;

	PRINT_D(HOSTINF_DBG,"Handling edit station\n");
	strWID.ps8WidVal = NMI_MALLOC(strWID.s32ValueSize);
	if(strWID.ps8WidVal == NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_NO_MEM);
	}

	pu8CurrByte = strWID.ps8WidVal;
	pu8CurrByte += NMI_HostIf_PackStaParam(pu8CurrByte, pstrStationParam);

	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_FALSE);
	if(s32Error)
	{
	
		PRINT_ER("Failed to send edit station config packet\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	NMI_CATCH(s32Error)
	{
	}
	NMI_FREE_IF_TRUE(pstrStationParam->pu8Rates);
	NMI_FREE_IF_TRUE(strWID.ps8WidVal);
}
#endif /*NMI_AP_EXTERNAL_MLME*/


/**
*  @brief Handle_EditStation
*  @details 	    Sending config packet to edit station 
*  @param[in]   tstrNMI_AddStaParam* pstrStationParam
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void Handle_PowerManagement(tstrHostIfPowerMgmtParam* strPowerMgmtParam)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	NMI_Sint8 s8PowerMode;
	strWID.u16WIDid = (NMI_Uint16)WID_POWER_MANAGEMENT;
	
	if(strPowerMgmtParam->bIsEnabled == NMI_TRUE)
	{
		s8PowerMode = MIN_FAST_PS;	
	}
	else
	{
		s8PowerMode = NO_POWERSAVE;
	}
	PRINT_D(HOSTINF_DBG,"Handling power mgmt to %d\n", s8PowerMode);
	strWID.ps8WidVal = &s8PowerMode;
	strWID.s32ValueSize = sizeof(NMI_Char);

	PRINT_D(HOSTINF_DBG,"Handling Power Management\n");
	
	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error)
	{
		PRINT_ER("Failed to send power management config packet\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	
	NMI_CATCH(s32Error)
	{

	}
}

/**
*  @brief hostIFthread
*  @details 	    Main thread to handle message queue requests 
*  @param[in]   void* pvArg
*  @return 	    NONE
*  @author	
*  @date	
*  @version	1.0
*/
static void hostIFthread(void* pvArg)
{
	NMI_Uint32 u32Ret;
	tstrHostIFmsg strHostIFmsg;
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
	
	while(1)
	{
		NMI_MsgQueueRecv(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), &u32Ret, NMI_NULL);				
		
		if(strHostIFmsg.u16MsgId == HOST_IF_MSG_EXIT)
		{
			PRINT_D(GENERIC_DBG,"THREAD: Exiting HostIfThread\n");
			break;
		}

		switch(strHostIFmsg.u16MsgId)
		{
			case HOST_IF_MSG_SCAN:
			{
				Handle_Scan(&strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr);
				break;
			}			
			case HOST_IF_MSG_CONNECT:
			{
				Handle_Connect(&strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr);
				break;
			}
			case HOST_IF_MSG_RCVD_NTWRK_INFO:
			{
				Handle_RcvdNtwrkInfo(&strHostIFmsg.uniHostIFmsgBody.strRcvdNetworkInfo);
				break;
			}
			case HOST_IF_MSG_RCVD_GNRL_ASYNC_INFO:
			{
				Handle_RcvdGnrlAsyncInfo(&strHostIFmsg.uniHostIFmsgBody.strRcvdGnrlAsyncInfo);
				break;
			}
			case HOST_IF_MSG_KEY:
			{
				Handle_Key(&strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr);
				break;
			}
			case HOST_IF_MSG_CFG_PARAMS:
			{
				
				Handle_CfgParam(&strHostIFmsg.uniHostIFmsgBody.strHostIFCfgParamAttr);
				break;
			}
			case HOST_IF_MSG_SET_CHANNEL:
			{
				Handle_SetChannel(&strHostIFmsg.uniHostIFmsgBody.strHostIFSetChan);
				break;
			}			
			case HOST_IF_MSG_DISCONNECT:
			{
				Handle_Disconnect();				
				break;
			}
			case HOST_IF_MSG_RCVD_SCAN_COMPLETE:
			{	
				NMI_TimerStop(&hScanTimer, NMI_NULL);
				PRINT_D(HOSTINF_DBG, "scan completed successfully\n");

				if(gWFiDrvHandle->enuHostIFstate != HOST_IF_CONNECTED) 
				{
					/* force the chip manually to sleep */
#ifndef UNUSE_POWER_MGMT //johnny : Adel guide
					printk("johnny chip_sleep_manually\n");
					chip_sleep_manually(INFINITE_SLEEP_TIME);
#endif
				}
				
				Handle_ScanDone(SCAN_EVENT_DONE);											
				break;
			}
			case HOST_IF_MSG_GET_RSSI:
			{
				Handle_GetRssi();				
				break;
			}
			case HOST_IF_MSG_GET_CHNL:
			{
				Handle_GetChnl();
				break;
			}

			#ifdef NMI_AP_EXTERNAL_MLME
			case HOST_IF_MSG_ADD_BEACON:
			{
				Handle_AddBeacon(&strHostIFmsg.uniHostIFmsgBody.strHostIFSetBeacon);
				break;
			}
			break;
			
			case HOST_IF_MSG_DEL_BEACON:
			{
				Handle_DelBeacon(&strHostIFmsg.uniHostIFmsgBody.strHostIFDelBeacon);
				break;
			}
			break;
			
			case HOST_IF_MSG_ADD_STATION:
			{
				Handle_AddStation(&strHostIFmsg.uniHostIFmsgBody.strAddStaParam);
				break;
			}

			case HOST_IF_MSG_DEL_STATION:
			{
				Handle_DelStation(&strHostIFmsg.uniHostIFmsgBody.strDelStaParam);
				break;
			}
			case HOST_IF_MSG_EDIT_STATION:
			{
				Handle_EditStation(&strHostIFmsg.uniHostIFmsgBody.strEditStaParam);
				break;
			}
			case HOST_IF_MSG_GET_INACTIVETIME:
			{
				Handle_Get_InActiveTime(&strHostIFmsg.uniHostIFmsgBody.strHostIfStaInactiveT);
				break;
			}
			#endif /*NMI_AP_EXTERNAL_MLME*/
			case HOST_IF_MSG_SCAN_TIMER_FIRED:
			{
				NMI_PRINTF("Scan Timeout\n");
				Handle_ScanDone(SCAN_EVENT_DONE);
				break;
			}
			case HOST_IF_MSG_CONNECT_TIMER_FIRED:
			{
				NMI_PRINTF("Connect Timeout \n");
				Handle_ConnectTimeout();
				break;
			}
			case HOST_IF_MSG_POWER_MGMT:
			{
				Handle_PowerManagement(&strHostIFmsg.uniHostIFmsgBody.strPowerMgmtparam);
				break;
			}

			default:
			{
				NMI_ERROR("[Host Interface] undefined Received Msg ID  \n");
				break;
			}
		}
	}

	PRINT_D(HOSTINF_DBG,"Releasing thread exit semaphore\n");
	NMI_SemaphoreRelease(&hSemHostIFthrdEnd, NMI_NULL);
	return;
	//do_exit(error);
	//PRINT_D(HOSTINF_DBG,"do_exit error code %d\n",error);
	
}

static void TimerCB(void* pvArg)
{
	tstrHostIFmsg strHostIFmsg;
	tenuScanConnTimer enuScanConnTimer;

	enuScanConnTimer = (tenuScanConnTimer)pvArg;

	PRINT_D(HOSTINF_DBG, "TimerCB(): rcvd enuScanConnTimer=%d\n", enuScanConnTimer);
	
	/* prepare the Timer Callback message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));	
	
	if(enuScanConnTimer == SCAN_TIMER)
	{	
		strHostIFmsg.u16MsgId = HOST_IF_MSG_SCAN_TIMER_FIRED;
		strHostIFmsg.uniHostIFmsgBody.strTimerCb.pvUsrArg = (void*)enuScanConnTimer;		
	}
	else if(enuScanConnTimer == CONNECT_TIMER)
	{	
		strHostIFmsg.u16MsgId = HOST_IF_MSG_CONNECT_TIMER_FIRED;
		strHostIFmsg.uniHostIFmsgBody.strTimerCb.pvUsrArg = (void*)enuScanConnTimer;		
	}

	/* send the message */
	NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
}

/**
*  @brief 		removes wpa/wpa2 keys
*  @details 	only in BSS STA mode if External Supplicant support is enabled. 
				removes all WPA/WPA2 station key entries from MAC hardware.
*  @param[in,out] handle to the wifi driver
*  @param[in] 	6 bytes of Station Adress in the station entry table
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
//Check implementation in core adding 9 bytes to the input!
NMI_Sint32 host_int_remove_key(NMI_WFIDrvHandle hWFIDrv,const NMI_Uint8* pu8StaAddress)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid	= (NMI_Uint16)WID_REMOVE_KEY;
	strWID.enuWIDtype	= WID_STR;
	strWID.ps8WidVal	= (NMI_Sint8*)pu8StaAddress;
	strWID.s32ValueSize = 6;
	
	return s32Error;
	
}

/**
*  @brief 		removes WEP key
*  @details 	valid only in BSS STA mode if External Supplicant support is enabled.
				remove a WEP key entry from MAC HW. 
				The BSS Station automatically finds the index of the entry using its 
				BSS ID and removes that entry from the MAC hardware.
*  @param[in,out] handle to the wifi driver
*  @param[in] 	6 bytes of Station Adress in the station entry table
*  @return 		Error code indicating success/failure
*  @note 		NO need for the STA add since it is not used for processing 
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_remove_wep_key(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 u8keyIdx)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;


	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	/* prepare the Remove Wep Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));


	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WEP;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = REMOVEKEY;


	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx = u8keyIdx;

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Error in sending message queue : Request to remove WEP key \n");
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;
}

/**
*  @brief 		sets WEP default key
*  @details 	Sets the index of the WEP encryption key in use, 
				in the key table	
*  @param[in,out] handle to the wifi driver
*  @param[in] 	key index ( 0, 1, 2, 3)
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_WEPDefaultKeyID(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 u8Index)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;


	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));


	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WEP;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = DEFAULTKEY;


	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx = u8Index;

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Error in sending message queue : Default key index\n");

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 		sets WEP deafault key
*  @details 	valid only in BSS STA mode if External Supplicant support is enabled. 
				sets WEP key entry into MAC hardware when it receives the 
				corresponding request from NDIS.
*  @param[in,out] handle to the wifi driver
*  @param[in] 	message containing WEP Key in the following format
				|---------------------------------------|
				|Key ID Value | Key Length |	Key		|
				|-------------|------------|------------|
				|	1byte	  |		1byte  | Key Length	|
				|---------------------------------------|

*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_add_wep_key_bss_sta(NMI_WFIDrvHandle hWFIDrv, const NMI_Uint8* pu8WepKey, NMI_Uint8 u8WepKeylen, NMI_Uint8 u8Keyidx)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;

	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);

	}
	
	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));


	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WEP;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY;

	
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey = (NMI_Uint8*)NMI_MALLOC(u8WepKeylen);

	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey,
				pu8WepKey, u8WepKeylen);
	
	
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen = (u8WepKeylen);

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx = u8Keyidx;

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Error in sending message queue :WEP Key\n");

	NMI_CATCH(s32Error)
	{

	}
	return s32Error;

}

#ifdef NMI_AP_EXTERNAL_MLME
/**

*  @brief 		host_int_add_wep_key_bss_ap
*  @details 	valid only in BSS AP mode if External Supplicant support is enabled. 
				sets WEP key entry into MAC hardware when it receives the 

				corresponding request from NDIS.
*  @param[in,out] handle to the wifi driver


*  @return 		Error code indicating success/failure
*  @note 		
*  @author		mdaftedar
*  @date		28 FEB 2013
*  @version		1.0
*/
NMI_Sint32 host_int_add_wep_key_bss_ap(NMI_WFIDrvHandle hWFIDrv, const NMI_Uint8* pu8WepKey, NMI_Uint8 u8WepKeylen, NMI_Uint8 u8Keyidx,NMI_Uint8 u8mode, AUTHTYPE_T tenuAuth_type)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	NMI_Uint8 i;

	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);

	}
	
	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	if(INFO)
	{
	for(i=0;i<u8WepKeylen;i++)
		PRINT_INFO(HOSTAPD_DBG,"KEY is %x\n",pu8WepKey[i]);
	}
	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WEP;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY_AP;

	
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey = (NMI_Uint8*)NMI_MALLOC((u8WepKeylen));

		
	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwepAttr.pu8WepKey,
				pu8WepKey, (u8WepKeylen));
	
	
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwepAttr.u8WepKeylen = (u8WepKeylen);

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwepAttr.u8Wepidx = u8Keyidx;

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwepAttr.u8mode = u8mode;

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwepAttr.tenuAuth_type = tenuAuth_type;
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);

	if(s32Error)
		PRINT_ER("Error in sending message queue :WEP Key\n");

	NMI_CATCH(s32Error)
	{

	}
	return s32Error;

}
#endif
/**
*  @brief 		adds ptk Key
*  @details 	
*  @param[in,out] handle to the wifi driver
*  @param[in] 	message containing PTK Key in the following format
|-----------------------------------------------------------------------------|
|Station address | Key Length |	Temporal Key | Rx Michael Key |Tx Michael Key |
|----------------|------------|--------------|----------------|---------------|
|	6 bytes		 |	1byte	  |   16 bytes	 |	  8 bytes	  |	   8 bytes	  |
|-----------------------------------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_add_ptk(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8* pu8Ptk,NMI_Uint8 u8PtkKeylen,
	const NMI_Uint8* mac_addr,NMI_Uint8* pu8RxMic,NMI_Uint8* pu8TxMic,NMI_Uint8 mode,NMI_Uint8 u8Ciphermode,NMI_Uint8 u8Idx)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	NMI_Uint8 u8KeyLen = u8PtkKeylen;
	NMI_Uint32 i;
	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
	if(pu8RxMic != NULL)
	{
		u8KeyLen += RX_MIC_KEY_LEN;
	}
	if(pu8TxMic != NULL)
	{
		u8KeyLen += TX_MIC_KEY_LEN;		
	}

	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));


	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WPAPtk;
	#ifdef NMI_AP_EXTERNAL_MLME
	if(mode == AP_MODE)	
	{
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY_AP;
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8keyidx = u8Idx;
	}
	#endif
	if(mode == STATION_MODE)
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY;


	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwpaAttr.pu8key = (NMI_Uint8*)NMI_MALLOC(u8PtkKeylen);


	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
			pu8Ptk, u8PtkKeylen);

	if(pu8RxMic != NULL)
	{
		
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key+16,
					pu8RxMic,RX_MIC_KEY_LEN);
	    if(INFO)
		{
		for(i=0;i<RX_MIC_KEY_LEN;i++)
						PRINT_INFO(CFG80211_DBG, "PairwiseRx[%d] = %x\n", i, pu8RxMic[i]);
		}
	}
	if(pu8TxMic != NULL)
	{
		
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key+24,
					pu8TxMic, TX_MIC_KEY_LEN);
	   if(INFO)
	   {
		for(i=0;i<TX_MIC_KEY_LEN;i++)
						PRINT_INFO(CFG80211_DBG, "PairwiseTx[%d] = %x\n", i, pu8TxMic[i]);
		}
	}

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen = u8KeyLen;

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8Ciphermode = u8Ciphermode;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwpaAttr.pu8macaddr = mac_addr;	
	
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);

	if(s32Error)
		PRINT_ER("Error in sending message queue:  PTK Key\n");

	////////////////
	NMI_SemaphoreAcquire(&hSemTestKeyBlock,NULL);
	//NMI_Sleep(100);
	/////////	

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 		adds Rx GTk Key
*  @details 	
*  @param[in,out] handle to the wifi driver
*  @param[in] 	pu8RxGtk : contains temporal key | Rx Mic | Tx Mic
				u8GtkKeylen :The total key length

*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_add_rx_gtk(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8RxGtk,NMI_Uint8 u8GtkKeylen,
			NMI_Uint8 u8KeyIdx,NMI_Uint32 u32KeyRSClen, NMI_Uint8* KeyRSC,
			NMI_Uint8* pu8RxMic,NMI_Uint8* pu8TxMic,NMI_Uint8 mode,NMI_Uint8 u8Ciphermode)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	NMI_Uint8 u8KeyLen = u8GtkKeylen;

	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));


	if(pu8RxMic != NULL)
	{
		u8KeyLen += RX_MIC_KEY_LEN;
	}
	if(pu8TxMic != NULL)
	{
		u8KeyLen += TX_MIC_KEY_LEN;		
	}
	if(KeyRSC!= NULL)
	{
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
				uniHostIFkeyAttr.strHostIFwpaAttr.pu8seq = (NMI_Uint8*)NMI_MALLOC(u32KeyRSClen);

		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8seq,
			KeyRSC, u32KeyRSClen);	
	}
	

	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WPARxGtk;
	
    #ifdef NMI_AP_EXTERNAL_MLME
    if(mode == AP_MODE)
    {
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY_AP;
		strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.u8Ciphermode = u8Ciphermode;
    }
    #endif
	if(mode == STATION_MODE)		
	 strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY;
	

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwpaAttr.pu8key = (NMI_Uint8*)NMI_MALLOC(u8KeyLen);

	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
			pu8RxGtk, u8GtkKeylen);

	if(pu8RxMic != NULL)
	{
		
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key+16,
					pu8RxMic,RX_MIC_KEY_LEN);
		
	}
	if(pu8TxMic != NULL)
	{
		
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key+24,
					pu8TxMic, TX_MIC_KEY_LEN);
		
	}

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8keyidx = u8KeyIdx;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen = u8KeyLen;

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
			uniHostIFkeyAttr.strHostIFwpaAttr.u8seqlen = u32KeyRSClen;

	
	
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		NMI_ERROR("Error in sending message queue:  RX GTK\n");
	////////////////
	NMI_SemaphoreAcquire(&hSemTestKeyBlock,NULL);
	//NMI_Sleep(100);
	/////////	

	NMI_CATCH(s32Error)
		{

		}
	return s32Error;
}
#if 0
/**
*  @brief 			host_int_add_tx_gtk
*  @details 		adds Tx GTk Key
*  @param[in,out] handle to the wifi driver
*  @param[in] 	message containing Tx GTK Key in the following format
				|----------------------------------------------------|
				| KeyID | Key Length | Temporal Key	| Tx Michael Key |
				|-------|------------|--------------|----------------|
				|1 byte |  1 byte	 |   16 bytes	|	  8 bytes	 |
				|----------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_add_tx_gtk(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 u8KeyLen,NMI_Uint8* pu8TxGtk,NMI_Uint8 u8KeyIdx)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;

	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = WPATxGtk;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY;

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.
		uniHostIFkeyAttr.strHostIFwpaAttr.pu8key = (NMI_Uint8*)NMI_MALLOC(u8KeyLen);

	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.pu8key,
			pu8TxGtk, u8KeyLen);

	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFwpaAttr.u8Keylen = u8KeyLen;

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		NMI_ERROR("Error in sending message queue: TX GTK\n");
	
	////////////////
	NMI_SemaphoreAcquire(&hSemTestKeyBlock,NULL);
	NMI_Sleep(100);
	/////////

	NMI_CATCH(s32Error)
		{

		}
	return s32Error;
}
#endif
/**
*  @brief 		host_int_set_pmkid_info
*  @details 	caches the pmkid valid only in BSS STA mode if External Supplicant	 	
				support is enabled. This Function sets the PMKID in firmware 
				when host drivr receives the corresponding request from NDIS. 
				The firmware then includes theset PMKID in the appropriate 
				management frames		 	
*  @param[in,out] handle to the wifi driver
*  @param[in] 	message containing PMKID Info in the following format
|-----------------------------------------------------------------|
|NumEntries |	BSSID[1] | PMKID[1] |  ...	| BSSID[K] | PMKID[K] |
|-----------|------------|----------|-------|----------|----------|
|	   1	|		6	 |   16		|  ...	|	 6	   |	16	  |
|-----------------------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_pmkid_info(NMI_WFIDrvHandle hWFIDrv, tstrHostIFpmkidAttr* pu8PmkidInfoArray)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	NMI_Uint32 i;


	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	/* prepare the Key Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_KEY;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.enuKeyType = PMKSA;
	strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.u8KeyAction = ADDKEY;

	for(i = 0 ; i< pu8PmkidInfoArray->numpmkid;i++)
	{

		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFpmkidAttr.pmkidlist[i].bssid, &pu8PmkidInfoArray->pmkidlist[i].bssid,
				ETH_ALEN);

		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFkeyAttr.uniHostIFkeyAttr.strHostIFpmkidAttr.pmkidlist[i].pmkid, &pu8PmkidInfoArray->pmkidlist[i].pmkid,
				PMKID_LEN);
	}

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER(" Error in sending messagequeue: PMKID Info\n");

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 		gets the cached the pmkid info
*  @details 	valid only in BSS STA mode if External Supplicant	 	
				support is enabled. This Function sets the PMKID in firmware 
				when host drivr receives the corresponding request from NDIS. 
				The firmware then includes theset PMKID in the appropriate 
				management frames		 	
*  @param[in,out] handle to the wifi driver, 
				  message containing PMKID Info in the following format
		|-----------------------------------------------------------------|
		|NumEntries |	BSSID[1] | PMKID[1] |  ...	| BSSID[K] | PMKID[K] |
		|-----------|------------|----------|-------|----------|----------|
		|	   1	|		6	 |   16		|  ...	|	 6	   |	16	  |
		|-----------------------------------------------------------------|
*  @param[in] 	
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_pmkid_info(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8* pu8PmkidInfoArray, 
									   NMI_Uint32 u32PmkidInfoLen)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid	= (NMI_Uint16)WID_PMKID_INFO;
	strWID.enuWIDtype	= WID_STR;
	strWID.s32ValueSize = u32PmkidInfoLen;
	strWID.ps8WidVal = pu8PmkidInfoArray;
	
	return s32Error;
}

/**
*  @brief 		sets the pass phrase
*  @details 	AP/STA mode. This function gives the pass phrase used to
				generate the Pre-Shared Key when WPA/WPA2 is enabled	
				The length of the field can vary from 8 to 64 bytes, 
				the lower layer should get the  	
*  @param[in,out] handle to the wifi driver, 	
*  @param[in] 	 String containing PSK 
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_RSNAConfigPSKPassPhrase(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8PassPhrase, 
														  NMI_Uint8 u8Psklength)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	//NMI_Uint8 u8Psklength = NMI_strlen(pu8PassPhrase);
	/*validating psk length*/
	if((u8Psklength >7) && (u8Psklength<65))
	{	
		strWID.u16WIDid	= (NMI_Uint16)WID_11I_PSK;
		strWID.enuWIDtype	= WID_STR;
		strWID.ps8WidVal	= pu8PassPhrase;
		strWID.s32ValueSize = u8Psklength;
	}

	return s32Error;
}
/**
*  @brief 		host_int_get_MacAddress
*  @details	gets mac address
*  @param[in,out] handle to the wifi driver,

*  @return 		Error code indicating success/failure
*  @note
*  @author		mdaftedar
*  @date		19 April 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_MacAddress(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8* pu8MacAddress)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrWID strWID;

	if(pstrWFIDrv == NMI_NULL )
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
	strWID.u16WIDid = (NMI_Uint16)WID_MAC_ADDR;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = (NMI_Uint8*)NMI_MALLOC(ETH_ALEN);
	strWID.s32ValueSize = ETH_ALEN;
	
	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);

	NMI_memcpy(pu8MacAddress,strWID.ps8WidVal,ETH_ALEN);
	/*get the value by searching the local copy*/
	NMI_FREE(strWID.ps8WidVal);
	if(s32Error)
	{
		PRINT_ER("Failed to get mac address\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}

	else
	{
		PRINT_INFO(HOSTINF_DBG,"MAC address retrieved:: \n");
		
		PRINT_INFO(HOSTINF_DBG,"%2x.%2x.%2x.%2x.%2x.%2x.\n",(NMI_Uint8)(strWID.ps8WidVal[0]),(NMI_Uint8)(strWID.ps8WidVal[1]),
		(NMI_Uint8)(strWID.ps8WidVal[2]),(NMI_Uint8)(strWID.ps8WidVal[3]),(NMI_Uint8)(strWID.ps8WidVal[4]),(NMI_Uint8)(strWID.ps8WidVal[5]));

		PRINT_INFO(HOSTINF_DBG,"\n");
	}

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 		host_int_set_MacAddress
*  @details	sets mac address
*  @param[in,out] handle to the wifi driver,

*  @return 		Error code indicating success/failure
*  @note
*  @author		mabubakr
*  @date		16 July 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_MacAddress(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8* pu8MacAddress)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrWID strWID;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	strWID.u16WIDid = (NMI_Uint16)WID_MAC_ADDR;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = (NMI_Sint8*)pu8MacAddress;
	strWID.s32ValueSize = ETH_ALEN;	

	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
		

	if(s32Error)
	{
		PRINT_D(HOSTINF_DBG,"Failed to set mac address\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	else
	{
		PRINT_INFO(HOSTINF_DBG,"MAC address set :: \n");
		
		PRINT_INFO(HOSTINF_DBG,"%2x.%2x.%2x.%2x.%2x.%2x.\n",(NMI_Uint8)(strWID.ps8WidVal[0]),(NMI_Uint8)(strWID.ps8WidVal[1]),
		(NMI_Uint8)(strWID.ps8WidVal[2]),(NMI_Uint8)(strWID.ps8WidVal[3]),(NMI_Uint8)(strWID.ps8WidVal[4]),(NMI_Uint8)(strWID.ps8WidVal[5]));

		PRINT_INFO(HOSTINF_DBG,"\n");
	}

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 		host_int_get_RSNAConfigPSKPassPhrase
*  @details 	gets the pass phrase:AP/STA mode. This function gets the pass phrase used to
				generate the Pre-Shared Key when WPA/WPA2 is enabled	
				The length of the field can vary from 8 to 64 bytes, 
				the lower layer should get the  	
*  @param[in,out] handle to the wifi driver, 
				  String containing PSK 
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_RSNAConfigPSKPassPhrase(NMI_WFIDrvHandle hWFIDrv,
												NMI_Uint8* pu8PassPhrase, NMI_Uint8 u8Psklength)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid	= (NMI_Uint16)WID_11I_PSK;
	strWID.enuWIDtype	= WID_STR;
	strWID.s32ValueSize = u8Psklength;
	strWID.ps8WidVal	= pu8PassPhrase;
	
	return s32Error;
}

/**
*  @brief 			host_int_get_site_survey_results
*  @details 	  	gets the site survey results
*  @param[in,out] handle to the wifi driver, 
				  Message containing  site survey results in the 
				  following format 
|---------------------------------------------------|
| MsgLength | fragNo.	| MsgBodyLength	| MsgBody	|					
|-----------|-----------|---------------|-----------|
|	 1		|	  1		|		1		|	 1		|					
-----------------------------------------	 |  ----------------
										     |   						
						|---------------------------------------|
						| Network1 | Netweork2 | ... | Network5 |
					|---------------------------------------|
						|	44	   |	44	   | ... |	 44		|
-------------------------- | ---------------------------------------
							 |
|---------------------------------------------------------------------|
| SSID | BSS Type | Channel | Security Status| BSSID | RSSI |Reserved |


|------|----------|---------|----------------|-------|------|---------|
|  33  |	 1	  |	  1		|		1		 |	  6	 |	 1	|	 1	  |
|---------------------------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
#ifndef CONNECT_DIRECT
NMI_Sint32 host_int_get_site_survey_results(NMI_WFIDrvHandle hWFIDrv, 
											NMI_Uint8 ppu8RcvdSiteSurveyResults[][MAX_SURVEY_RESULT_FRAG_SIZE], 
											NMI_Uint32 u32MaxSiteSrvyFragLen)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID astrWIDList[2];
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	astrWIDList[0].u16WIDid = (NMI_Uint16)WID_SITE_SURVEY_RESULTS;
	astrWIDList[0].enuWIDtype = WID_STR;
	astrWIDList[0].ps8WidVal = ppu8RcvdSiteSurveyResults[0];
	astrWIDList[0].s32ValueSize = u32MaxSiteSrvyFragLen;
	
	astrWIDList[1].u16WIDid = (NMI_Uint16)WID_SITE_SURVEY_RESULTS;
	astrWIDList[1].enuWIDtype = WID_STR;
	astrWIDList[1].ps8WidVal = ppu8RcvdSiteSurveyResults[1];
	astrWIDList[1].s32ValueSize = u32MaxSiteSrvyFragLen;		

	s32Error = SendConfigPkt(GET_CFG, astrWIDList, 2, NMI_TRUE);
		
	/*get the value by searching the local copy*/
	if(s32Error)
	{
		PRINT_ER("Failed to send config packet to get survey results\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}

	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}
#endif

/**
*  @brief 		sets a start scan request
*  @details 	 	
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Scan Source one of the following values
				DEFAULT_SCAN        0									
				USER_SCAN           BIT0								
				OBSS_PERIODIC_SCAN  BIT1								
				OBSS_ONETIME_SCAN   BIT2	
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_start_scan_req(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 scanSource)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_START_SCAN_REQ;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8*)&scanSource;
	strWID.s32ValueSize = sizeof(NMI_Char);
	
	return s32Error;
}

/**
*  @brief 			host_int_get_start_scan_req
*  @details 	 	gets a start scan request
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Scan Source one of the following values
				DEFAULT_SCAN        0									
				USER_SCAN           BIT0								
				OBSS_PERIODIC_SCAN  BIT1								
				OBSS_ONETIME_SCAN   BIT2	
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/ 

NMI_Sint32 host_int_get_start_scan_req(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8* pu8ScanSource)
 {	
	 NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_START_SCAN_REQ;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8*)pu8ScanSource;
	strWID.s32ValueSize = sizeof(NMI_Char);
	
	return s32Error;
 }

/**
*  @brief 			host_int_set_join_req
*  @details 	 	sets a join request	
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Index of the bss descriptor 	
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_join_req(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8bssid,
							     NMI_Uint8* pu8ssid, size_t ssidLen,
							     const NMI_Uint8* pu8IEs, size_t IEsLen,
							     tNMIpfConnectResult pfConnectResult, void* pvUserArg, 
							     NMI_Uint8 u8security, AUTHTYPE_T tenuAuth_type, 
							     NMI_Uint8 u8channel)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tenuScanConnTimer enuScanConnTimer;	
	
	if(pstrWFIDrv == NMI_NULL || pfConnectResult == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	if(gWFiDrvHandle == NULL)
	{
		PRINT_ER("Driver not initialized: gWFiDrvHandle = NULL\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	

	if(gWFiDrvHandle->strNMI_UsrScanReq.u32RcvdChCount == 0)
	{
		PRINT_ER("No scan results exist: Scanning should be done\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
	
	/* prepare the Connect Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_CONNECT;

	strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.u8security = u8security;
	strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.tenuAuth_type = tenuAuth_type;
	strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.u8channel = u8channel;
	strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pfConnectResult = pfConnectResult;
	strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pvUserArg = pvUserArg;

	if(pu8bssid != NULL)
	{
		strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8bssid = (NMI_Uint8*)NMI_MALLOC(6); /* will be deallocated 
																							    by the receiving 
																							    thread */
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8bssid, 
					  pu8bssid, 6);
	}

	if(pu8ssid != NULL)
	{
		strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.ssidLen = ssidLen;
		strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8ssid = (NMI_Uint8*)NMI_MALLOC(ssidLen); /* will be deallocated 
																									by the receiving 
																									thread */
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8ssid, 

				  	  pu8ssid, ssidLen);
	}

	if(pu8IEs != NULL)
	{
		strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.IEsLen = IEsLen;
		strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8IEs = (NMI_Uint8*)NMI_MALLOC(IEsLen); /* will be deallocated 
																								    by the receiving 
																								    thread */
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFconnectAttr.pu8IEs, 
				  	  pu8IEs, IEsLen);
	}

	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		PRINT_ER("Failed to send message queue: Set join request\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}

	enuScanConnTimer = CONNECT_TIMER;	
	NMI_TimerStart(&hConnectTimer, HOST_IF_CONNECT_TIMEOUT, (void*)enuScanConnTimer, NMI_NULL);
	
	NMI_CATCH(s32Error)
	{

	}

	return s32Error;
}

/**
*  @brief 			host_int_disconnect
*  @details 	 	disconnects from the currently associated network
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Reason Code of the Disconnection
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_disconnect(NMI_WFIDrvHandle hWFIDrv, NMI_Uint16 u16ReasonCode)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrHostIFmsg strHostIFmsg;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
				
	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	if(gWFiDrvHandle == NULL)
	{
		PRINT_ER("gWFiDrvHandle = NULL\n");
		NMI_ERRORREPORT(s32Error, NMI_FAIL);
	}	
		
	/* prepare the Disconnect Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_DISCONNECT;
	
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Failed to send message queue: disconnect\n");
	////////////////
	NMI_SemaphoreAcquire(&hSemTestDisconnectBlock,NULL);	
	/////////
	
	NMI_CATCH(s32Error)
	{

	}
	
	return s32Error;
}

/**
*  @brief 		host_int_disconnect_station
*  @details 	 disconnects a sta	
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Association Id of the station to be disconnected	
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_disconnect_station(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 assoc_id)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_DISCONNECT;
	strWID.enuWIDtype = WID_CHAR;
	strWID.ps8WidVal = (NMI_Sint8*)&assoc_id;
	strWID.s32ValueSize = sizeof(NMI_Char);
	
	return s32Error;
}

/**
*  @brief 			host_int_get_assoc_req_info
*  @details 	 	gets a Association request info
*  @param[in,out] handle to the wifi driver,
				Message containg assoc. req info in the following format
------------------------------------------------------------------------
|                        Management Frame Format                    |
|-------------------------------------------------------------------| 
|Frame Control|Duration|DA|SA|BSSID|Sequence Control|Frame Body|FCS |
|-------------|--------|--|--|-----|----------------|----------|----|
| 2           |2       |6 |6 |6    |		2       |0 - 2312  | 4  |
|-------------------------------------------------------------------|
|                                                                   |
|             Association Request Frame - Frame Body                |
|-------------------------------------------------------------------|
| Capability Information | Listen Interval | SSID | Supported Rates |
|------------------------|-----------------|------|-----------------|
|			2            |		 2         | 2-34 |		3-10        |
| ---------------------------------------------------------------------
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/

NMI_Sint32 host_int_get_assoc_req_info(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8AssocReqInfo, 
										    NMI_Uint32 u32AssocReqInfoLen)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_ASSOC_REQ_INFO;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = pu8AssocReqInfo;
	strWID.s32ValueSize = u32AssocReqInfoLen;

	
	return s32Error;
}

/**
*  @brief 		gets a Association Response info
*  @details 	 	
*  @param[in,out] handle to the wifi driver,
				Message containg assoc. resp info 
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_assoc_res_info(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8AssocRespInfo, 
										    NMI_Uint32 u32MaxAssocRespInfoLen, NMI_Uint32* pu32RcvdAssocRespInfoLen)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	strWID.u16WIDid = (NMI_Uint16)WID_ASSOC_RES_INFO;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = pu8AssocRespInfo;
	strWID.s32ValueSize = u32MaxAssocRespInfoLen;

	
	/* Sending Configuration packet */
	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error)
	{
		PRINT_ER("Failed to send association response config packet\n");
		*pu32RcvdAssocRespInfoLen = 0;
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	else
	{
		*pu32RcvdAssocRespInfoLen = strWID.s32ValueSize;
	}

	NMI_CATCH(s32Error)
	{
	
	}		
	return s32Error;
}

/**
*  @brief 		gets a Association Response info
*  @details 	Valid only in STA mode. This function gives the RSSI	
				values observed in all the channels at the time of scanning.
				The length of the field is 1 greater that the total number of 
				channels supported. Byte 0 contains the number of channels while
				each of Byte N contains	the observed RSSI value for the channel index N.							
*  @param[in,out] handle to the wifi driver,
				array of scanned channels' RSSI 
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_rx_power_level(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8RxPowerLevel, 
										   NMI_Uint32 u32RxPowerLevelLen)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID strWID;
	//tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	strWID.u16WIDid = (NMI_Uint16)WID_RX_POWER_LEVEL;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = pu8RxPowerLevel;
	strWID.s32ValueSize = u32RxPowerLevelLen;

	
	return s32Error;
}

/**
*  @brief 		sets a channel
*  @details 	 	
*  @param[in,out] handle to the wifi driver, 
*  @param[in]	Index of the channel to be set		
|-------------------------------------------------------------------| 
|          CHANNEL1      CHANNEL2 ....		             CHANNEL14	|
|  Input:         1             2					            14	|
|-------------------------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_set_mac_chnl_num(NMI_WFIDrvHandle hWFIDrv,NMI_Uint8 u8ChNum)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrWID	strWID;

	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	/*prepare configuration packet*/
	strWID.u16WIDid = (NMI_Uint16)WID_CURRENT_CHANNEL;
	strWID.enuWIDtype= WID_CHAR;
	strWID.ps8WidVal = (NMI_Char*)&u8ChNum;
	strWID.s32ValueSize = sizeof(NMI_Char);
	
	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error)
	{
		PRINT_ER("Failed to send set channel config packet\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;
}

/**
*  @brief 		gets the current channel index
*  @details 	 	
*  @param[in,out] handle to the wifi driver,
				current channel index
|-----------------------------------------------------------------------| 
|          CHANNEL1      CHANNEL2 ....                     CHANNEL14	|
|  Input:         1             2                                 14	|
|-----------------------------------------------------------------------|
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_host_chnl_num(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8ChNo)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}
	
	/* prepare the Get Channel Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_GET_CHNL;

	/* send the message */
	s32Error = 	NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Failed to send get host channel param's message queue ");
	NMI_SemaphoreAcquire(&hSemGetCHNL, NULL);
	//gu8Chnl = 11;

	*pu8ChNo = gu8Chnl;

	NMI_CATCH(s32Error)
	{}

	return s32Error;


}


/**
*  @brief 			 host_int_test_set_int_wid
*  @details 		 Test function for setting wids
*  @param[in,out]   NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32TestMemAddr
*  @return 		 Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_test_set_int_wid(NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32TestMemAddr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID	strWID;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;


	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	/*prepare configuration packet*/
	strWID.u16WIDid = (NMI_Uint16)WID_MEMORY_ADDRESS;
	strWID.enuWIDtype = WID_INT;
	strWID.ps8WidVal = (NMI_Char*)&u32TestMemAddr;	
	strWID.s32ValueSize = sizeof(NMI_Uint32);
	
	/*Sending Cfg*/
	s32Error = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Error)
	{
		PRINT_ER("Test Function: Failed to set wid value\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	else
	{
		PRINT_D(HOSTINF_DBG,"Successfully set wid value\n");

	}
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;
}

#ifdef NMI_AP_EXTERNAL_MLME
/**
*  @brief 		host_int_get_inactive_time
*  @details 	 	
*  @param[in,out] handle to the wifi driver,
				current sta macaddress, inactive_time
*  @return 		
*  @note 		
*  @author		
*  @date		
*  @version		1.0
*/
NMI_Sint32 host_int_get_inactive_time(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8 * mac,NMI_Uint32* pu32InactiveTime)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
 
	
       NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIfStaInactiveT.mac,
					  mac, ETH_ALEN);

	strHostIFmsg.u16MsgId = HOST_IF_MSG_GET_INACTIVETIME;
       
	/* send the message */
	s32Error = 	NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
		PRINT_ER("Failed to send get host channel param's message queue ");

	NMI_SemaphoreAcquire(&hSemInactiveTime, NULL);
	
	*pu32InactiveTime = gu32InactiveTime;

	NMI_CATCH(s32Error)
	{}

	return s32Error;
}
#endif
/**
*  @brief 		host_int_test_get_int_wid
*  @details 	Test function for getting wids
*  @param[in,out] NMI_WFIDrvHandle hWFIDrv, NMI_Uint32* pu32TestMemAddr
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_test_get_int_wid(NMI_WFIDrvHandle hWFIDrv, NMI_Uint32* pu32TestMemAddr)
{

	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrWID	strWID;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;	


	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}

	strWID.u16WIDid = (NMI_Uint16)WID_MEMORY_ADDRESS;
	strWID.enuWIDtype = WID_INT;
	strWID.ps8WidVal = (NMI_Sint8*)pu32TestMemAddr;
	strWID.s32ValueSize = sizeof(NMI_Uint32);
	
	s32Error = SendConfigPkt(GET_CFG, &strWID, 1, NMI_TRUE);
	/*get the value by searching the local copy*/
	if(s32Error)
	{
		PRINT_ER("Test Function: Failed to get wid value\n");
		NMI_ERRORREPORT(s32Error,NMI_INVALID_STATE);
	}
	else
	{
		PRINT_D(HOSTINF_DBG,"Successfully got wid value\n");

	}
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;
}


/**
*  @brief 		host_int_get_rssi
*  @details 	gets the currently maintained RSSI value for the station. 
				The received signal strength value in dB. 
				The range of valid values is -128 to 0.
*  @param[in,out] handle to the wifi driver,
				rssi value in dB
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_get_rssi(NMI_WFIDrvHandle hWFIDrv, NMI_Sint8* ps8Rssi)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrHostIFmsg strHostIFmsg;

	
	/* prepare the Get RSSI Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_GET_RSSI;

	/* send the message */
	s32Error = 	NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error){
		PRINT_ER("Failed to send get host channel param's message queue ");
		return NMI_FAIL;
		}

	NMI_SemaphoreAcquire(&hSemGetRSSI, NULL);
	
	
	if(ps8Rssi == NULL)
	{
		PRINT_ER("RSS pointer value is null");
		return NMI_FAIL;
	}


	*ps8Rssi = gs8Rssi;

	
	return s32Error;
}

/**
*  @brief 		host_int_scan
*  @details 	scans a set of channels
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Scan source
				Scan Type	PASSIVE_SCAN = 0,							
							ACTIVE_SCAN  = 1
				Channels Array
				Channels Array length
				Scan Callback function
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/ 
NMI_Sint32 host_int_scan(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8 u8ScanSource,
					     NMI_Uint8 u8ScanType, NMI_Uint8* pu8ChnlFreqList,
					     NMI_Uint8 u8ChnlListLen, const NMI_Uint8* pu8IEs,
					     size_t IEsLen, tNMIpfScanResult ScanResult,
					     void* pvUserArg,tstrHiddenNetwork  *pstrHiddenNetwork)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tenuScanConnTimer enuScanConnTimer;
	
	if(pstrWFIDrv == NMI_NULL || ScanResult == NMI_NULL)
	{		
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
							
		/* prepare the Scan Message */
		NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

		strHostIFmsg.u16MsgId = HOST_IF_MSG_SCAN;

		if(pstrHiddenNetwork != NULL)
		{
			strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.strHiddenNetwork.pstrHiddenNetworkInfo = pstrHiddenNetwork->pstrHiddenNetworkInfo;
			strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.strHiddenNetwork.u8ssidnum = pstrHiddenNetwork->u8ssidnum;
	
		}
		else
			NMI_PRINTF("pstrHiddenNetwork IS EQUAL TO NULL\n");


		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.u8ScanSource = u8ScanSource;
		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.u8ScanType = u8ScanType;
		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pfScanResult = ScanResult;
		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pvUserArg = pvUserArg;

		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.u8ChnlListLen = u8ChnlListLen;
		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pu8ChnlFreqList = (NMI_Uint8*)NMI_MALLOC(u8ChnlListLen); /* will be deallocated 
																										    by the receiving 
																										    thread */
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pu8ChnlFreqList, 
					  pu8ChnlFreqList, u8ChnlListLen); 

		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.IEsLen = IEsLen;
		strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pu8IEs = (NMI_Uint8*)NMI_MALLOC(IEsLen); /* will be deallocated 
																							by the receiving 
																				   		       thread */
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strHostIFscanAttr.pu8IEs, 
					  pu8IEs, IEsLen);	

		/* send the message */
		s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
		if(s32Error)
		{
			PRINT_ER("Error in sending message queue scanning parameters: Error(%d)\n",s32Error);
			NMI_ERRORREPORT(s32Error, NMI_FAIL);
		}

		enuScanConnTimer = SCAN_TIMER;	
		NMI_TimerStart(&hScanTimer, HOST_IF_SCAN_TIMEOUT, (void*)enuScanConnTimer, NMI_NULL);
		
	
	NMI_CATCH(s32Error)
	{

	}
	return s32Error;

}
/**
*  @brief 			hif_set_cfg
*  @details 		sets configuration wids values
*  @param[in,out] handle to the wifi driver,
*  @param[in]	WID, WID value				
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 hif_set_cfg(NMI_WFIDrvHandle hWFIDrv, tstrCfgParamVal * pstrCfgParamVal)
{
	
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	
	tstrHostIFmsg strHostIFmsg;
	

	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
	/* prepare the WiphyParams Message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
	strHostIFmsg.u16MsgId = HOST_IF_MSG_CFG_PARAMS;
	strHostIFmsg.uniHostIFmsgBody.strHostIFCfgParamAttr.pstrCfgParamVal = *pstrCfgParamVal;
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);

	NMI_CATCH(s32Error)
	{
	}

	return s32Error;

}


/**
*  @brief 		hif_get_cfg
*  @details 	gets configuration wids values
*  @param[in,out] handle to the wifi driver, 
				WID value
*  @param[in]	WID, 				
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah

*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 hif_get_cfg(NMI_WFIDrvHandle hWFIDrv,NMI_Uint16 u16WID,NMI_Uint16* pu16WID_Value)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;

	NMI_SemaphoreAcquire(&gtOsCfgValuesSem,NULL);
	
	if(pstrWFIDrv == NMI_NULL)
	{
		PRINT_ER("Driver not initialized: pstrWFIDrv = NULL \n");
		NMI_ERRORREPORT(s32Error, NMI_INVALID_ARGUMENT);
	}
	PRINT_D(HOSTINF_DBG,"Getting configuration parameters\n");
	switch(u16WID)
	{
	
		case WID_BSS_TYPE:
			  {
	
				  *pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.bss_type;
			 
			  }
			  break;
		case WID_AUTH_TYPE:
			  {

					*pu16WID_Value =(NMI_Uint16)gWFiDrvHandle->strCfgValues.auth_type;
		
			  }
			  break;
		case WID_AUTH_TIMEOUT:
			  {

				  *pu16WID_Value = gWFiDrvHandle->strCfgValues.auth_timeout;

			  }
			  break;
		
		case WID_POWER_MANAGEMENT:
			  {
	
				*pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.power_mgmt_mode;
			  }
			  break;
		case WID_SHORT_RETRY_LIMIT:
			  {
				 *pu16WID_Value =	gWFiDrvHandle->strCfgValues.short_retry_limit;
			  }
			  break;
		case WID_LONG_RETRY_LIMIT:
			  {
					*pu16WID_Value = gWFiDrvHandle->strCfgValues.long_retry_limit;
				
			  }
			  break;
		case WID_FRAG_THRESHOLD:
			  {
				  *pu16WID_Value = gWFiDrvHandle->strCfgValues.frag_threshold;
			  }
			  break;
		case WID_RTS_THRESHOLD:
			 {
				 *pu16WID_Value = gWFiDrvHandle->strCfgValues.rts_threshold;
			 }
			 break;
		case WID_PREAMBLE:
			 {
				*pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.preamble_type;
			  }
			  break;
		case WID_SHORT_SLOT_ALLOWED:
			  {
				  *pu16WID_Value =(NMI_Uint16) gWFiDrvHandle->strCfgValues.short_slot_allowed;
			  }
			  break;
		case WID_11N_TXOP_PROT_DISABLE:
			 {
				  *pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.txop_prot_disabled;
			 }
			 break;
		case WID_BEACON_INTERVAL:
			  {
				*pu16WID_Value = gWFiDrvHandle->strCfgValues.beacon_interval;
			  }
			  break;
		case WID_DTIM_PERIOD:
			  {
				*pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.dtim_period;
			  }
			  break;
		case WID_SITE_SURVEY:
			{	
				*pu16WID_Value = (NMI_Uint16)gWFiDrvHandle->strCfgValues.site_survey_enabled;
			}
			break;
		case WID_SITE_SURVEY_SCAN_TIME:
			  {
				*pu16WID_Value = gWFiDrvHandle->strCfgValues.site_survey_scan_time;
			  }
			  break;

		case WID_ACTIVE_SCAN_TIME:
			  {
	
					*pu16WID_Value= gWFiDrvHandle->strCfgValues.active_scan_time;
			  }
			  break;
		case WID_PASSIVE_SCAN_TIME:
			  {
				*pu16WID_Value = gWFiDrvHandle->strCfgValues.passive_scan_time;
			  
			  }
			  break;
		case WID_CURRENT_TX_RATE:
			  {
					*pu16WID_Value = gWFiDrvHandle->strCfgValues.curr_tx_rate;
			  }
			  break;
		default:
			break;
		}

		NMI_SemaphoreRelease(&gtOsCfgValuesSem, NULL);

	NMI_CATCH(s32Error)
	{
	}
	return s32Error;

}

/*****************************************************************************/
/*							Notification Functions							 */
/*****************************************************************************/
/**
*  @brief 		notifies host with join and leave requests
*  @details 	This function prepares an Information frame having the
				information about a joining/leaving station.
*  @param[in,out] handle to the wifi driver,
*  @param[in]	6 byte Sta Adress
				Join or leave flag:
				Join = 1,
				Leave =0
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
void host_int_send_join_leave_info_to_host
						(NMI_Uint16 assocId, NMI_Uint8* stationAddr, NMI_Bool joining)
{
}
/**
*  @brief 		notifies host with stations found in scan
*  @details 	sends the beacon/probe response from scan
*  @param[in,out] handle to the wifi driver,
*  @param[in]	Sta Address,
				Frame length,
				Rssi of the Station found
*  @return 		Error code indicating success/failure
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/

void host_int_send_network_info_to_host
			(NMI_Uint8 *macStartAddress,NMI_Uint16 u16RxFrameLen, NMI_Sint8 s8Rssi)
{
}
/**
*  @brief 		host_int_init
*  @details 	host interface initialization function
*  @param[in,out] handle to the wifi driver,
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
static NMI_Uint32 u32Intialized = 0;
NMI_Sint32 host_int_init(NMI_WFIDrvHandle* phWFIDrv)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv;
	tstrNMI_SemaphoreAttrs strSemaphoreAttrs;

	if(u32Intialized == 1)
	{
		PRINT_D(HOSTINF_DBG,"Host interface is previously initialized\n");
		*phWFIDrv = (NMI_WFIDrvHandle)gWFiDrvHandle;
		return 0;
	}	
	PRINT_D(HOSTINF_DBG,"Initializing host interface\n");

	gbScanWhileConnected = NMI_FALSE;	

	NMI_SemaphoreFillDefault(&strSemaphoreAttrs);	

	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemHostIFthrdEnd, &strSemaphoreAttrs);
	
	/////////////////////////////////////////
	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemTestKeyBlock, &strSemaphoreAttrs);
	/////////////////////////////////////////

	/////////////////////////////////////////
	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemTestDisconnectBlock, &strSemaphoreAttrs);
	/////////////////////////////////////////

	/////////////////////////////////////////
	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemGetRSSI, &strSemaphoreAttrs);
	/////////////////////////////////////////
	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemGetCHNL, &strSemaphoreAttrs);	

	/////////////////////////////////////////
	strSemaphoreAttrs.u32InitCount = 0;
	NMI_SemaphoreCreate(&hSemInactiveTime, &strSemaphoreAttrs);	
       
	
	s32Error = NMI_MsgQueueCreate(&gMsgQHostIF, NMI_NULL);
	if(s32Error < 0){
		PRINT_ER("Failed to creat MQ\n");
		goto _fail_;
		}
		
	s32Error = NMI_ThreadCreate(&HostIFthreadHandler,hostIFthread, NMI_NULL, NMI_NULL);
	if(s32Error < 0){
		PRINT_ER("Failed to creat Thread\n");
		goto _fail_mq_;
		}
	s32Error = NMI_TimerCreate(&hScanTimer, TimerCB, NMI_NULL);
	if(s32Error < 0){
		PRINT_ER("Failed to creat Timer\n");
		goto _fail_thread_;
		}

	s32Error = NMI_TimerCreate(&hConnectTimer, TimerCB, NMI_NULL);
	if(s32Error < 0){
		PRINT_ER("Failed to creat Timer\n");
		goto _fail_timer_1;
		}

				
	NMI_SemaphoreCreate(&gtOsCfgValuesSem,NULL);
	NMI_SemaphoreAcquire(&gtOsCfgValuesSem,NULL);
	
	/*Allocate host interface private structure*/
	pstrWFIDrv  = (tstrNMI_WFIDrv*)NMI_MALLOC(sizeof(tstrNMI_WFIDrv));
	if(pstrWFIDrv == NMI_NULL)
	{
		//NMI_ERRORREPORT(s32Error,NMI_NO_MEM);
		s32Error = NMI_NO_MEM;
		PRINT_ER("Failed to allocate memory\n");
		goto _fail_timer_2;
	}
	NMI_memset(pstrWFIDrv,0,sizeof(tstrNMI_WFIDrv));
	/*return driver handle to user*/
	*phWFIDrv = (NMI_WFIDrvHandle)pstrWFIDrv;
	/*save into globl handle*/
	gWFiDrvHandle = pstrWFIDrv;
	
	PRINT_D(HOSTINF_DBG,"Global handle pointer value=%x\n",(NMI_Uint32)gWFiDrvHandle);

#ifdef SIMULATION
	TransportInit();
#endif
	
	gWFiDrvHandle->enuHostIFstate = HOST_IF_IDLE;
	//gWFiDrvHandle->bPendingConnRequest = NMI_FALSE;

	/*Initialize CFG WIDS Defualt Values*/
	gWFiDrvHandle->strCfgValues.site_survey_enabled = SITE_SURVEY_OFF;
	gWFiDrvHandle->strCfgValues.scan_source = DEFAULT_SCAN;
	gWFiDrvHandle->strCfgValues.active_scan_time = ACTIVE_SCAN_TIME;
	gWFiDrvHandle->strCfgValues.passive_scan_time= PASSIVE_SCAN_TIME;
	gWFiDrvHandle->strCfgValues.curr_tx_rate = AUTORATE;
	
	PRINT_INFO(HOSTINF_DBG,"Initialization values, Site survey value: %d\n Scan source: %d\n Active scan time: %d\n Passive scan time: %d\nCurrent tx Rate = %d\n",
			gWFiDrvHandle->strCfgValues.site_survey_enabled,gWFiDrvHandle->strCfgValues.scan_source,
			gWFiDrvHandle->strCfgValues.active_scan_time,gWFiDrvHandle->strCfgValues.passive_scan_time,
			gWFiDrvHandle->strCfgValues.curr_tx_rate);

	NMI_SemaphoreRelease(&gtOsCfgValuesSem,NULL);
	
	/*TODO Code to setup simulation to be removed later*/
	/*Intialize configurator module*/
	s32Error = CoreConfiguratorInit();
	if(s32Error < 0){
		PRINT_ER("Failed to initialize core configurator\n");
		goto _fail_mem_;
		}

#ifdef SIMULATION
	/*Initialize Simulaor*/
	CoreConfigSimulatorInit();
#endif
	u32Intialized = 1;
	return s32Error;

	
_fail_mem_:
	if(pstrWFIDrv != NMI_NULL)
			NMI_FREE(pstrWFIDrv);
_fail_timer_2:
	NMI_SemaphoreRelease(&gtOsCfgValuesSem,NULL);
	NMI_TimerDestroy(&hConnectTimer,NMI_NULL);
_fail_timer_1:
	NMI_TimerDestroy(&hScanTimer,NMI_NULL);
_fail_thread_:
	NMI_ThreadDestroy(&HostIFthreadHandler, NMI_NULL);
_fail_mq_:
	NMI_MsgQueueDestroy(&gMsgQHostIF, NMI_NULL);
_fail_:
	return s32Error;
	

}
/**
*  @brief 		host_int_deinit
*  @details 	host interface initialization function
*  @param[in,out] handle to the wifi driver,
*  @note 		
*  @author		zsalah
*  @date		8 March 2012
*  @version		1.0
*/
NMI_Sint32 host_int_deinit(NMI_WFIDrvHandle hWFIDrv)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrHostIFmsg strHostIFmsg;
#ifdef NMI_PARSE_SCAN_IN_HOST
	tstrJoinBssParam * ptstrJoinBssParam;
#endif
	/*obtain driver handle*/
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	if(u32Intialized == 0)
	{
		PRINT_ER("Host Interface is not initialized\n");
		return 0;
	}
	PRINT_D(HOSTINF_DBG,"De-initializing host interface\n");
	

	/*Calling the CFG80211 scan done function with the abort flag set to true*/
	if(gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult)
       {                       
        	gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult(SCAN_EVENT_ABORTED, NMI_NULL,
				gWFiDrvHandle->strNMI_UsrScanReq.u32UserScanPvoid);
              
    		gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult= NMI_NULL;              
       }
	/*deinit configurator and simulator*/
#ifdef SIMULATION
	CoreConfigSimulatorDeInit();
#endif
	CoreConfiguratorDeInit();
#ifdef SIMULATION
	TransportDeInit();
#endif

	if(NMI_TimerDestroy(&hScanTimer, NMI_NULL)){
		printk(">> Scan timer is active \n");
		msleep(HOST_IF_SCAN_TIMEOUT+1000);
	}
	
	if(NMI_TimerDestroy(&hConnectTimer, NMI_NULL)){
		printk(">> Connect timer is active \n");
		msleep(HOST_IF_CONNECT_TIMEOUT+1000);
	}
	
	pstrWFIDrv->enuHostIFstate = HOST_IF_IDLE;	
	
	gbScanWhileConnected = NMI_FALSE;	

	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
	
       
	strHostIFmsg.u16MsgId = HOST_IF_MSG_EXIT;
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error != NMI_SUCCESS)
	{
		PRINT_ER("Error in sending deinit's message queue message function: Error(%d)\n",s32Error);
	}
	
	NMI_SemaphoreAcquire(&hSemHostIFthrdEnd, NULL);

	
	//PRINT_D(HOSTINF_DBG,"Thread Destroy %p\n",&HostIFthreadHandler);
	//s32Error = NMI_ThreadDestroy(&HostIFthreadHandler, NMI_NULL);
	//NMI_ERRORCHECK(s32Error);

	if(pstrWFIDrv != NMI_NULL)
	{		
		NMI_FREE(pstrWFIDrv);
	}
	/*Bug4218: Parsing Join Param*/
	//cleaning JoinTable
	#ifdef NMI_PARSE_SCAN_IN_HOST	
	while(gtstrBssTable.u8noBssEntries!=0)
	{
		//PRINT_D(HOSTINF_DBG,"Freeing Join Parameters Table\n");
		ptstrJoinBssParam = gtstrBssTable.head;
		gtstrBssTable.head = (tstrJoinBssParam*)gtstrBssTable.head->nextJoinBss;
		NMI_FREE(ptstrJoinBssParam);
		gtstrBssTable.u8noBssEntries-=1;
	}
	#endif /*NMI_PARSE_SCAN_IN_HOST*/
	/* Destroy the MSG Queue */
	NMI_MsgQueueDestroy(&gMsgQHostIF, NMI_NULL);
	
	/*Destroy Semaphores*/	
	NMI_SemaphoreDestroy(&hSemHostIFthrdEnd,NULL);
	
	NMI_SemaphoreDestroy(&hSemTestKeyBlock,NULL);	
	NMI_SemaphoreDestroy(&hSemTestDisconnectBlock,NULL);	
	NMI_SemaphoreDestroy(&hSemGetRSSI,NULL);
	NMI_SemaphoreDestroy(&hSemGetCHNL,NULL);	
	NMI_SemaphoreDestroy(&hSemInactiveTime,NULL);	
	
	NMI_SemaphoreAcquire(&gtOsCfgValuesSem,NULL);
	NMI_SemaphoreDestroy(&gtOsCfgValuesSem,NULL);

	/*Setting the gloabl driver handler with NULL*/
	u32Intialized = 0;
	gWFiDrvHandle = NULL;
	

	

	return s32Error;
}


/**
*  @brief 		NetworkInfoReceived
*  @details 	function to to be called when network info packet is received
*  @param[in]	pu8Buffer the received packet
*  @param[in]   u32Length  length of the received packet
*  @return 		none
*  @note 		
*  @author		
*  @date		1 Mar 2012
*  @version		1.0
*/
void NetworkInfoReceived(NMI_Uint8* pu8Buffer, NMI_Uint32 u32Length)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrHostIFmsg strHostIFmsg;	
	
	if(gWFiDrvHandle == NMI_NULL)
	{
		PRINT_ER("NetworkInfo received but driver not init[%x]\n",(NMI_Uint32)gWFiDrvHandle);
		return;
	}
	
	/* prepare the Asynchronous Network Info message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_RCVD_NTWRK_INFO;

	strHostIFmsg.uniHostIFmsgBody.strRcvdNetworkInfo.u32Length = u32Length;
	strHostIFmsg.uniHostIFmsgBody.strRcvdNetworkInfo.pu8Buffer = (NMI_Uint8*)NMI_MALLOC(u32Length); /* will be deallocated 
																							 	by the receiving 
																								thread */
	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strRcvdNetworkInfo.pu8Buffer, 
			  	  pu8Buffer, u32Length); 
	
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);	
	if(s32Error)
	{
		PRINT_ER("Error in sending network info message queue message parameters: Error(%d)\n",s32Error);
	}
	
	return;
}

/**
*  @brief 		GnrlAsyncInfoReceived
*  @details 	function to be called when general Asynchronous info packet is received
*  @param[in]	pu8Buffer the received packet
*  @param[in]   u32Length  length of the received packet
*  @return 		none
*  @note 		
*  @author		
*  @date		15 Mar 2012
*  @version		1.0
*/
void GnrlAsyncInfoReceived(NMI_Uint8* pu8Buffer, NMI_Uint32 u32Length)
{	
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrHostIFmsg strHostIFmsg;

	PRINT_D(HOSTINF_DBG,"General asynchronous info packet received \n");
	
	if(gWFiDrvHandle == NULL)
	{
		PRINT_ER("Wifi driver handler is equal to NULL\n");
		return;
	}
	
	if(gWFiDrvHandle->strNMI_UsrConnReq.pfUserConnectResult == NMI_NULL)
	{
		/* received mac status is not needed when there is no current Connect Request */
		return;
	}

	/* prepare the General Asynchronous Info message */
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	strHostIFmsg.u16MsgId = HOST_IF_MSG_RCVD_GNRL_ASYNC_INFO;

	strHostIFmsg.uniHostIFmsgBody.strRcvdGnrlAsyncInfo.u32Length = u32Length;
	strHostIFmsg.uniHostIFmsgBody.strRcvdGnrlAsyncInfo.pu8Buffer = (NMI_Uint8*)NMI_MALLOC(u32Length); /* will be deallocated 
																							 	by the receiving 
																								thread */
	NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strRcvdGnrlAsyncInfo.pu8Buffer, 
			  	  pu8Buffer, u32Length); 
	
	/* send the message */
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		PRINT_ER("Error in sending message queue asynchronous message info: Error(%d)\n",s32Error);
	}
	
	return;
}

/**
*  @brief host_int_ScanCompleteReceived
*  @details 	    Setting scan complete received notifcation in message queue 
*  @param[in]     NMI_Uint8* pu8Buffer, NMI_Uint32 u32Length
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
void host_int_ScanCompleteReceived(NMI_Uint8* pu8Buffer, NMI_Uint32 u32Length)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;	
	tstrHostIFmsg strHostIFmsg;
	
	PRINT_D(GENERIC_DBG,"Scan notification received \n");

	if(gWFiDrvHandle == NULL)
	{
		return;
	}
	/*if there is an ongoing scan request*/	
	if(gWFiDrvHandle->strNMI_UsrScanReq.pfUserScanResult)
	{
		/* prepare theScan Done message */
		NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

		strHostIFmsg.u16MsgId = HOST_IF_MSG_RCVD_SCAN_COMPLETE;


		/* will be deallocated by the receiving thread */
		/*no need to send message body*/

		/*strHostIFmsg.uniHostIFmsgBody.strScanComplete.u32Length = u32Length;
		strHostIFmsg.uniHostIFmsgBody.strScanComplete.pu8Buffer  = (NMI_Uint8*)NMI_MALLOC(u32Length); 
		NMI_memcpy(strHostIFmsg.uniHostIFmsgBody.strScanComplete.pu8Buffer, 
				  	  pu8Buffer, u32Length); */

		/* send the message */
		s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
		if(s32Error)
		{
			PRINT_ER("Error in sending message queue scan complete parameters: Error(%d)\n",s32Error);
		}
	}	
	
	
	return;

}

#ifdef NMI_AP_EXTERNAL_MLME
/**
*  @brief host_int_add_beacon
*  @details 	   Setting add beacon params in message queue 
*  @param[in]    NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32Interval,
			   NMI_Uint32 u32DTIMPeriod,NMI_Uint32 u32HeadLen, NMI_Uint8* pu8Head,
			   NMI_Uint32 u32TailLen, NMI_Uint8* pu8Tail
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_int_add_beacon(NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32Interval,
									 NMI_Uint32 u32DTIMPeriod,
									 NMI_Uint32 u32HeadLen, NMI_Uint8* pu8Head,
									 NMI_Uint32 u32TailLen, NMI_Uint8* pu8Tail)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tstrHostIFSetBeacon* pstrSetBeaconParam = &strHostIFmsg.uniHostIFmsgBody.strHostIFSetBeacon;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	PRINT_D(HOSTINF_DBG,"Setting adding beacon message queue params\n");
	
	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_ADD_BEACON;
	pstrSetBeaconParam->u32Interval = u32Interval;
	pstrSetBeaconParam->u32DTIMPeriod= u32DTIMPeriod;
	pstrSetBeaconParam->u32HeadLen = u32HeadLen;
	pstrSetBeaconParam->pu8Head = (NMI_Uint8*)NMI_MALLOC(u32HeadLen);
	if(pstrSetBeaconParam->pu8Head == NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_NO_MEM);
	}
	NMI_memcpy(pstrSetBeaconParam->pu8Head, pu8Head, u32HeadLen);
	pstrSetBeaconParam->u32TailLen = u32TailLen;
	
	/* Bug 4599 : if tail length = 0 skip allocating & copying */ 
	if(u32TailLen > 0)
	{
		pstrSetBeaconParam->pu8Tail = (NMI_Uint8*)NMI_MALLOC(u32TailLen);
		if(pstrSetBeaconParam->pu8Tail== NULL)
		{
			NMI_ERRORREPORT(s32Error,NMI_NO_MEM);
		}
		NMI_memcpy(pstrSetBeaconParam->pu8Tail, pu8Tail, u32TailLen);
	}
	else
	{
		pstrSetBeaconParam->pu8Tail=NULL;
	}
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		NMI_ERRORREPORT(s32Error, s32Error);
	}

	NMI_CATCH(s32Error)
	{
		if(pstrSetBeaconParam->pu8Head != NULL)
		{
			NMI_FREE(pstrSetBeaconParam->pu8Head);
		}
		
		if(pstrSetBeaconParam->pu8Tail != NULL)
		{
			NMI_FREE(pstrSetBeaconParam->pu8Tail);
		}
	}

	return s32Error;

}


/**
*  @brief host_int_del_beacon
*  @details 	   Setting add beacon params in message queue 
*  @param[in]    NMI_WFIDrvHandle hWFIDrv
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_int_del_beacon(NMI_WFIDrvHandle hWFIDrv)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}
	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_DEL_BEACON;
	
	PRINT_D(HOSTINF_DBG,"Setting deleting beacon message queue params\n");
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	NMI_ERRORCHECK(s32Error);

	NMI_CATCH(s32Error)
	{
	}
	return s32Error;
}


/**
*  @brief host_int_add_station
*  @details 	   Setting add station params in message queue 
*  @param[in]    NMI_WFIDrvHandle hWFIDrv, tstrNMI_AddStaParam* pstrStaParams
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_int_add_station(NMI_WFIDrvHandle hWFIDrv, tstrNMI_AddStaParam* pstrStaParams)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tstrNMI_AddStaParam* pstrAddStationMsg = &strHostIFmsg.uniHostIFmsgBody.strAddStaParam;
	

	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	PRINT_D(HOSTINF_DBG,"Setting adding station message queue params\n");

	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_ADD_STATION;

	NMI_memcpy(pstrAddStationMsg , pstrStaParams, sizeof(tstrNMI_AddStaParam));
	if(pstrAddStationMsg->u8NumRates>0)
	{
		pstrAddStationMsg->pu8Rates = NMI_MALLOC(pstrAddStationMsg->u8NumRates);
		NMI_NULLCHECK(s32Error, pstrAddStationMsg->pu8Rates);

		NMI_memcpy(pstrAddStationMsg->pu8Rates, pstrStaParams->pu8Rates, pstrAddStationMsg->u8NumRates);	
	}
	
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		NMI_ERRORREPORT(s32Error, s32Error);
	}

	NMI_CATCH(s32Error)
		{
		}
	return s32Error;
}

/**
*  @brief host_int_del_station
*  @details 	   Setting delete station params in message queue 
*  @param[in]    NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8MacAddr
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_int_del_station(NMI_WFIDrvHandle hWFIDrv, NMI_Uint8* pu8MacAddr)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tstrHostIFDelSta* pstrDelStationMsg = &strHostIFmsg.uniHostIFmsgBody.strDelStaParam;
	
	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));

	PRINT_D(HOSTINF_DBG,"Setting deleting station message queue params\n");

	
	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_DEL_STATION;

	NMI_memcpy(pstrDelStationMsg->au8MacAddr, pu8MacAddr, ETH_ALEN);
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		NMI_ERRORREPORT(s32Error, s32Error);
	}

	NMI_CATCH(s32Error)
		{
		}
	return s32Error;
}

/**
*  @brief host_int_edit_station
*  @details 	   Setting edit station params in message queue 
*  @param[in]    NMI_WFIDrvHandle hWFIDrv, tstrNMI_AddStaParam* pstrStaParams
*  @return 	    Error code.
*  @author	
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_int_edit_station(NMI_WFIDrvHandle hWFIDrv, tstrNMI_AddStaParam* pstrStaParams)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tstrNMI_AddStaParam* pstrAddStationMsg = &strHostIFmsg.uniHostIFmsgBody.strAddStaParam;

	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	PRINT_D(HOSTINF_DBG,"Setting editing station message queue params\n");
	
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
	
	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_EDIT_STATION;

	NMI_memcpy(pstrAddStationMsg , pstrStaParams, sizeof(tstrNMI_AddStaParam));
	if(pstrAddStationMsg->u8NumRates>0)
	{
		pstrAddStationMsg->pu8Rates = NMI_MALLOC(pstrAddStationMsg->u8NumRates);
		NMI_memcpy(pstrAddStationMsg->pu8Rates, pstrStaParams->pu8Rates, pstrAddStationMsg->u8NumRates);
		NMI_NULLCHECK(s32Error, pstrAddStationMsg->pu8Rates);
	}
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		NMI_ERRORREPORT(s32Error, s32Error);
	}
	NMI_CATCH(s32Error)
			{
			}
	return s32Error;
}
#endif /*NMI_AP_EXTERNAL_MLME*/
uint32_t nmi_get_chipid(uint8_t);

NMI_Sint32 host_int_set_power_mgmt(NMI_WFIDrvHandle hWFIDrv, NMI_Bool bIsEnabled, NMI_Uint32 u32Timeout)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;

/* Set power management only for platforms with RTC
The internal clock has a bug in NMc1000CA*/

	tstrNMI_WFIDrv * pstrWFIDrv = (tstrNMI_WFIDrv *)hWFIDrv;
	tstrHostIFmsg strHostIFmsg;
	tstrHostIfPowerMgmtParam* pstrPowerMgmtParam = &strHostIFmsg.uniHostIFmsgBody.strPowerMgmtparam;

	// Disable PS with muxed sdio for D0 for DMA version 2, cause it had a bug with DMA ver 2 with the interrupt registers that was fixed in F0
#if (defined NMI_SDIO) && (!defined USE_DMA_VER_1) && (!defined NMI_SDIO_IRQ_GPIO)
	if(nmi_get_chipid(NMI_FALSE) == 0x1000d0)
	{
		return s32Error;
	}
#endif

	// F0 boards requires the 1.4 VCO supply. Otherwise, disable PS
#ifndef VCO_14_SUPPLY
	if(nmi_get_chipid(NMI_FALSE) < 0x1002a0)
	{
		return s32Error;
	}
#endif //VCO_14_SUPPLY

	if(pstrWFIDrv == NMI_NULL)
	{
		NMI_ERRORREPORT(s32Error,NMI_INVALID_ARGUMENT);
	}

	PRINT_D(HOSTINF_DBG,"Setting Power management message queue params\n");
	
	NMI_memset(&strHostIFmsg, 0, sizeof(tstrHostIFmsg));
	
	
	/* prepare the WiphyParams Message */
	strHostIFmsg.u16MsgId = HOST_IF_MSG_POWER_MGMT;

	pstrPowerMgmtParam->bIsEnabled = bIsEnabled;
	pstrPowerMgmtParam->u32Timeout = u32Timeout;
	
	s32Error = NMI_MsgQueueSend(&gMsgQHostIF, &strHostIFmsg, sizeof(tstrHostIFmsg), NMI_NULL);
	if(s32Error)
	{
		NMI_ERRORREPORT(s32Error, s32Error);
	}
	NMI_CATCH(s32Error)
	{
	}
	return s32Error;
}


/*Bug4218: Parsing Join Param*/
#ifdef NMI_PARSE_SCAN_IN_HOST	
/*Bug4218: Parsing Join Param*/
/**
*  @brief 		host_int_addToJoinBssTable
*  @details 	   	Add Join Parameters to the end of BssTable (linked list)
*  @param[in]    	tstrJoinBssParam* pNewJoinBssParam
*  @return 	    	Error code.
*  @author		zsalah
*  @date	
*  @version		1.0**/
static NMI_Sint32 host_int_addToJoinBssTable(tstrJoinBssParam* pNewJoinBssParam)
{
	if(pNewJoinBssParam!=NULL)
	{
		//PRINT_D(HOSTINF_DBG,"Adding JOIN PARAMETERS\n");
		if (gtstrBssTable.u8noBssEntries==0)
		{	
			pNewJoinBssParam->nextJoinBss = NULL;
			gtstrBssTable.head = pNewJoinBssParam;
			gtstrBssTable.tail = pNewJoinBssParam;

		}
		else
		{	
			pNewJoinBssParam->nextJoinBss = NULL;
			gtstrBssTable.tail->nextJoinBss = (struct _tstrJoinParam*)pNewJoinBssParam;
			gtstrBssTable.tail = pNewJoinBssParam;
			
		}
		gtstrBssTable.u8noBssEntries+=1;
		PRINT_D(HOSTINF_DBG,"Added Entry to table, count= %d \n",gtstrBssTable.u8noBssEntries);
		return 0;
	}
	return -1;
}

/*Bug4218: Parsing Join Param*/
/**
*  @brief 		host_int_getJoinBssParam
*  @details 	   	get the JoinParam entry of a given ssid  from JoinParamTable 
*  @param[in]   	NMI_Uint8* ssid,NMI_Uint8 ssidLen
*  @return 	    	tstrJoinBssParam*
*  @author		zsalah
*  @date	
*  @version		1.0**/
tstrJoinBssParam* host_int_getJoinBssParam(NMI_Uint8* ssid,NMI_Uint8 ssidLen)
{
	tstrJoinBssParam* ptstrCurrBssParam;
	NMI_Uint8 u8noBssEntries;	
	int i;
	ptstrCurrBssParam = gtstrBssTable.head;
	u8noBssEntries = gtstrBssTable.u8noBssEntries;
	PRINT_D(HOSTINF_DBG,"NO of Enteries = %d\n",gtstrBssTable.u8noBssEntries);
	if(gtstrBssTable.u8noBssEntries==0)
		return NULL;
	for(i=0; i<ssidLen;i++)
		PRINT_D(HOSTINF_DBG,"%c",ssid[i]);
	while(!(u8noBssEntries==0))
	{
		
		if(NMI_memcmp(ptstrCurrBssParam->ssid,ssid,ssidLen)==0)
			{
				PRINT_D(HOSTINF_DBG,"\nFound Join Entry\n");
				return ptstrCurrBssParam;
			}
		else
			ptstrCurrBssParam = (tstrJoinBssParam*)ptstrCurrBssParam->nextJoinBss;
		u8noBssEntries--;
	}
	PRINT_D(HOSTINF_DBG,"Entry Not Foundin Table\n");
	return NULL;;
}
/*Bug4218: Parsing Join Param*/
/**
*  @brief 		host_int_addJoinBssParam
*  @details 	   	Parse Needed Join Parameters and save it in a new JoinBssParam entry
*  @param[in]   	tstrNetworkInfo* ptstrNetworkInfo
*  @return 	    	
*  @author		zsalah
*  @date	
*  @version		1.0**/
static void host_int_addJoinBssParam(tstrNetworkInfo* ptstrNetworkInfo)
{
	tstrJoinBssParam* pNewJoinBssParam=NULL;
	NMI_Uint8* pu8IEs;
	NMI_Uint16 u16IEsLen;
	NMI_Uint16 index = 0;
	NMI_Uint8 suppRatesNo;
	NMI_Uint8 extSuppRatesNo;
	NMI_Uint16 jumpOffset;
	NMI_Uint8 pcipherCount;
	NMI_Uint8 authCount;
	NMI_Uint8 i;

	pu8IEs = ptstrNetworkInfo->pu8IEs;
	u16IEsLen =ptstrNetworkInfo->u16IEsLen;
	
	pNewJoinBssParam = NMI_MALLOC(sizeof(tstrJoinBssParam));
	if(pNewJoinBssParam!=NULL)
	{
		NMI_memset(pNewJoinBssParam, 0, sizeof(tstrJoinBssParam));
		pNewJoinBssParam->dtim_period = ptstrNetworkInfo->u8DtimPeriod;
		pNewJoinBssParam->beacon_period = ptstrNetworkInfo->u16BeaconPeriod;
		pNewJoinBssParam->cap_info =ptstrNetworkInfo->u16CapInfo;
		NMI_memcpy(pNewJoinBssParam->au8bssid,ptstrNetworkInfo->au8bssid,6);
		/*for(i=0; i<6;i++)
			PRINT_D(HOSTINF_DBG,"%c",pNewJoinBssParam->au8bssid[i]);*/
		NMI_memcpy((NMI_Uint8*)pNewJoinBssParam->ssid,ptstrNetworkInfo->au8ssid,ptstrNetworkInfo->u8SsidLen+1);
		pNewJoinBssParam->ssidLen = ptstrNetworkInfo->u8SsidLen;
		/*for(i=0; i<pNewJoinBssParam->ssidLen;i++)
			PRINT_D(HOSTINF_DBG,"%c",pNewJoinBssParam->ssid[i]);*/

		//parse supported rates:
		while(index<u16IEsLen)
		{
			//supportedRates IE
			if (pu8IEs[index]== SUPP_RATES_IE)
			{
				//PRINT_D(HOSTINF_DBG, "Supported Rates\n");
				suppRatesNo = pu8IEs[index+1];
				pNewJoinBssParam->supp_rates[0] = suppRatesNo;
				index+=2;//skipping ID and length bytes;
				
				for(i=0; i<suppRatesNo; i++)
				{
					pNewJoinBssParam->supp_rates[i+1] = pu8IEs[index+i];
					//PRINT_D(HOSTINF_DBG,"%0x ",pNewJoinBssParam->supp_rates[i+1]);
				}
				printk("\n");
				index+=suppRatesNo;
				continue;
			}
			//Ext SupportedRates IE
			else if(pu8IEs[index]== EXT_SUPP_RATES_IE)
			{	
				//PRINT_D(HOSTINF_DBG, "Extended Supported Rates\n");
				//checking if no of ext. supp and supp rates < max limit
				extSuppRatesNo = pu8IEs[index+1];
				if(extSuppRatesNo > (MAX_RATES_SUPPORTED-suppRatesNo))
					pNewJoinBssParam->supp_rates[0] = MAX_RATES_SUPPORTED;
				else
					pNewJoinBssParam->supp_rates[0]+=extSuppRatesNo;
				index+=2;
				//pNewJoinBssParam.supp_rates[0] contains now old number not the ext. no
				for(i=0; i< (pNewJoinBssParam->supp_rates[0]-suppRatesNo); i++)
				{
					pNewJoinBssParam->supp_rates[suppRatesNo+i+1] = pu8IEs[index+i];
					//PRINT_D(HOSTINF_DBG,"%0x ",pNewJoinBssParam->supp_rates[suppRatesNo+i+1]);
				}
				//printk("\n");
				index+=extSuppRatesNo;
				continue;
			}

			//HT Cap. IE
			else if(pu8IEs[index]==HT_CAPABILITY_IE)
			{
				//if IE found set the flag
				pNewJoinBssParam->ht_capable = BTRUE;
				index+=pu8IEs[index+1]+2; //ID,Length bytes and IE body
				//PRINT_D(HOSTINF_DBG,"HT_CAPABALE\n");
				continue;
			}	
		    else if((pu8IEs[index] == WMM_IE) && /* WMM Element ID */
			       (pu8IEs[index+2] == 0x00) && (pu8IEs[index+3] == 0x50) && 
			       (pu8IEs[index+4] == 0xF2) && /* OUI */
			       (pu8IEs[index+5] == 0x02) && /* OUI Type     */
			       ((pu8IEs[index+6] == 0x00) || (pu8IEs[index+6] == 0x01)) && /* OUI Sub Type */
			       (pu8IEs[index+7] == 0x01))
		      {
		            /* Presence of WMM Info/Param element indicates WMM capability */
		            pNewJoinBssParam->wmm_cap = BTRUE;

		            /* Check if Bit 7 is set indicating U-APSD capability */
		            if(pu8IEs[index + 8] & (1 << 7))
		            {
		                pNewJoinBssParam->uapsd_cap = BTRUE;
		            }
				index += pu8IEs[index + 1]+2;
		            continue;
		      }
			else if ((pu8IEs[index]==RSN_IE)||
				((pu8IEs[index]==WPA_IE)&&  (pu8IEs[index+2] == 0x00) && 
				(pu8IEs[index+3] == 0x50) && (pu8IEs[index+4] == 0xF2) &&
				 (pu8IEs[index+5] == 0x01)))
			{
				NMI_Uint16 rsnIndex = index;
				/*PRINT_D(HOSTINF_DBG,"RSN IE Length:%d\n",pu8IEs[rsnIndex+1]);
				for(i=0; i<pu8IEs[rsnIndex+1]; i++)
				{
					PRINT_D(HOSTINF_DBG,"%0x ",pu8IEs[rsnIndex+2+i]);
				}*/
				if(pu8IEs[rsnIndex]==RSN_IE)
				{
					pNewJoinBssParam->mode_802_11i = 2;
					//PRINT_D(HOSTINF_DBG,"\nRSN_IE\n");
				}
				else
				{	//check if rsn was previously parsed
					if(pNewJoinBssParam->mode_802_11i==0)
						pNewJoinBssParam->mode_802_11i = 1;
					//PRINT_D(HOSTINF_DBG,"\nWPA_IE\n");
					rsnIndex+=4;
				}
				rsnIndex+=7;//skipping id, length, version(2B) and first 3 bytes of gcipher
				pNewJoinBssParam->rsn_grp_policy = pu8IEs[rsnIndex];
				rsnIndex++;
				//PRINT_D(HOSTINF_DBG,"Group Policy: %0x \n",pNewJoinBssParam->rsn_grp_policy);
				//initialize policies with invalid values
				NMI_memset(pNewJoinBssParam->rsn_pcip_policy,0xFF, 3);
				NMI_memset(pNewJoinBssParam->rsn_auth_policy,0xFF, 3);

				jumpOffset = pu8IEs[rsnIndex]*4; //total no.of bytes of pcipher field (count*4)

				/*parsing pairwise cipher*/

				//saving 3 pcipher max.
				pcipherCount = (pu8IEs[rsnIndex] > 3) ? 3 :pu8IEs[rsnIndex];
				rsnIndex+=2; //jump 2 bytes of pcipher count	

				//PRINT_D(HOSTINF_DBG,"\npcipher: ");
				for(i=1; i<=pcipherCount; i++)
				{
					//each count corresponds to 4 bytes, only last byte is saved
					pNewJoinBssParam->rsn_pcip_policy[i-1] = pu8IEs[rsnIndex+(i*4)-1];
					//PRINT_D(HOSTINF_DBG,"%0x ",pNewJoinBssParam->rsn_pcip_policy[i-1]);
				}
				rsnIndex+=jumpOffset;

				jumpOffset = pu8IEs[rsnIndex]*4;
				
				/*parsing AKM suite (auth_policy)*/
				//saving 3 auth policies max.
				authCount = (pu8IEs[rsnIndex] > 3) ? 3 :pu8IEs[rsnIndex];
				rsnIndex+=2; //jump 2 bytes of pcipher count	

				//PRINT_D(HOSTINF_DBG,"\nauth policy: ");
				for(i=1; i<=authCount; i++)
				{
					//each count corresponds to 4 bytes, only last byte is saved
					pNewJoinBssParam->rsn_auth_policy[i-1] = pu8IEs[rsnIndex+(i*4)-1];
					printk("%0x ",pNewJoinBssParam->rsn_auth_policy[i-1]);
				}
				//printk("\n");
				rsnIndex+=jumpOffset;
				/*pasring rsn cap. only if rsn IE*/
				if(pu8IEs[index]==RSN_IE)
				{	pNewJoinBssParam->rsn_cap[0]=pu8IEs[rsnIndex];
					pNewJoinBssParam->rsn_cap[1]=pu8IEs[rsnIndex+1];
					//PRINT_D(HOSTINF_DBG,"%0x %0x\n",pNewJoinBssParam->rsn_cap[0], pNewJoinBssParam->rsn_cap[1]);
					rsnIndex+=2	;
				}
				pNewJoinBssParam->rsn_found=1;
				index+=pu8IEs[index+1]+2; //ID,Length bytes and IE body
				continue;
			}
			else
				index+=pu8IEs[index+1]+2; //ID,Length bytes and IE body
		
	    	}
			
			
	}
		
	host_int_addToJoinBssTable(pNewJoinBssParam);
}
#endif	/*NMI_PARSE_SCAN_IN_HOST*/


/**
*  @brief 		host_int_addBASession
*  @details 	   	Open a block Ack session with the given parameters 
*  @param[in]   	tstrNetworkInfo* ptstrNetworkInfo
*  @return 	    	
*  @author		anoureldin
*  @date	
*  @version		1.0**/
#define BLOCK_ACK_REQ_SIZE 0x14

static int host_int_addBASession(char* pBSSID,char TID,short int BufferSize,
	short int SessionTimeout)
{
	tstrWID strWID;
	int AddbaTimeout = 100;
	char* ptr = NULL;
	int s32Err;
	printk("Opening Block Ack session with\nBSSID = %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\nTID=%d\nBufferSize == %d\nSessionTimeOut = %d\n",
			pBSSID[0],pBSSID[1],pBSSID[2],pBSSID[3],pBSSID[4],pBSSID[5],TID,BufferSize,SessionTimeout);

	strWID.u16WIDid = (NMI_Uint16)WID_11E_P_ACTION_REQ;
	strWID.enuWIDtype = WID_STR;
	strWID.ps8WidVal = (NMI_Uint8*)NMI_MALLOC(BLOCK_ACK_REQ_SIZE);
	strWID.s32ValueSize = BLOCK_ACK_REQ_SIZE;

	ptr = strWID.ps8WidVal;

	//*ptr++ = 0x14;
	*ptr++ = 0x14;
	*ptr++ = 0x3;
	*ptr++ = 0x0;
	NMI_memcpy(ptr,pBSSID, 6);
	ptr += 6;
	*ptr++ = TID;
	/* BA Policy*/
	*ptr++ = 1;
	/* Buffer size*/
	*ptr++ = (BufferSize & 0xFF);
	*ptr++ = ((BufferSize>>16) & 0xFF);
	/* BA timeout*/
	*ptr++ = (SessionTimeout& 0xFF);
	*ptr++ = ((SessionTimeout>>16) & 0xFF);
	/* ADDBA timeout*/
	*ptr++ = (AddbaTimeout& 0xFF);
	*ptr++ = ((AddbaTimeout>>16) & 0xFF);
	/* Group Buffer Max Frames*/
	*ptr++ = 8;
	/* Group Buffer Timeout */
	*ptr++ = 0;

	s32Err = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	if(s32Err)
		PRINT_D(HOSTINF_DBG, "Couldn't open BA Session\n");


	strWID.u16WIDid = (NMI_Uint16)WID_11E_P_ACTION_REQ;
	strWID.enuWIDtype = WID_STR;
	strWID.s32ValueSize = 15;

	ptr = strWID.ps8WidVal;

	//*ptr++ = 0x14;
	*ptr++ = 15;
	*ptr++ = 7;
	*ptr++ = 0x2;
	NMI_memcpy(ptr,pBSSID, 6);
	ptr += 6;
	/* TID*/
	*ptr++ = TID;
	/* Max Num MSDU */
	*ptr++ = 8;
	/* BA timeout*/
	*ptr++ = (SessionTimeout& 0xFF);
	*ptr++ = ((SessionTimeout>>16) & 0xFF);
	/*Ack-Policy */
	*ptr++ = 3;
	s32Err = SendConfigPkt(SET_CFG, &strWID, 1, NMI_TRUE);
	
	return s32Err;

	
}




