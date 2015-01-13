/*
 * This file is derived from crc32.c in U-Boot 1.1.4.
 * For conditions of distribution and use, see copyright in crc32.c
 */
/*
 * Some descriptions of such software. Copyright (c) 2008 WonderMedia Technologies, Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * WonderMedia Technologies, Inc.
 * 10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/super.h>
#include <mach/hardware.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/err.h>
#include <asm/io.h>
#include "mtdcore.h"
#include "devices/wmt_sf.h"
#include <linux/sha256.h>

static DEFINE_SEMAPHORE(s_wmt_env_lock);

#undef pr_err
#undef pr_warning
#undef pr_info
#define pr_err(fmt, args...)		printk("[WMTENV] %s, %d: " fmt, __func__, __LINE__, ##args)
#define pr_warning(fmt, args...)	printk("[WMTENV] %s, %d: " fmt, __func__, __LINE__, ##args)
#define pr_info(fmt, args...)		printk("[WMTENV] %s, %d: " fmt, __func__, __LINE__, ##args)


extern unsigned int MTDSF_PHY_ADDR;
extern int wmt_sfc_init(struct sfreg_t *sfc);
extern int spi_flash_sector_erase(unsigned long addr, struct sfreg_t *sfreg);
extern int spi_flash_sector_write(struct sfreg_t *sfreg,
				  unsigned char *sf_base_addr,
				  loff_t to, size_t len, u_char *buf);
extern int sf_copy_env(char *dest, char *src, int len);
extern int rsa_check(unsigned int pub_key_addr, unsigned int pub_key_size,
		     unsigned int sig_addr, unsigned int sig_size,
		     u8 *out_buf);

extern int wmt_is_secure_enabled(void);

/**
 * white list when secure mode is enabled.
 */
static const char *env_white_list[] = {
	"boot-method",
	"ril.imei",
	"wmt.modem.wakeuplight",
	"wmt.display.tvformat",
    "wmt.display.fb1",
    "wmt.display.fb2",
    "ethaddr",
	NULL
};

