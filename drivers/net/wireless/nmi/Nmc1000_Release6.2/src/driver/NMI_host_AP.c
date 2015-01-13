/*!  
*  @file	NMI_host_AP.c
*  @brief	code related to AP mode on driver
*  @author	asobhy
*  @date	09 APRIL 2013
*  @version	1.0
*/



#ifndef SIMULATION
#include "linux_wlan_common.h"
#endif
#include "NMI_OsWrapper/include/NMI_OSWrapper.h"
#include "NMI_host_AP.h"
#ifdef NMI_FULLY_HOSTING_AP


beacon_info strBeaconInfo = {0};


/*
*  @brief 			NMI_beacon_tx_complete
*  @details 	   	call back function for beacon transmission through vmm , does nothing at the moment
*  @return 	    	
*  @author		asobhy
*  @date			09 APRIL 2013
*  @version		1.0
*/	
static void NMI_beacon_tx_complete(void* priv, int status)
{
	
	 beacon_data* pv_data = (beacon_data*)priv;
	 NMI_Uint8 * buf=  pv_data->buff;

	 
	//if(status == 1){
		//if(INFO || buf[0] == 0x80 || buf[0] == 0xb0)
		//PRINT_D(HOSTAPD_DBG,"Packet sent successfully - Size = %d - Address = %p.\n",pv_data->size,pv_data->buff);
	//}else{
			//PRINT_D(HOSTAPD_DBG,"Couldn't send packet - Size = %d - Address = %p.\n",pv_data->size,pv_data->buff);
		//}
}


/*
*  @brief 			NMI_beacon_xmit
*  @details 	   	send beacon frame to firmware
*  @param[in]    	u8 *buf : pointer to the beacon frame
*				size_t len : beacon frame length
*  @return 	    	Error code.
*  @author		asobhy
*  @date			09 APRIL 2013
*  @version		1.0
*/	
static int NMI_beacon_xmit(const u8 *buf, size_t len)
{
	beacon_data *strBeacon_tx =NULL;

	/* allocate the beacon_data struct */
	strBeacon_tx = (beacon_data*)kmalloc(sizeof(beacon_data),GFP_ATOMIC);
	if(strBeacon_tx == NULL){
		PRINT_ER("Failed to allocate memory for mgmt_tx structure\n");
	     	return NMI_FAIL;
	}

	/* allocate buffer for beacon data within the beacon_data struct */
	strBeacon_tx->buff= (char*)kmalloc(len,GFP_ATOMIC);
	if(strBeacon_tx->buff == NULL)
	{
		PRINT_ER("Failed to allocate memory for mgmt_tx buff\n");
	     	return NMI_FAIL;
	
	}

	/* fill in the beacon data stuct */
	memcpy(strBeacon_tx->buff,buf,len);
	strBeacon_tx->size=len;

	/* the actual transmission of beacon to firmware*/
	//NMI_PRINTF("--IN beacon_tx: Sending beacon Pkt to tx queue--\n");
	nmi_wlan_txq_add_mgmt_pkt(strBeacon_tx,strBeacon_tx->buff,strBeacon_tx->size,NMI_beacon_tx_complete);

	return 0;
}


