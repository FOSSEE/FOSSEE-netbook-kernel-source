/*
 * --------------------------------------------------------------------------
 *
 *       Filename:  flash.h
 *
 *    Description:  
 *
 *        Version:  0.01
 *        Created:  2013年07月02日 14时17分16秒
 *
 *         Author:  smmei (), 
 *        Company:  
 * --------------------------------------------------------------------------
 */
#ifndef __FLASH_H__
#define __FLASH_H__

struct flash_dev {
	char name[20];
	int (*init)(void);
	int (*set_mode)(int mode);
	void (*exit)(void);
};

extern struct flash_dev flash_dev_gpio;
extern struct flash_dev flash_dev_eup2471;
extern struct flash_dev flash_dev_eup3618;
extern struct flash_dev flash_dev_ktd231;
extern struct flash_dev flash_dev_sgm3780;

extern struct flash_dev *flash_instantiation(void);
extern int flash_set_mode(struct flash_dev *fl, int mode);
extern void flash_destroy(struct flash_dev *fl);

#endif 	/* #ifndef __FLASH_H__ */

