
#ifndef __SP0a19_H_
#define __SP0a19_H_

//ECLK Drv
#define  Pre_Value_P0_0x30  0x00
//Filter en&dis
#define  Pre_Value_P0_0x56  0x70
#define  Pre_Value_P0_0x57  0x10  //filter outdoor
#define  Pre_Value_P0_0x58  0x10  //filter indoor
#define  Pre_Value_P0_0x59  0x10  //filter night
#define  Pre_Value_P0_0x5a  0x08  //smooth outdoor 0x02
#define  Pre_Value_P0_0x5b  0x14  //smooth indoor 0x02
#define  Pre_Value_P0_0x5c  0x30  //smooht night 0x20
//outdoor sharpness
#define  Pre_Value_P0_0x65  0x03
#define  Pre_Value_P0_0x66  0x01
#define  Pre_Value_P0_0x67  0x03
#define  Pre_Value_P0_0x68  0x46
//indoor sharpness
#define  Pre_Value_P0_0x6b  0x04
#define  Pre_Value_P0_0x6c  0x01
#define  Pre_Value_P0_0x6d  0x03// 0x03
#define  Pre_Value_P0_0x6e  0x43
//night sharpness
#define  Pre_Value_P0_0x71  0x05
#define  Pre_Value_P0_0x72  0x01
#define  Pre_Value_P0_0x73  0x03
#define  Pre_Value_P0_0x74  0x43
//color
#define  Pre_Value_P0_0x7f  0xd7  //R
#define  Pre_Value_P0_0x87  0xf8  //B
//satutation
#define  Pre_Value_P0_0xd8  0x48
#define  Pre_Value_P0_0xd9  0x44//0x48
#define  Pre_Value_P0_0xda  0x3a//0x48
#define  Pre_Value_P0_0xdb  0x38//0x48
//AE target
#define  Pre_Value_P0_0xf7  0x78
#define  Pre_Value_P0_0xf8  0x63
#define  Pre_Value_P0_0xf9  0x68
#define  Pre_Value_P0_0xfa  0x53
//HEQ
#define  Pre_Value_P0_0xdd  0x78 // 0x70
#define  Pre_Value_P0_0xde  0x88 // 0x90
//AWB pre gain
#define  Pre_Value_P1_0x28  0x75
#define  Pre_Value_P1_0x29  0x4e

//VBLANK
#define  Pre_Value_P0_0x05  0x00
#define  Pre_Value_P0_0x06  0x00
//HBLANK
#define  Pre_Value_P0_0x09  0x01
#define  Pre_Value_P0_0x0a  0x76

//AE target
#define  SP0A19_P0_0xf7  0x88
#define  SP0A19_P0_0xf8  0x80
#define  SP0A19_P0_0xf9  0x70
#define  SP0A19_P0_0xfa  0x68
//HEQ
#define  SP0A19_P0_0xdd  0x7c
#define  SP0A19_P0_0xde  0xc4

// Scene Mode
uint8_t sp0a19_scene_mode_auto[] = {
	//caprure preview daylight 24M 50hz 20-8FPS maxgain:0x70

};

uint8_t sp0a19_scene_mode_night[] = {
	//capture preview night 24M 50hz 20-6FPS maxgain:0x78

};


// White Balance
uint8_t sp0a19_wb_auto [] = {
	0xfd,0x01,
	0x28,0xc5,
	0x29,0x9b,
	0xfd,0x00,
	0x32,0x15,   //awb & ae  opened
	0xfd,0x00,
};

uint8_t sp0a19_wb_incandescent [] = {
	0xfd,0x00,
	0x32,0x05,
	0xfd,0x01,
	0x28,0x89,
	0x29,0xb8,
	0xfd,0x00,
};

uint8_t sp0a19_wb_fluorescent [] = {
	0xfd,0x00,
	0x32,0x05,
	0xfd,0x01,
	0x28,0xaf,
	0x29,0x99,
	0xfd,0x00,
};