static const unsigned int sf_crc_table[256] = {
	0x00000000,	0x77073096,	0xee0e612c,	0x990951ba,	0x076dc419,
	0x706af48f,	0xe963a535,	0x9e6495a3,	0x0edb8832,	0x79dcb8a4,
	0xe0d5e91e,	0x97d2d988,	0x09b64c2b,	0x7eb17cbd,	0xe7b82d07,
	0x90bf1d91,	0x1db71064,	0x6ab020f2,	0xf3b97148,	0x84be41de,
	0x1adad47d,	0x6ddde4eb,	0xf4d4b551,	0x83d385c7,	0x136c9856,
	0x646ba8c0,	0xfd62f97a,	0x8a65c9ec,	0x14015c4f,	0x63066cd9,
	0xfa0f3d63,	0x8d080df5,	0x3b6e20c8,	0x4c69105e,	0xd56041e4,
	0xa2677172,	0x3c03e4d1,	0x4b04d447,	0xd20d85fd,	0xa50ab56b,
	0x35b5a8fa,	0x42b2986c,	0xdbbbc9d6,	0xacbcf940,	0x32d86ce3,
	0x45df5c75,	0xdcd60dcf,	0xabd13d59,	0x26d930ac,	0x51de003a,
	0xc8d75180,	0xbfd06116,	0x21b4f4b5,	0x56b3c423,	0xcfba9599,
	0xb8bda50f,	0x2802b89e,	0x5f058808,	0xc60cd9b2,	0xb10be924,
	0x2f6f7c87,	0x58684c11,	0xc1611dab,	0xb6662d3d,	0x76dc4190,
	0x01db7106,	0x98d220bc,	0xefd5102a,	0x71b18589,	0x06b6b51f,
	0x9fbfe4a5,	0xe8b8d433,	0x7807c9a2,	0x0f00f934,	0x9609a88e,
	0xe10e9818,	0x7f6a0dbb,	0x086d3d2d,	0x91646c97,	0xe6635c01,
	0x6b6b51f4,	0x1c6c6162,	0x856530d8,	0xf262004e,	0x6c0695ed,
	0x1b01a57b,	0x8208f4c1,	0xf50fc457,	0x65b0d9c6,	0x12b7e950,
	0x8bbeb8ea,	0xfcb9887c,	0x62dd1ddf,	0x15da2d49,	0x8cd37cf3,
	0xfbd44c65,	0x4db26158,	0x3ab551ce,	0xa3bc0074,	0xd4bb30e2,
	0x4adfa541,	0x3dd895d7,	0xa4d1c46d,	0xd3d6f4fb,	0x4369e96a,
	0x346ed9fc,	0xad678846,	0xda60b8d0,	0x44042d73,	0x33031de5,
	0xaa0a4c5f,	0xdd0d7cc9,	0x5005713c,	0x270241aa,	0xbe0b1010,
	0xc90c2086,	0x5768b525,	0x206f85b3,	0xb966d409,	0xce61e49f,
	0x5edef90e,	0x29d9c998,	0xb0d09822,	0xc7d7a8b4,	0x59b33d17,
	0x2eb40d81,	0xb7bd5c3b,	0xc0ba6cad,	0xedb88320,	0x9abfb3b6,
	0x03b6e20c,	0x74b1d29a,	0xead54739,	0x9dd277af,	0x04db2615,
	0x73dc1683,	0xe3630b12,	0x94643b84,	0x0d6d6a3e,	0x7a6a5aa8,
	0xe40ecf0b,	0x9309ff9d,	0x0a00ae27,	0x7d079eb1,	0xf00f9344,
	0x8708a3d2,	0x1e01f268,	0x6906c2fe,	0xf762575d,	0x806567cb,
	0x196c3671,	0x6e6b06e7,	0xfed41b76,	0x89d32be0,	0x10da7a5a,
	0x67dd4acc,	0xf9b9df6f,	0x8ebeeff9,	0x17b7be43,	0x60b08ed5,
	0xd6d6a3e8,	0xa1d1937e,	0x38d8c2c4,	0x4fdff252,	0xd1bb67f1,
	0xa6bc5767,	0x3fb506dd,	0x48b2364b,	0xd80d2bda,	0xaf0a1b4c,
	0x36034af6,	0x41047a60,	0xdf60efc3,	0xa867df55,	0x316e8eef,
	0x4669be79,	0xcb61b38c,	0xbc66831a,	0x256fd2a0,	0x5268e236,
	0xcc0c7795,	0xbb0b4703,	0x220216b9,	0x5505262f,	0xc5ba3bbe,
	0xb2bd0b28,	0x2bb45a92,	0x5cb36a04,	0xc2d7ffa7,	0xb5d0cf31,
	0x2cd99e8b,	0x5bdeae1d,	0x9b64c2b0,	0xec63f226,	0x756aa39c,
	0x026d930a,	0x9c0906a9,	0xeb0e363f,	0x72076785,	0x05005713,
	0x95bf4a82,	0xe2b87a14,	0x7bb12bae,	0x0cb61b38,	0x92d28e9b,
	0xe5d5be0d,	0x7cdcefb7,	0x0bdbdf21,	0x86d3d2d4,	0xf1d4e242,
	0x68ddb3f8,	0x1fda836e,	0x81be16cd,	0xf6b9265b,	0x6fb077e1,
	0x18b74777,	0x88085ae6,	0xff0f6a70,	0x66063bca,	0x11010b5c,
	0x8f659eff,	0xf862ae69,	0x616bffd3,	0x166ccf45,	0xa00ae278,
	0xd70dd2ee,	0x4e048354,	0x3903b3c2,	0xa7672661,	0xd06016f7,
	0x4969474d,	0x3e6e77db,	0xaed16a4a,	0xd9d65adc,	0x40df0b66,
	0x37d83bf0,	0xa9bcae53,	0xdebb9ec5,	0x47b2cf7f,	0x30b5ffe9,
	0xbdbdf21c,	0xcabac28a,	0x53b39330,	0x24b4a3a6,	0xbad03605,
	0xcdd70693,	0x54de5729,	0x23d967bf,	0xb3667a2e,	0xc4614ab8,
	0x5d681b02,	0x2a6f2b94,	0xb40bbe37,	0xc30c8ea1,	0x5a05df1b,
	0x2d02ef8d
};
#define	DO1(buf) crc = sf_crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define	DO2(buf) do {DO1(buf);	DO1(buf); } while (0)
#define	DO4(buf) do {DO2(buf);	DO2(buf); } while (0)
#define	DO8(buf) do {DO4(buf);	DO4(buf); } while (0)