/**
*  @brief 			host_add_beacon
*  @details 	   	Setting add beacon params in message queue 
*  @param[in] 	NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32Interval,
			 	NMI_Uint32 u32DTIMPeriod,NMI_Uint32 u32HeadLen, NMI_Uint8* pu8Head,
			   	NMI_Uint32 u32TailLen, NMI_Uint8* pu8Tail
*  @return 	    	Error code.
*  @author		asobhy
*  @date	
*  @version	1.0
*/
NMI_Sint32 host_add_beacon(NMI_WFIDrvHandle hWFIDrv, NMI_Uint32 u32Interval,
									 NMI_Uint32 u32DTIMPeriod,
									 NMI_Uint32 u32HeadLen, NMI_Uint8* pu8Head,
									 NMI_Uint32 u32TailLen, NMI_Uint8* pu8Tail)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	NMI_Bool bIsBeaconSet = NMI_FALSE;
	NMI_Uint8 *pu8BeaconFrame;
	NMI_Uint16  u16size=0;
	nmi_wlan_dev_t* strWlan = Get_wlan_context(&u16size); 

	if(u16size != sizeof(nmi_wlan_dev_t))
		PRINT_ER("size of nmi_wlan_dev_t in nmi_wlan != it's size in NMI_HOST_AP\n");


	NMI_PRINTF("--IN host_add_beacon--\n");

	/* calculate beacon length */
	strBeaconInfo.u16beacon_len = u32HeadLen + u32TailLen + DEFAULT_TIM_LEN + 2 ;
	PRINT_D(HOSTAPD_DBG,"beacon_len=%d\n",strBeaconInfo.u16beacon_len);


	/* set beacon interval in chip */
	if(u32Interval>0)
	{
		strBeaconInfo.u16Beacon_Period = u32Interval;
		PRINT_D(HOSTAPD_DBG,"Beacon_Period=%d\n",strBeaconInfo.u16Beacon_Period );
		strWlan->hif_func.hif_write_reg(rMAC_BEACON_PERIOD, u32Interval);
		
	}

	/* allocate beacon frame */
	if(strBeaconInfo.u8beacon_frame == NULL)
	{
		PRINT_D(HOSTAPD_DBG,"beacon_frame wasn't allocated. allocating new buffers\n");
		/* Allocate 2 global beacon buffers as the beacon for AP may be modfied  */
		/* for which 2 buffers are needed.                                       */
		strBeaconInfo.u8beacon_frame  = (NMI_Uint8 *)kmalloc(strBeaconInfo.u16beacon_len,GFP_ATOMIC);
		if(strBeaconInfo.u8beacon_frame  == NULL)
		{
			PRINT_ER("Couldn't allocate beacon_frame\n");
			/* Exception - no memory for beacons */
			s32Error = NMI_FAIL;
			return s32Error;
		}
	}
	else
	{
		bIsBeaconSet = NMI_TRUE;
	}

	/* set beacon DTIM period in chip */
	if(u32DTIMPeriod>0)
	{
		strBeaconInfo.u8DTIMPeriod= u32DTIMPeriod;
		PRINT_D(HOSTAPD_DBG,"DTIMPeriod=%d\n",strBeaconInfo.u8DTIMPeriod);
		strWlan->hif_func.hif_write_reg(rMAC_DTIM_PERIOD, u32DTIMPeriod);
	}

	/* save TIM element location within the beacon */
	strBeaconInfo.u8tim_element_index = u32HeadLen;
	strBeaconInfo.u16tim_element_trailer_len = u32TailLen;

	/* Copy beacon head part*/
	pu8BeaconFrame = strBeaconInfo.u8beacon_frame;
	NMI_memcpy(pu8BeaconFrame, pu8Head, u32HeadLen);
	pu8BeaconFrame += u32HeadLen;

	/* Set the TIM element field with default parameters and update the      */
	/* index value with the default length.                                  */
	*(pu8BeaconFrame++) = ITIM;
	*(pu8BeaconFrame++) = DEFAULT_TIM_LEN;
	*(pu8BeaconFrame++) = 0;
	*(pu8BeaconFrame++) = strBeaconInfo.u8DTIMPeriod;
	*(pu8BeaconFrame++) = 0;
	*(pu8BeaconFrame++) = 0;

	NMI_memcpy(pu8BeaconFrame, pu8Tail, u32TailLen);
	pu8BeaconFrame+=u32TailLen;

	NMI_beacon_xmit(strBeaconInfo.u8beacon_frame,strBeaconInfo.u16beacon_len);

	PRINT_D(HOSTAPD_DBG,"Starting TSF timer \n");

	/* Initialize the virtual bitmap */
	    strBeaconInfo.u8vbmap[TYPE_OFFSET]        		= ITIM;                  		/* Element ID       */
	    strBeaconInfo.u8vbmap[LENGTH_OFFSET]      		= DEFAULT_TIM_LEN;       	/* Element Length   */
	    strBeaconInfo.u8vbmap[DTIM_CNT_OFFSET]    		= 0;                     			/* Dtim Count       */
	    strBeaconInfo.u8vbmap[DTIM_PERIOD_OFFSET] 	= strBeaconInfo.u8DTIMPeriod;     /* Dtim Period      */
	    strBeaconInfo.u8vbmap[BMAP_CTRL_OFFSET]   		= 0;                     			/* Bitmap Control   */
	    strBeaconInfo.u8vbmap[TIM_OFFSET]         		= 0;                     			/* Copy TIM element */


	/* Start TSF timer in chip to start sending beacons*/
	strWlan->hif_func.hif_write_reg(rMAC_TSF_CON, BIT1 | BIT0);

	return s32Error;

}

/*
*  @brief 			host_del_beacon
*  @details 	   	delete the allocated beacon
*  @param[in]    	NMI_WFIDrvHandle hWFIDrv
*  @return 	    	Error code.
*  @author		asobhy
*  @date			09 APRIL 2013
*  @version		1.0
*/
NMI_Sint32 host_del_beacon(NMI_WFIDrvHandle hWFIDrv)
{
	NMI_Sint32 s32Error = NMI_SUCCESS;
	PRINT_D(HOSTAPD_DBG,"host_del_beacon \n");
	
	if(strBeaconInfo.u8beacon_frame != NULL)
	{
		kfree(strBeaconInfo.u8beacon_frame);
		strBeaconInfo.u8beacon_frame=NULL;
		strBeaconInfo.u16beacon_len = 0;
	}
		
	NMI_ERRORCHECK(s32Error);

	NMI_CATCH(s32Error)
	{
	}
	return s32Error;
}