uint8_t sp0a19_wb_daylight [] = {
	0xfd,0x00, 
	0x32,0x05,
	0xfd,0x01,
	0x28,0xbc,
	0x29,0x5d,
	0xfd,0x00,
};

uint8_t sp0a19_wb_cloudy [] = {
	0xfd,0x00,
	0x32,0x05,
	0xfd,0x01,
	0x28,0xbf,
	0x29,0x89,
	0xfd,0x00,
};

uint8_t sp0a19_wb_tungsten [] = {
	0xfd,0x00,
	0x32,0x05,
	0xfd,0x01,
	0x28,0x90,
	0x29,0xc7,
	0xfd,0x00,
};


// Exposure
uint8_t sp0a19_exposure_neg2[] = {
};

uint8_t sp0a19_exposure_neg1[] = {
};

uint8_t sp0a19_exposure_zero[] = {
};

uint8_t sp0a19_exposure_pos1[] = {
};

uint8_t sp0a19_exposure_pos2[] = {
};


// Color Effect
uint8_t sp0a19_colorfx_none[] = {
	0xfd, 0x00,
	0x62, 0x00,
	0x63, 0x80,
	0x64, 0x80,
};

uint8_t sp0a19_colorfx_bw[] = {
	0xfd, 0x00,
	0x62, 0x20,
	0x63, 0x80,
	0x64, 0x80,
};

uint8_t sp0a19_colorfx_sepia[] = {
	0xfd, 0x00,
	0x62, 0x10,
	0x63, 0xc0,
	0x64, 0x20,
};

uint8_t sp0a19_colorfx_negative[] = {
	0xfd, 0x00,
	0x62, 0x10,
	0x63, 0x80,
	0x64, 0x80,
};

uint8_t sp0a19_colorfx_emboss[] = {
};

uint8_t sp0a19_colorfx_sketch[] = {
};

uint8_t sp0a19_colorfx_sky_blue[] = {
};

uint8_t sp0a19_colorfx_grass_green[] = {
};

uint8_t sp0a19_colorfx_skin_whiten[] = {
};

uint8_t sp0a19_colorfx_vivid[] = {
};

// Brightness
uint8_t sp0a19_brightness_neg4[] = {
};

uint8_t sp0a19_brightness_neg3[] = {
};

uint8_t sp0a19_brightness_neg2[] = {
};

uint8_t sp0a19_brightness_neg1[] = {
};

uint8_t sp0a19_brightness_zero[] = {
};

uint8_t sp0a19_brightness_pos1[] = {
};

uint8_t sp0a19_brightness_pos2[] = {
};

uint8_t sp0a19_brightness_pos3[] = {
};

uint8_t sp0a19_brightness_pos4[] = {
};

// Contrast
uint8_t sp0a19_contrast_neg4[] = {
};

uint8_t sp0a19_contrast_neg3[] = {
};

uint8_t sp0a19_contrast_neg2[] = {
};

uint8_t sp0a19_contrast_neg1[] = {
};

uint8_t sp0a19_contrast_zero[] = {
};

uint8_t sp0a19_contrast_pos1[] = {
};

uint8_t sp0a19_contrast_pos2[] = {
};

uint8_t sp0a19_contrast_pos3[] = {
};

uint8_t sp0a19_contrast_pos4[] = {
};

// Saturation
uint8_t sp0a19_saturation_neg4[] = {
};

uint8_t sp0a19_saturation_neg3[] = {
};

uint8_t sp0a19_saturation_neg2[] = {
};

uint8_t sp0a19_saturation_neg1[] = {
};

uint8_t sp0a19_saturation_zero[] = {
};

uint8_t sp0a19_saturation_pos1[] = {
};

uint8_t sp0a19_saturation_pos2[] = {
};

uint8_t sp0a19_saturation_pos3[] = {
};

uint8_t sp0a19_saturation_pos4[] = {
};