static unsigned long uboot_crc32(u32 crc, unsigned char const *buf, size_t len)
{
	crc = crc ^ 0xffffffff;
	while (len >= 8) {
		DO8(buf);
		len	-= 8;
	}

	if (len) {
		do {
			DO1(buf);
		} while (--len);
	}
	return crc ^ 0xffffffff;
}

#define SPI_FLASH_BASE	0xfff80000
#define ENV_MAX_SIZE	SZ_64K
#define ENV_DATA_SIZE	(ENV_MAX_SIZE - sizeof(unsigned int))
#define ENV1 0
#define ENV2 1
#define MAX_NAME_SIZE   256
#define MAX_VALUE_SIZE  (4*1024)

struct env_t {
	unsigned long crc;    /* CRC32 over data bytes */
	unsigned char data[ENV_DATA_SIZE];
};

struct uboot_env {
	struct env_t *env[2];
	bool   env_readed;
	bool   env_init;

	// raw
	void *io_base;
	uint32_t offset[2];
	size_t size[2];
};

static struct uboot_env *uboot_env;

//GPIO_BASE_ADDR+0x100 should be bootstrap gpio, however, this is not true on some device.
//As all wm8880 device boot from spi, i comment this out 
//static inline int boot_type(void)
//{
//	uint32_t val = *((volatile unsigned int *)(GPIO_BASE_ADDR + 0x100));
//
//	val = (val >> 1) & 0x3;
//	switch (val) {
//	case 0:
//		return SPI_FLASH_TYPE;
//	case 1:
//		return NAND_FLASH_TYPE;
//	case 2:
//		return NOR_FLASH_TYPE;
//	}
//	return -EINVAL;
//}

static inline unsigned char env_get_char(int type, int index)
{
	return uboot_env->env[type]->data[index];
}

/*
 * Match a name / name=value pair
 *
 * s1 is either a simple 'name', or a 'name=value' pair.
 * i2 is the environment index for a 'name2=value2' pair.
 * If the names match, return the index for the value2, else NULL.
 */
static int envmatch(int type, unsigned char *s1, int i2)
{
	while (*s1 == env_get_char(type, i2++))
		if (*s1++ == '=')
			return i2;
	if (*s1 == '\0' && env_get_char(type, i2-1) == '=')
		return i2;
	return -1;
}


/*
 * raw interface
 */
static int raw_uboot_env_erase(int type)
{
	uint32_t offset;
	int rc;

	offset = uboot_env->offset[type]+0xfff80000-MTDSF_PHY_ADDR;
    
	rc = spi_flash_sector_erase(offset, (struct sfreg_t *)SF_BASE_ADDR);
	if (rc != ERR_OK) {
		pr_err("spi_flash_sector_erase failed, try again\n");
        rc = spi_flash_sector_erase(offset, (struct sfreg_t *)SF_BASE_ADDR);
        if(rc != ERR_OK){
            pr_err("spi_flash_sector_erase failed again\n");
		    return -EIO;
        }
	}
	return 0;
}

static int raw_uboot_env_write(int type,struct env_t *env)
{
	uint32_t offset;
	void *io_base = uboot_env->io_base;
	int rc;

	offset = uboot_env->offset[type];

	rc = spi_flash_sector_write(((struct sfreg_t *)SF_BASE_ADDR),
				    io_base,
				    offset,
				    ENV_MAX_SIZE,
				    (u_char *)env);
	if (rc != ENV_MAX_SIZE)
		pr_err("spi_flash_sector_write failed: 0x%x\n", rc);
	
	return 0;
}


static int raw_uboot_env_read(void)
{
	unsigned long crc32;
	int i;

    //REG32_VAL(PMCEU_ADDR) |= SF_CLOCK_EN;
	//wmt_sfc_init((struct sfreg_t *)SF_BASE_ADDR);

	// ubootenv 
	for (i = 0; i < 2; ++i) {
		struct env_t *env = uboot_env->env[i];
		uint32_t offset = uboot_env->offset[i];

		_memcpy_fromio((void *)env, uboot_env->io_base + offset, ENV_MAX_SIZE);
		crc32 = uboot_crc32(0, env->data, ENV_DATA_SIZE);
		if (env->crc != crc32) {
			pr_err("ERROR:crc32 0x%lx, env->crc 0x%lx ????\n\n", crc32, env->crc);
			if (i == 0)	// uboot env must pass crc32
            {
				return -EINVAL;
            }
		}
	}
    //REG32_VAL(PMCEU_ADDR) &= ~(SF_CLOCK_EN);
	return 0;
}