/*
*  @brief 			handle tbtt ISR and send updated beacon to chip
*  @details 	   	
*  @param[in]    	
*  @return 	    	
*  @author		asobhy
*  @date			09 APRIL 2013
*  @version		1.0
*/
void process_tbtt_isr(void)
{

       NMI_Uint16 i           = 0;
	NMI_Uint8 u8OldTimLen = 0;
	NMI_Uint32 dtim_count  = 0;
	NMI_Uint16  u16size=0;
	nmi_wlan_dev_t* strWlan = Get_wlan_context(&u16size); 

	//Warning : This is a development only print to notify the developer, it should be removed later on 
	if(u16size != sizeof(nmi_wlan_dev_t))
		PRINT_ER("size of nmi_wlan_dev_t in nmi_wlan != it's size in NMI_HOST_AP\n");

	PRINT_D(HOSTAPD_DBG,"--IN process_tbtt_isr--\n");

        /* Read the current DTIM count from H/W */
	strWlan->hif_func.hif_read_reg(rMAC_DTIM_COUNT_ADDR, &dtim_count);
		
       //NMI_PRINTF("dtim_count = %d",dtim_count);

        /* If the beacon pointer has been updated to a new value, the free   */
        /* beacon buffer index is updated to the other buffer.               */
        if(dtim_count == 0)
        {
            dtim_count = strBeaconInfo.u8DTIMPeriod - 1;

		// TODO: 
            /* The beacon transmitted at this TBTT is the DTIM. Requeue all  */
            /* MC/BC packets to the high priority queue.                     */
            /* Check if the beacon that is being transmitted has the MC bit  */
            /* set                                                           */
        /*    if(BTRUE == get_mc_bit_bcn(old_bcn_idx))
                while(requeue_ps_packet(NULL, &g_mc_q, BTRUE, BFALSE)
                      == PKT_REQUEUED);
        */
        }
        else
        {
            /* Do nothing */
            dtim_count--;
        }

	/* If the buffer that is not currently in use has not been freed*/
	if(strBeaconInfo.u8beacon_frame != NULL)
	{
	
#ifdef NMI_AP_EXTERNAL_MLME

			
		/* Shift the position of the trailer after the TIM element if needed*/
		u8OldTimLen = strBeaconInfo.u8beacon_frame[strBeaconInfo.u8tim_element_index + LENGTH_OFFSET];
		if(u8OldTimLen >   strBeaconInfo.u8vbmap[LENGTH_OFFSET])
		{
			NMI_Uint32 u32ShiftOffset = u8OldTimLen - strBeaconInfo.u8vbmap[LENGTH_OFFSET];
			NMI_Uint32 u32TrailerIndex = strBeaconInfo.u8tim_element_index+u8OldTimLen+2;
			/*need to shrink the old TIM; i.e. shift the trailer backwards*/
			for(i=u32TrailerIndex;  i<(u32TrailerIndex+strBeaconInfo.u16tim_element_trailer_len); i++)
			{
				strBeaconInfo.u8beacon_frame[i-u32ShiftOffset] = strBeaconInfo.u8beacon_frame[i];
			}
		}
		else if(u8OldTimLen <   strBeaconInfo.u8vbmap[LENGTH_OFFSET])
		{
			NMI_Uint32 u32ShiftOffset = strBeaconInfo.u8vbmap[LENGTH_OFFSET] - u8OldTimLen;
			NMI_Uint32 u32TrailerIndex = strBeaconInfo.u8tim_element_index+u8OldTimLen+2;
			/*need to enlarge the old TIM; i.e. shift the trailer forward*/
			for(i=(u32TrailerIndex+strBeaconInfo.u16tim_element_trailer_len-1);  i>=u32TrailerIndex; i--)
			{
				strBeaconInfo.u8beacon_frame[i+u32ShiftOffset] = strBeaconInfo.u8beacon_frame[i];
			}
		}
		/* Other wise TIM element has the same length, and no shifting is required*/
#endif /*NMI_AP_EXTERNAL_MLME*/	
	        strBeaconInfo.u8vbmap[DTIM_CNT_OFFSET] = dtim_count;
		
	        for(i = 0; i < strBeaconInfo.u8vbmap[LENGTH_OFFSET] + 2; i++)
	        {
	            strBeaconInfo.u8beacon_frame[strBeaconInfo.u8tim_element_index + i] =
	                                                                    strBeaconInfo.u8vbmap[i];
	        }
	        strBeaconInfo.u16beacon_len=  strBeaconInfo.u8tim_element_index +  strBeaconInfo.u16tim_element_trailer_len +
	                                          strBeaconInfo.u8vbmap[LENGTH_OFFSET] + 2 ;

		// TODO: 
	        /* The status of BC/MC packets queued for PS should be updated only  */
	        /* in DTIM beacon dtim_count==0. For the rest of the becons reset    */
	        /* the BC/MC bit in TIM                                              */
	       /* if(dtim_count != 0)
	            reset_mc_bit_bcn(g_beacon_index);*/

		/* send the updated beacon to firmware */
		NMI_beacon_xmit(strBeaconInfo.u8beacon_frame,strBeaconInfo.u16beacon_len);
		

	}


}


#endif 