// Resolution

// QCIF
uint8_t sp0a19_176x144[]={
};

// QVGA
uint8_t sp0a19_320x240[]={
  0xfd,0x01,  
	0x0e,0x03,
	0xfd,0x00,
	0x30,0x10,
};

// CIF
uint8_t sp0a19_352x288[]={
};

// VGA
uint8_t sp0a19_640x480[]={
  0xfd,0x01,  
	0x0e,0x00,
	0xfd,0x00,
	0x30,0x00,
};

// SVGA
uint8_t sp0a19_800x600[]={
};

// XGA
uint8_t sp0a19_1024x768[]={
};

// 720p
uint8_t sp0a19_1280x720[]={
};

// UXGA
uint8_t sp0a19_1600x1200[]={
};

// 1080p
uint8_t sp0a19_1920x1080[]={
};

// QXGA
uint8_t sp0a19_2048x1536[]={
};

uint8_t sp0a19_2592x1944[]={
};

// Initiliztion
uint8_t sp0a19_default_regs_init[] = {
	/*init registers code.*/
    0xfd,0x00,
    0x1c,0x28,
    0x32,0x00,
    0x0f,0x2f,
    0x10,0x2e,
    0x11,0x00,
    0x12,0x18,
    0x13,0x2f,
    0x14,0x00,
    0x15,0x3f,
    0x16,0x00,
    0x17,0x18,
    0x25,0x40,
    0x1a,0x0b,
    0x1b,0xc ,
    0x1e,0xb ,
    0x20,0x3f,
    0x21,0x13,
    0x22,0x19,
    0x26,0x1a,
    0x27,0xab,
    0x28,0xfd,
    0x30,0x00,
    0x31,0x00, //0x10
    0xfb,0x31,
    0x1f,0x08,  

	//Blacklevel
    0xfd,0x00,
    0x65,0x00,//06
    0x66,0x00,//06
    0x67,0x00,//06
    0x68,0x00,//06
    0x45,0x00,
    0x46,0x0f,

	//ae setting 
    0xfd,0x00,
    0x03,0x01,
    0x04,0x32,
    0x06,0x00,
    0x09,0x01,
    0x0a,0x46,
    0xf0,0x66,
    0xf1,0x00,
    0xfd,0x01,
    0x90,0x0c,
    0x92,0x01,
    0x98,0x66,
    0x99,0x00,
    0x9a,0x01,
    0x9b,0x00,

	//Status
    0xfd,0x01,
    0xce,0xc8,
    0xcf,0x04,
    0xd0,0xc8,
    0xd1,0x04, 

    0xfd,0x01,
    0xc4,0x56,
    0xc5,0x8f,
    0xca,0x30,
    0xcb,0x45,
    0xcc,0x70,
    0xcd,0x48,
    0xfd,0x00,

	//lsc  for st
    0xfd,0x01,
    0x35,0x15,
    0x36,0x15,
    0x37,0x15,
    0x38,0x15,
    0x39,0x15,
    0x3a,0x15,
    0x3b,0x13,
    0x3c,0x15,
    0x3d,0x15,
    0x3e,0x15,
    0x3f,0x15,
    0x40,0x18,
    0x41,0x00,
    0x42,0x04,
    0x43,0x04,
    0x44,0x00,
    0x45,0x00,
    0x46,0x00,
    0x47,0x00,
    0x48,0x00,
    0x49,0xfd,
    0x4a,0x00,
    0x4b,0x00,
    0x4c,0xfd,
    0xfd,0x00,  
	//awb 1
    0xfd,0x01,
    0x28,0xc5,
    0x29,0x9b,
    0x2e,0x02,	
    0x2f,0x16,
    0x17,0x17,
    0x18,0x19,
    0x19,0x45,
    0x2a,0xef,
    0x2b,0x15,  
	//awb2
    0xfd,0x01,
    0x73,0x80,
    0x1a,0x80,
    0x1b,0x80, 
	//d65
    0x65,0xd5,
    0x66,0xfa,
    0x67,0x72,
    0x68,0x8a,
	//indoor
    0x69,0xc6,
    0x6a,0xee,
    0x6b,0x94,
    0x6c,0xab,
	//f
    0x61,0x7a,
    0x62,0x98,
    0x63,0xc5,
    0x64,0xe6,
	//cwf
    0x6d,0xb9,
    0x6e,0xde,
    0x6f,0xb2,
    0x70,0xd5,

	//////////////////skin detect//////
    0xfd,0x01,
    0x08,0x15,
    0x09,0x04,
    0x0a,0x20,
    0x0b,0x12,
    0x0c,0x27,
    0x0d,0x06,
    0x0f,0x63,  
	//BPC_grad
    0xfd,0x00,
    0x79,0xf0,
    0x7a,0x80,  //f0
    0x7b,0x80,  //f0
    0x7c,0x20,  //f0  

	//smooth
    0xfd,0x00,
	//��ͨ����ƽ����ֵ	
    0x57,0x08,	//raw_dif_thr_outdoor
    0x58,0x0c, //raw_dif_thr_normal
    0x56,0x10, //raw_dif_thr_dummy
    0x59,0x15, //raw_dif_thr_lowlight
	//GrGbƽ����ֵ
    0x89,0x08,	//raw_grgb_thr_outdoor 
    0x8a,0x0c, //raw_grgb_thr_normal  
    0x9c,0x10, //raw_grgb_thr_dummy   
    0x9d,0x15, //raw_grgb_thr_lowlight

	//Gr\Gb֮��ƽ��ǿ��
    0x81,0xe0,    //raw_gflt_fac_outdoor
    0x82,0x80, //80//raw_gflt_fac_normal
    0x83,0x30,    //raw_gflt_fac_dummy
    0x84,0x18,    //raw_gflt_fac_lowlight
	//Gr��Gb��ͨ����ƽ��ǿ��  
    0x85,0xe0, //raw_gf_fac_outdoor  
    0x86,0x80, //raw_gf_fac_normal  
    0x87,0x30, //raw_gf_fac_dummy   
    0x88,0x18, //raw_gf_fac_lowlight
	//R��Bƽ��ǿ��  
    0x5a,0xff,		 //raw_rb_fac_outdoor
    0x5b,0xe0, //40//raw_rb_fac_normal
    0x5c,0x60, 	 //raw_rb_fac_dummy
    0x5d,0x20, 	 //raw_rb_fac_lowlight

	//sharpen 
    0xfd,0x01,
    0xe2,0x50,	//sharpen_y_base
    0xe4,0xa0,	//sharpen_y_max

    0xe5,0x08, //rangek_neg_outdoor
    0xd3,0x08, //rangek_pos_outdoor   
    0xd7,0x08, //range_base_outdoor   

    0xe6,0x0a, //rangek_neg_normal
    0xd4,0x0a, //rangek_pos_normal 
    0xd8,0x0a, //range_base_normal  

    0xe7,0x12, //rangek_neg_dummy
    0xd5,0x12, //rangek_pos_dummy
    0xd9,0x12, //range_base_dummy  

    0xd2,0x15, //rangek_neg_lowlight
    0xd6,0x15, //rangek_pos_lowlight
    0xda,0x15, //range_base_lowlight

    0xe8,0x20,//sharp_fac_pos_outdoor
    0xec,0x2c,//sharp_fac_neg_outdoor
    0xe9,0x20,//sharp_fac_pos_nr
    0xed,0x2c,//sharp_fac_neg_nr
    0xea,0x18,//sharp_fac_pos_dummy
    0xef,0x1c,//sharp_fac_neg_dummy
    0xeb,0x15,//sharp_fac_pos_low
    0xf0,0x18,//sharp_fac_neg_low 

	//CCM
    0xfd,0x01,
    0xa0,0x66,//80(��ɫ�ӽ�����ɫ������)
    0xa1,0x0 ,//0 
    0xa2,0x19,//0 
    0xa3,0xf3,//f0
    0xa4,0x8e,//a6
    0xa5,0x0 ,//ea
    0xa6,0x0 ,//0 
    0xa7,0xe6,//e6
    0xa8,0x9a,//9a
    0xa9,0x0 ,//0 
    0xaa,0x3 ,//33
    0xab,0xc ,//c 
    0xfd,0x00,

	//gamma  

    0xfd,0x00,
    0x8b,0x00,//0 
    0x8c,0x0c,//11
    0x8d,0x19,//19 
    0x8e,0x2C,//28 
    0x8f,0x49,//46 
    0x90,0x61,//61 
    0x91,0x77,//78 
    0x92,0x8A,//8A 
    0x93,0x9B,//9B 
    0x94,0xA9,//A9 
    0x95,0xB5,//B5 
    0x96,0xC0,//C0 
    0x97,0xCA,//CA 
    0x98,0xD4,//D4 
    0x99,0xDD,//DD 
    0x9a,0xE6,//E6 
    0x9b,0xEb,//EF 
    0xfd,0x01,//01 
    0x8d,0xF2,//F7 
    0x8e,0xFa,//FF 

	//rpc
    0xfd,0x00,
    0xe0,0x4c,
    0xe1,0x3c,
    0xe2,0x34,
    0xe3,0x2e,
    0xe4,0x2e,
    0xe5,0x2c,
    0xe6,0x2c,
    0xe8,0x2a,
    0xe9,0x2a,
    0xea,0x2a,
    0xeb,0x28,
    0xf5,0x28,
    0xf6,0x28,
	//ae min gain  
    0xfd,0x01,
    0x94,0xa0,//rpc_max_indr
    0x95,0x28,//1e//rpc_min_indr 
    0x9c,0xa0,//rpc_max_outdr
    0x9d,0x28,//rpc_min_outdr    

	//ae target
    0xfd,0x00, 
    0xed,SP0A19_P0_0xf7+0x04, 
    0xf7,SP0A19_P0_0xf7,
    0xf8,SP0A19_P0_0xf8,  
    0xec,SP0A19_P0_0xf8-0x04,
    0xef,SP0A19_P0_0xf9+0x04,
    0xf9,SP0A19_P0_0xf9,
    0xfa,SP0A19_P0_0xfa,  
    0xee,SP0A19_P0_0xfa-0x04, 
	//gray detect
    0xfd,0x01,
    0x30,0x40,
    0x31,0x70,//
    0x32,0x40,//80
    0x33,0xef,//
    0x34,0x05,//04
    0x4d,0x2f,//40
    0x4e,0x20,//
    0x4f,0x16,//13      
	//lowlight lum
    0xfd,0x00, 
    0xb2,0x10,//lum_limit
    0xb3,0x1f,//lum_set
    0xb4,0x30,//black_vt
    0xb5,0x45,//white_vt
	//saturation
    0xfd,0x00,
    0xbe,0xff,
    0xbf,0x01, 
    0xc0,0xff,
    0xc1,0xd8,
    0xd3,0x80,//0x78
    0xd4,0x78,//0x78
    0xd6,0x68,//0x78      
    0xd7,0x50,//0x78
	//HEQ
    0xfd,0x00,
    0xdc,0x00,
    0xdd,SP0A19_P0_0xdd,
    0xde,SP0A19_P0_0xde,//80
    0xdf,0x80,    
	//func enable
    0xfd,0x00,
    0x32,0x15,//d
    0x34,0x76,//16
    0x35,0x40,
    0x33,0xef,
    0x5f,0x51,
    0xfd,0x00,
    0x36,0x80, 
                
 };

// Exit
uint8_t sp0a19_default_regs_exit[] = {
};

#endif	/* #ifndef __sp0a19_H_ */