static int save_env(int index, struct env_t *env)
{
	int ret;

	if(env == NULL || index > 2|| index < 0)
		return -1;

	env->crc = uboot_crc32(0, env->data, ENV_DATA_SIZE);

	ret = raw_uboot_env_erase(index);
	if (ret)
		return ret;

	raw_uboot_env_write(index,env);

	return 0;

}


static int env_init_if_needed(void)
{
	int i;

	//if (boot_type() != SPI_FLASH_TYPE){
    //   pr_err("unsupported boot type!");
    //   return -EINVAL;
    //}

	if (!uboot_env) {
		uboot_env = kzalloc(sizeof(*uboot_env), GFP_KERNEL);
		if (!uboot_env){
            pr_err("out of memory!\n");
			return -ENOMEM;
        }

		for (i = 0; i < 2; ++i) {
			uboot_env->env[i] = kmalloc(ENV_MAX_SIZE, GFP_KERNEL);
			if (!uboot_env->env[i]){
                pr_err("out of memory!\n");            
				return -ENOMEM;
            }
		}
	}

	if (!uboot_env->io_base) {
		uboot_env->io_base = (void *)ioremap(0xFFF80000, SZ_512K);
		if (!uboot_env->io_base) {
			printk(KERN_WARNING "uboot_env ioremap fail\n");
			return -EIO;
		}
		uboot_env->offset[0] = 0x50000; // 0xfffd0000
		uboot_env->size[0] = 0x10000;
		uboot_env->offset[1] = 0x60000; // 0xfffe0000
		uboot_env->size[1] = 0x10000;
	}

	if(!uboot_env->env_init){
		wmt_sfc_init((struct sfreg_t *)SF_BASE_ADDR);
		uboot_env->env_init = true;
	}
	// read the uboot env only once
	if (uboot_env->env_readed == false) {
		if (raw_uboot_env_read()){
            pr_err("read env fail!\n");        
			return -EINVAL;
        }
		uboot_env->env_readed = true;
	}
	return 0;
}

/*
 * return 0 have env
 * return others no such env 
 */
int search_env(int index, char *varname)
{
	int i, j, k, nxt = 0;
	int rcode = 0;

	if (env_init_if_needed()){
		rcode = -EIO;
		goto out;
	}

	k = -1;
	i = 0;
	for (j = 0; env_get_char(index, j) != '\0'; j = nxt+1) {

		for (nxt = j; env_get_char(index, nxt) != '\0'; ++nxt)
			;
		k = envmatch(index, (unsigned char *)varname, j);
		if (k < 0)
			continue;
		break;
	}
	
	if (k < 0) {
		rcode++;
	}
	
out:
	return rcode;
}

/*
 * insert env to env buf 
 */
