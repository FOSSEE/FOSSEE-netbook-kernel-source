/**************************************************************
使用前注意通道数，驱动默认使用通道是sense 
大于drive否则需要将使用到的DRIVENO与SENSENO调换
此情况包括0x66和0x67寄存器，但不必修改。
***************************************************************/
#ifndef __SSD253X_20125181742_TS_H__
#define __SSD253X_20125181742_TS_H__
#define DRIVENO	15
#define SENSENO	10
#define EdgeDisable		1	// if Edge Disable, set it to 1, else reset to 0
#define RunningAverageMode	2	//{0,8},{5,3},{6,2},{7,1}
#define RunningAverageDist	4	// Threshold Between two consecutive points
#define MicroTimeTInterupt	10000000 //20000000// 100Hz - 10,000,000us
#define FINGERNO		10

//#define USE_TOUCH_KEY

#define USE_CUT_EDGE    //0x8b must be 0x00;  EdgeDisable set 0
//#undef  USE_CUT_EDGE

#ifdef USE_CUT_EDGE
		#define XPOS_MAX 576 //(DRIVENO - EdgeDisable) *64
		#define YPOS_MAX 896 //(SENSENO - EdgeDisable) *64
#endif



#endif