int insert_env(int index, char *varname, char *varval)
{
	int len, oldval;
	int rcode = 0;
	unsigned char *env, *nxt = NULL;
	unsigned char *env_data;

	if(*varname == '\0'|| index > 2 || index < 0)
		return -EINVAL;

	env_data = uboot_env->env[index]->data;

	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	for (env = env_data; *env; env = nxt+1) {
		for (nxt = env; *nxt; ++nxt)
			;
		oldval = envmatch(index, (unsigned char *)varname, env - env_data);
		if (oldval >= 0)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval >= 0) {
		/* otp env can not overwrite */
		//if (is_otp_env) {
		//	rcode = -EEXIST;
		//	goto out;
		//}

		if (*++nxt == '\0') {
			if (env > env_data)
				env--;
			else
				*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	if ((varval == NULL) || (*varval == '\0')) {
		if (oldval < 0) {
			pr_info("No assigned any value for %s\n", varname);
			rcode++;
		} else {
			rcode = 0;
		}
		goto out;
	}

	/*
	 * Append new definition at the end
	 */
	for (env = env_data; *env || *(env+1); ++env)
		;
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "varname" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(varname) + 2;
	/* add '=' for first arg, ' ' for all others */
	len += strlen(varval);
	if (len > (&env_data[ENV_MAX_SIZE]-env)) {
		printk(KERN_WARNING "## Warning: environment overflow, \"%s\" deleted\n",
		       varname);
		rcode++;
		goto out;
	}
	while ((*env = *varname++) != '\0')
		env++;

	*env = '=';
	while ((*++env = *varval++) != '\0')
		;

	/* end is marked with double '\0' */
	*++env = '\0';

	rcode = 0;

out:

	return rcode;
}

int is_persist(char *name)
{
    int i, len;
    char *persistlist[]={"otp.", "ethaddr", "wmt.ethaddr.persist", "androidboot.serialno", 
                    "btaddr", "wmt.btaddr.persist","pcba.serialno","serialnum","persist.", NULL};

    for(i=0; persistlist[i] != NULL; i++){
        len = strlen(persistlist[i]);
        if(!strncmp(name, persistlist[i], len))
            return 0;       
    }

    return -1;    
}

/* 
 * sync env2's persist to env1, then update to env1 and env2
 */
int sync_persist_env(struct env_t *env1, struct env_t *env2)
{   
    int i;
    int updated=0;
    unsigned char name[MAX_NAME_SIZE] = {0};
    unsigned char *val = NULL,*valbuf=NULL;   
    unsigned char *s;
	
	valbuf = kzalloc(MAX_VALUE_SIZE,GFP_KERNEL);
	if(!valbuf){
		printk("alloc mem failed!\n");
		return -ENOMEM;
	}
	
    for(s = env2->data; s < (env2->data+ENV_DATA_SIZE) && *s!='\0'; ){
        
        if(is_persist(s)==0){
            i=0;
            while(*s != '=' && *s != '\0' && i < (sizeof(name)-1)) 
				name[i++] = *s++;            
            
			name[i] = '\0';
            
            i=0;
            s++;//skip '='			
			val = valbuf;
            while(*s != '\0') 
				val[i++] = *s++;
				
            val[i] = '\0';
            s++;
            //printk("%s=%s\n",name,val);

            if(search_env(ENV1,name)){
				printk("insert %s=%s to env1\n",name,val);
                insert_env(ENV1,name,val);
                //updated ++;
            }
        }
        else{        
            s += (strlen(s)+1);
        }
    }

	//printk("sync %d otps to env1\n",updated);
	save_env(ENV1,env1);
	save_env(ENV2,env1);
	memcpy(env2,env1,sizeof(struct env_t));
	kfree(valbuf);

    return 0;
}


/*
 * sync and update env to SF
 */
static int update_env(int to_update)
{
	struct env_t *env1 = uboot_env->env[ENV1];
	struct env_t *env2 = uboot_env->env[ENV2];

	if(!to_update){
		return 0;
	}
	
	if(uboot_env->env_readed)
		return sync_persist_env(env1,env2);
		
	return -EIO;
}

static int esync(void)
{
	int i;
	int ret;
	u32 crc1,crc2;
	char *dest,*src;
	struct env_t *env1,*env2;

	/* copy env raw data */
	for(i = 0; i < 2; i++){
		dest = (char *)uboot_env->env[i];
		src = (char *)(uboot_env->offset[i] + uboot_env->io_base);
		memset(dest, 0x00, ENV_MAX_SIZE);
		sf_copy_env(dest, src, ENV_MAX_SIZE);
	}
	
	env1 = uboot_env->env[ENV1];
	crc1 = uboot_crc32(0, env1->data, ENV_DATA_SIZE);
	
	env2 = uboot_env->env[ENV2];
	crc2 = uboot_crc32(0, env2->data, ENV_DATA_SIZE);
	
	printk("crc1:%08x,%08x; crc2:%08x,%08x\n", env1->crc,crc1,env2->crc,crc2);

	if( crc1 == env1->crc && crc2 == env2->crc && crc1 == crc2){
		printk("env1==env2\n");
	}else if(crc1 != env1->crc && crc2  == env2->crc){
		printk("env2->env1\n");
		return save_env(ENV1, env2);
	}else if(crc1 == env1->crc && crc2 != env2->crc ){
		printk("env1->env2\n");
		return save_env(ENV2, env1);
	}else if(crc1 == env1->crc && crc2 == env2->crc && crc1 != crc2 ){
		printk("env1<->env2\n");
		return sync_persist_env(env1,env2);
	}else{
		printk("env1,env2 invalid\n");
	}
	
	return 0;
}

/* Get the system parameter if existed.
 *
 * - varname: parameter name
 * - varval : a buffer to store the parameter
 * - varlen : the buffer size for the varval pointer
 *
 * return 0 if success.
 */
int wmt_getsyspara(char *varname, unsigned char *varval, int *varlen)
{
	int i, j, k, nxt = 0;
	int rcode = 0;

	int ret = down_interruptible(&s_wmt_env_lock);
	if (ret) {
		printk(KERN_WARNING "lock s_wmt_env_lock error: %d\n", ret);
		return -EAGAIN;
	}

	if (env_init_if_needed()){
		rcode = -EIO;
		goto out;
	}

	k = -1;
	i = 0;
	for (j = 0; env_get_char(ENV1, j) != '\0'; j = nxt+1) {

		for (nxt = j; env_get_char(ENV1, nxt) != '\0'; ++nxt)
			;
		k = envmatch(ENV1, (unsigned char *)varname, j);
		if (k < 0)
			continue;
		while (k < nxt && i < *varlen)
			varval[i++] = env_get_char(ENV1, k++);
		if( k == nxt)
			varval[i] = '\0';
		break;
	}

	if (k < nxt && k > 0) {
		printk(KERN_WARNING "## Warning: \"%s\" size(%d) exceed buffer size(%d)\n",
		       varname, i+(nxt-k), *varlen);
		*varlen = i+(nxt-k);
		rcode = 10;
		goto out;
	}

	if (k < 0) {
		rcode++;
	}
out:
	up(&s_wmt_env_lock);
	return rcode;
}
EXPORT_SYMBOL_GPL(wmt_getsyspara);

/* Set the system parameter.
 *
 * - varname: parameter name
 * - varval : the buffer to store the system parameter value for setting.
 *
 *   If the pointer is NULL and the system parameter is existed,
 *   then the system parameter will be clear.
 *
 * return 0 if success.
 */
int wmt_setsyspara(char *name, char *varval)
{
	int len, oldval;
	int rcode = 0;
	unsigned char *env, *nxt = NULL;
	unsigned char *env_data;
	int is_otp_env;
	int do_wsf = 1;
	unsigned char *varname = name;

	if(*varname == '\0')
		return -EINVAL;

	int ret = down_interruptible(&s_wmt_env_lock);
	if (ret) {
		printk(KERN_WARNING "lock s_wmt_env_lock error: %d\n", ret);
		return -EAGAIN;
	}
	
	/*
	 * parameter name start with '~' is store in buf
	 */
	if(*varname == '~'){
		do_wsf = 0;
		varname++;		
	}

	if (env_init_if_needed()){
		rcode = -EIO;
		goto out;
	}
	
	if(!strcmp(varname,"esync")){
		rcode = esync();	
		goto out;
	}

	is_otp_env = !strncmp(varname, "otp.", 4);

	if( !is_otp_env && wmt_is_secure_enabled() ) {
		//check white list for u-boot env
		int i;
		for( i = 0; env_white_list[i]; i++) {
			if( !strcmp(varname, env_white_list[i]))
				break;
		}
		if(!env_white_list[i]) {
			printk("Not in env white list, disable write <%s>\n", varname);
			rcode = -EPERM;
			goto out;
		}
	}

	if (strcmp(varname, "boot-method") == 0) {
		if( strcmp(varval, "boot-nand-ota-normal") && 
		    strcmp(varval, "boot-nand-ota-recovery")&&
		    strcmp(varval, "boot-nand-otz-normal") && 
		    strcmp(varval, "boot-nand-otz-recovery")) {
			printk("boot-method unsupported varval: %s\n", varval);
			rcode = -EINVAL;
			goto out;
		}
	}

	env_data = uboot_env->env[ENV1]->data;

	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	for (env = env_data; *env; env = nxt+1) {
		for (nxt = env; *nxt; ++nxt)
			;
		oldval = envmatch(ENV1, (unsigned char *)varname, env - env_data);
		if (oldval >= 0)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval >= 0) {
		/* otp env can not overwrite */
		if (is_otp_env) {
			rcode = -EEXIST;
			printk("Warning:OTP env can not overwrite!\n");
			goto out;
		}

		if (*++nxt == '\0') {
			if (env > env_data)
				env--;
			else
				*env = '\0';
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

	if ((varval == NULL) || (*varval == '\0')) {
		if (oldval < 0) {
			pr_info("No assigned any value for %s\n", varname);
			rcode++;
		} else {
			/*
			 * varname will be clear
			 */
			pr_info("Delete environment variable: %s\n", varname);
			if (update_env(do_wsf))
				rcode++;
			else
				rcode = 0;
		}
		goto out;
	}

	/*
	 * Append new definition at the end
	 */
	for (env = env_data; *env || *(env+1); ++env)
		;
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "varname" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(varname) + 2;
	/* add '=' for first arg, ' ' for all others */
	len += strlen(varval);
	if (len > (&env_data[ENV_MAX_SIZE]-env)) {
		printk(KERN_WARNING "## Warning: environment overflow, \"%s\" deleted\n",
		       varname);
		rcode++;
		goto out;
	}
	while ((*env = *varname++) != '\0')
		env++;

	*env = '=';
	while ((*++env = *varval++) != '\0')
		;

	/* end is marked with double '\0' */
	*++env = '\0';

	if (update_env(do_wsf))
		rcode++;
	else
		rcode = 0;

out:
	up(&s_wmt_env_lock);
	return rcode;
}
EXPORT_SYMBOL_GPL(wmt_setsyspara);

/*
 * Get the WMT SoC chipid & bondingid.
 */
int wmt_getsocinfo(unsigned int *chipid, unsigned int *bondingid)
{
	*chipid = SCC_CHIP_ID_ADDR;
	*bondingid = BONDING_OPTION_4BYTE_ADDR;
	return 0;
}
EXPORT_SYMBOL_GPL(wmt_getsocinfo);

int wmt_is_secure_enabled(void)
{
	static int secure_enabled = 0;

	if (secure_enabled == 0) {
		char value[512] = {'\0',};
		int len = 511;
		if (wmt_getsyspara("otp.rsa.pem", value, &len) == 0 && len > 0) {
			secure_enabled = 1;
		}
		else {
			secure_enabled = -1;
		}
	}
	return secure_enabled == 1 ? 1 : 0;
}
EXPORT_SYMBOL_GPL(wmt_is_secure_enabled);

static int do_rsa(uint8_t *sig_data, size_t sig_len, uint8_t *publickey,
		  uint8_t *hash_signature)
{
	uint8_t out_buf[128], tmp;
	int ret = 0;
	int i, j, k;

	ret = rsa_check((unsigned int)publickey, strlen(publickey),
			(uint32_t)sig_data, sig_len, out_buf);
	if (ret) {
		printk("decode signature fail\n");
		return 2;
	}

	for (i = 0, j = 0; i < 64; i=i+2,j++) {
		tmp = 0;
		for (k = 0; k < 2; k++) {
			if (out_buf[i+k] == '0')
				tmp += ((k == 0) ?(0<<4):0);
			else if (out_buf[i+k] == '1')
				tmp += ((k == 0) ?(1<<4):1);
			else if (out_buf[i+k] == '2')
				tmp += ((k == 0) ?(2<<4):2);
			else if (out_buf[i+k] == '3')
				tmp += ((k == 0) ?(3<<4):3);
			else if (out_buf[i+k] == '4')
				tmp += ((k == 0) ?(4<<4):4);
			else if (out_buf[i+k] == '5')
				tmp += ((k == 0) ?(5<<4):5);
			else if (out_buf[i+k] == '6')
				tmp += ((k == 0) ?(6<<4):6);
			else if (out_buf[i+k] == '7')
				tmp += ((k == 0) ?(7<<4):7);
			else if (out_buf[i+k] == '8')
				tmp += ((k == 0) ?(8<<4):8);
			else if (out_buf[i+k] == '9')
				tmp += ((k == 0) ?(9<<4):9);
			else if (out_buf[i+k] == 'a')
				tmp += ((k == 0) ?(0xa<<4):0xa);
			else if (out_buf[i+k] == 'b')
				tmp += ((k == 0) ?(0xb<<4):0xb);
			else if (out_buf[i+k] == 'c')
				tmp += ((k == 0) ?(0xc<<4):0xc);
			else if (out_buf[i+k] == 'd')
				tmp += ((k == 0) ?(0xd<<4):0xd);
			else if (out_buf[i+k] == 'e')
				tmp += ((k == 0) ?(0xe<<4):0xe);
			else if (out_buf[i+k] == 'f')
				tmp += ((k == 0) ?(0xf<<4):0xf);
			else {
				printk("change from character to digit fail out_buf[%d]=%c\n", i, out_buf[i]);
				ret = 3;
				break;
			}
		}
		if (ret == 3)
			break;
		hash_signature[j] = tmp;
	}

	return ret;
}

static int do_hash(uint8_t *buf, size_t len, unsigned char sha256sum[32])
{
	sha256_context ctx;

	sha256_starts(&ctx);

	sha256_update(&ctx, buf, len);

	sha256_finish(&ctx, sha256sum);

#ifdef DEBUG
	{
		int j;
		for (j = 0; j < 32; j++) {
			printk( "%02x", sha256sum[j] );
		}
		printk("\n");
	}
#endif
	return 0;
}

int wmt_write_signed_image(struct write_signed_image *w)
{
	uint8_t hash_sig[64], hash_img[32];
	char publickey[400];
	size_t len = sizeof(publickey);
	uint32_t offset;
	size_t size;
	void *io_base;
	int rc = 0, i;

    
	if (wmt_getsyspara("otp.rsa.pem", publickey, &len) == 0) {

		if (do_rsa(w->sig_data, w->sig_len, publickey, hash_sig)) {
			printk("do rsa failed\n");
			return -1;
		}

        
        
		if (do_hash(w->img_data, w->img_len, hash_img)) {
			printk("do hash failed\n");
			return -2;
		}

		if (memcmp(hash_sig, hash_img, 32)) {
			for (i = 0; i < 32; i++)
				printk("%2.2x", hash_sig[i]);
			printk("\n");
			for (i = 0; i < 32; i++)
				printk("%2.2x", hash_img[i]);
			printk("\n image check fail\n");
			return -3;
		}
		pr_info("Decrypto signature success\n");
	} else
		pr_info("otp.rsa.pem not found\n");

	// update
    
	switch (w->type) {
	case SIGNED_IMAGE_TYPE_WLOAD:		// "w-load-SF",
		offset	= 0x00070000;
		size	= 0x00010000;
		break;
	case SIGNED_IMAGE_TYPE_UBOOT:		// "u-boot-SF",
		offset	= 0x00000000;
		size	= 0x00050000;
		break;
	case SIGNED_IMAGE_TYPE_UBOOT_ENV:	// "u-boot env. cfg. 1-SF",
		offset	= 0x00050000;
		size	= 0x00010000;
		break;
	default:
		return -EINVAL;
	}

	if (w->img_len > size) {
		printk(" max size 0x%x\n", size);
		return -E2BIG;
	}

	rc = down_interruptible(&s_wmt_env_lock);
	if (rc) {
		printk(KERN_WARNING "lock s_wmt_env_lock error: %d\n", rc);
		return -EAGAIN;
	}

	// Erase
	for (len = 0; len < size; len += 0x10000) {
		printk("  Erase flash 0x%x\n", offset + len);

		rc = spi_flash_sector_erase(offset + len,
					    (struct sfreg_t *)SF_BASE_ADDR);
		if (rc != ERR_OK) {
			pr_err("spi_flash_sector_erase failed\n");
			rc = -EIO;
			goto out;
		}
	}

	// Write
	io_base = uboot_env->io_base;
	
        spi_flash_sector_write(((struct sfreg_t *)SF_BASE_ADDR),
                           io_base, offset, w->img_len, w->img_data);
        
out:
	up(&s_wmt_env_lock);
	return rc;
}


/*
 *	reload the env from partition.
 *	After Hibernation restore, call this function since
 *	the env may has been changed before restoration.
 *	This function is considered only be called by hibernation related code.
 *	Do not call this function from different kernel thread at the same time.
 */
int env_cache_flush(void)
{
	int i;
	int ret;
	u32 crc1,crc2;
	char *dest,*src;
	struct env_t *env1,*env2;

	/* copy env raw data */
	for(i = 0; i < 2; i++){
		dest = (char *)uboot_env->env[i];
		src = (char *)(uboot_env->offset[i] + uboot_env->io_base);
		memset(dest, 0x00, ENV_MAX_SIZE);
		sf_copy_env(dest, src, ENV_MAX_SIZE);
	}
	
	env1 = uboot_env->env[ENV1];
	crc1 = uboot_crc32(0, env1->data, ENV_DATA_SIZE);
	
	env2 = uboot_env->env[ENV2];
	crc2 = uboot_crc32(0, env2->data, ENV_DATA_SIZE);
	
	if(crc1 != env1->crc){
		printk("Error:env1 crc error!");
		return 1;
	}
	
	if(crc2 != env2->crc)
		printk("Warning:env2 crc error!");

	return 0;
}
