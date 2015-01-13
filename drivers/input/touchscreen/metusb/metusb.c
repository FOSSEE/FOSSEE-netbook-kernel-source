
//#define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/syscalls.h>


#define DRIVER_VERSION		"v1.0"
#define DRIVER_AUTHOR		"Xiaoyijian"
#define DRIVER_DESC		"Metouch USB Touchscreen Driver"

static int swap_xy = 0;
static int swapx=0;
static int swapy=0;

static int v_shift=0;
static int v_flag=0;

static int h_shift=0;
static int h_flag=0;


#define TP_TIMEOUT	30
#define TP_TIMEROUT_MAX 3

/* device specifc data/functions */
struct usbtouch_usb;
struct usbtouch_device_info {
	int min_xc, max_xc;
	int min_yc, max_yc;
	int min_press, max_press;
	int rept_size;

	/*
	 * Always service the USB devices irq not just when the input device is
	 * open. This is useful when devices have a watchdog which prevents us
	 * from periodically polling the device. Leave this unset unless your
	 * touchscreen device requires it, as it does consume more of the USB
	 * bandwidth.
	 */
	bool irq_always;

	void (*process_pkt) (struct usbtouch_usb *usbtouch, unsigned char *pkt, int len);

	/*
	 * used to get the packet len. possible return values:
	 * > 0: packet len
	 * = 0: skip one byte
	 * < 0: -return value more bytes needed
	 */
	int  (*get_pkt_len) (unsigned char *pkt, int len);

	int  (*read_data)   (struct usbtouch_usb *usbtouch, unsigned char *pkt);
	int  (*alloc)       (struct usbtouch_usb *usbtouch);
	int  (*init)        (struct usbtouch_usb *usbtouch);
	void (*exit)	    (struct usbtouch_usb *usbtouch);
};

/* a usbtouch device */
struct usbtouch_usb {
	unsigned char *data;
	dma_addr_t data_dma;
	unsigned char *buffer;
	int buf_len;
	struct urb *irq;
	struct usb_interface *interface;
	struct input_dev *input;
	
	struct workqueue_struct *tp_queue;
	struct delayed_work	tp_work;
	struct mutex tp_timeout_mutex;
	int tp_timer_count;
	
	struct usbtouch_device_info *type;
	char name[128];
	char phys[64];
	void *priv;
	int x, y;
	int touch, press;
};

#define DEVTYPE_METOUCH 0

#define USB_DEVICE_HID_CLASS(vend, prod) \
	.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS \
		| USB_DEVICE_ID_MATCH_DEVICE, \
	.idVendor = (vend), \
	.idProduct = (prod), \
	.bInterfaceClass = USB_INTERFACE_CLASS_HID, \
	.bInterfaceProtocol = USB_INTERFACE_PROTOCOL_MOUSE

/* Define these values to match your devices */
#define METOUCH_VENDOR_ID	0x5A53
#define METOUCH_PRODUCT_ID	0x0001
#define METUSB_MINOR_BASE       0x0

static int metouch_read_data(struct usbtouch_usb *dev, unsigned char *pkt);
static void usbtouch_process_pkt(struct usbtouch_usb *usbtouch,unsigned char *pkt, int len);
static void usbtouch_irq(struct urb *urb);
static int usbtouch_open(struct input_dev *input);
static void usbtouch_close(struct input_dev *input);
static int usbtouch_suspend(struct usb_interface *intf, pm_message_t message);
static int usbtouch_resume(struct usb_interface *intf);
static int usbtouch_reset_resume(struct usb_interface *intf);
static void usbtouch_free_buffers(struct usb_device *udev,struct usbtouch_usb *usbtouch);
static struct usb_endpoint_descriptor *usbtouch_get_input_endpoint(struct usb_host_interface *interface);
static int usbtouch_probe(struct usb_interface *intf,const struct usb_device_id *id);
static void usbtouch_disconnect(struct usb_interface *intf);
static int __init usbtouch_init(void);
static void __exit usbtouch_cleanup(void);
static void GetUserCfg(void);
static void AnlysCmd(char *cmd);
static int myatoi(char *str);
static void DeleteAllSpace(char *cmd);
int mypow(int t);


static struct usbtouch_device_info usbtouch_dev_info[] = {
	[DEVTYPE_METOUCH] = {
		.min_xc		= 0x0,
		.max_xc		= 0x0fff,
		.min_yc		= 0x0,
		.max_yc		= 0x0fff,
		.rept_size	= 8,
		.read_data	= metouch_read_data,
	},
};

static struct usb_device_id metusb_table [] = {
	{ USB_DEVICE(METOUCH_VENDOR_ID, METOUCH_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, metusb_table);

/*****************************************************************************
 * METOUCH Part
 */
/*
AA 55 XL XH YL YH BTN CRC
****************************************************************************/
static int metouch_read_data(struct usbtouch_usb *dev, unsigned char *pkt)
{
	dev->x = (pkt[3] << 8) | pkt[2];
	dev->y = (pkt[5] << 8) | pkt[4];
	dev->touch = (pkt[6] & 0x03) ? 1 : 0;
        
        if (h_flag == 0) dev->x = dev->x + h_shift;
        if (v_flag == 0) dev->y = dev->y + v_shift;
        
        if (h_flag>0) 
               {
               if (dev->x > h_shift) dev->x = dev->x - h_shift;
               else dev->x=0;
               }          

        if (v_flag>0) 
               {
               if (dev->y > v_shift) dev->y = dev->y - v_shift;
               else dev->y=0;
               }          
    
        if (dev->x > 4095) dev->x=4095;
        if (dev->y > 4095) dev->y=4095;

        if (dev->x <0) dev->x=0;
        if (dev->y <0) dev->y=0;

        if (swapx>0) dev->x = 4095 - dev->x;
        if (swapy>0) dev->y = 4095 - dev->y;

	return 1;
}


/*****************************************************************************
 * Generic Part
 */
static void usbtouch_process_pkt(struct usbtouch_usb *usbtouch,
                                 unsigned char *pkt, int len)
{
	struct usbtouch_device_info *type = usbtouch->type;

	if (!type->read_data(usbtouch, pkt))
			return;

	mutex_lock(&usbtouch->tp_timeout_mutex);
	if( usbtouch->tp_timer_count < 0 ){
		queue_delayed_work(usbtouch->tp_queue, &usbtouch->tp_work, msecs_to_jiffies(TP_TIMEOUT));
	}
	usbtouch->tp_timer_count = TP_TIMEROUT_MAX;
	mutex_unlock(&usbtouch->tp_timeout_mutex);

	//input_report_key(usbtouch->input, BTN_TOUCH, usbtouch->touch);
	input_report_abs(usbtouch->input, ABS_MT_TRACKING_ID, 0);
	if (swap_xy) {
		//input_report_abs(usbtouch->input, ABS_X, usbtouch->y);
		//input_report_abs(usbtouch->input, ABS_Y, usbtouch->x);
		input_report_abs(usbtouch->input, ABS_MT_POSITION_X, usbtouch->y );
		input_report_abs(usbtouch->input, ABS_MT_POSITION_Y, usbtouch->x );
	} else {
		//input_report_abs(usbtouch->input, ABS_X, usbtouch->x);
		//input_report_abs(usbtouch->input, ABS_Y, usbtouch->y);
		input_report_abs(usbtouch->input, ABS_MT_POSITION_X, usbtouch->x );
		input_report_abs(usbtouch->input, ABS_MT_POSITION_Y, usbtouch->y );		
	}
	//if (type->max_press)
	//	input_report_abs(usbtouch->input, ABS_PRESSURE, usbtouch->press);
	//input_sync(usbtouch->input);
	input_mt_sync(usbtouch->input);
	input_sync(usbtouch->input);
	


}


static void usbtouch_irq(struct urb *urb)
{
	struct usbtouch_usb *usbtouch = urb->context;
	int retval;

	switch (urb->status) {
	case 0:
		/* success */
		break;
	case -ETIME:
		/* this urb is timing out */
		dbg("%s - urb timed out - was the device unplugged?",
		    __func__);
		return;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
	case -EPIPE:
		/* this urb is terminated, clean up */
		dbg("%s - urb shutting down with status: %d",
		    __func__, urb->status);
		return;
	default:
		dbg("%s - nonzero urb status received: %d",
		    __func__, urb->status);
		goto exit;
	}

	usbtouch->type->process_pkt(usbtouch, usbtouch->data, urb->actual_length);

exit:
	usb_mark_last_busy(interface_to_usbdev(usbtouch->interface));
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		err("%s - usb_submit_urb failed with result: %d",
		    __func__, retval);
}

static int usbtouch_open(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);
	int r;

	usbtouch->irq->dev = interface_to_usbdev(usbtouch->interface);

	r = usb_autopm_get_interface(usbtouch->interface) ? -EIO : 0;
	if (r < 0)
		goto out;

	if (!usbtouch->type->irq_always) {
		if (usb_submit_urb(usbtouch->irq, GFP_KERNEL)) {
			r = -EIO;
			goto out_put;
		}
	}

	usbtouch->interface->needs_remote_wakeup = 1;
out_put:
	usb_autopm_put_interface(usbtouch->interface);
out:
	return r;
}

static void usbtouch_close(struct input_dev *input)
{
	struct usbtouch_usb *usbtouch = input_get_drvdata(input);
	int r;

	if (!usbtouch->type->irq_always)
		usb_kill_urb(usbtouch->irq);
	r = usb_autopm_get_interface(usbtouch->interface);
	usbtouch->interface->needs_remote_wakeup = 0;
	if (!r)
		usb_autopm_put_interface(usbtouch->interface);
}

static int usbtouch_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);

	usb_kill_urb(usbtouch->irq);

	return 0;
}

static int usbtouch_resume(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
	struct input_dev *input = usbtouch->input;
	int result = 0;

	mutex_lock(&input->mutex);
	if (input->users || usbtouch->type->irq_always)
		result = usb_submit_urb(usbtouch->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);

	return result;
}

static int usbtouch_reset_resume(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);
	struct input_dev *input = usbtouch->input;
	int err = 0;

	/* reinit the device */
	if (usbtouch->type->init) {
		err = usbtouch->type->init(usbtouch);
		if (err) {
			dbg("%s - type->init() failed, err: %d",
			    __func__, err);
			return err;
		}
	}

	/* restart IO if needed */
	mutex_lock(&input->mutex);
	if (input->users)
		err = usb_submit_urb(usbtouch->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);

	return err;
}

static void usbtouch_free_buffers(struct usb_device *udev,
				  struct usbtouch_usb *usbtouch)
{
	usb_free_coherent(udev, usbtouch->type->rept_size,
			  usbtouch->data, usbtouch->data_dma);
	kfree(usbtouch->buffer);
}

static struct usb_endpoint_descriptor *usbtouch_get_input_endpoint(struct usb_host_interface *interface)
{
	int i;

	for (i = 0; i < interface->desc.bNumEndpoints; i++)
		if (usb_endpoint_dir_in(&interface->endpoint[i].desc))
			return &interface->endpoint[i].desc;

	return NULL;
}


static void tp_timeout_func(struct work_struct *work){
	struct usbtouch_usb *usbtouch =
		container_of(work, struct usbtouch_usb, tp_work.work);			
	int button_up = -1;

	mutex_lock(&usbtouch->tp_timeout_mutex);
	button_up = --usbtouch->tp_timer_count;
	mutex_unlock(&usbtouch->tp_timeout_mutex);
	
	if( button_up < 0){
		input_mt_sync(usbtouch->input);
		input_sync(usbtouch->input);
	}else{
		queue_delayed_work(usbtouch->tp_queue, &usbtouch->tp_work, msecs_to_jiffies(TP_TIMEOUT));
	}
	
}

static int usbtouch_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	struct usbtouch_usb *usbtouch;
	struct input_dev *input_dev;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usbtouch_device_info *type;
	int err = -ENOMEM;

	GetUserCfg();

	/* some devices are ignored */
	if (id->driver_info == -1)
		return -ENODEV;

	endpoint = usbtouch_get_input_endpoint(intf->cur_altsetting);
	if (!endpoint)
		return -ENXIO;

	usbtouch = kzalloc(sizeof(struct usbtouch_usb), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!usbtouch || !input_dev)
		goto out_free;

	type = &usbtouch_dev_info[id->driver_info];
	usbtouch->type = type;
	if (!type->process_pkt)
		type->process_pkt = usbtouch_process_pkt;

	usbtouch->data = usb_alloc_coherent(udev, type->rept_size,
					    GFP_KERNEL, &usbtouch->data_dma);
	if (!usbtouch->data)
		goto out_free;

	if (type->get_pkt_len) {
		usbtouch->buffer = kmalloc(type->rept_size, GFP_KERNEL);
		if (!usbtouch->buffer)
			goto out_free_buffers;
	}

	usbtouch->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbtouch->irq) {
		dbg("%s - usb_alloc_urb failed: usbtouch->irq", __func__);
		goto out_free_buffers;
	}

	usbtouch->interface = intf;
	usbtouch->input = input_dev;

	if (udev->manufacturer)
		strlcpy(usbtouch->name, udev->manufacturer, sizeof(usbtouch->name));

	if (udev->product) {
		if (udev->manufacturer)
			strlcat(usbtouch->name, " ", sizeof(usbtouch->name));
		strlcat(usbtouch->name, udev->product, sizeof(usbtouch->name));
	}

	if (!strlen(usbtouch->name))
		snprintf(usbtouch->name, sizeof(usbtouch->name),
			"USB Touchscreen %04x:%04x",
			 le16_to_cpu(udev->descriptor.idVendor),
			 le16_to_cpu(udev->descriptor.idProduct));

	usb_make_path(udev, usbtouch->phys, sizeof(usbtouch->phys));
	strlcat(usbtouch->phys, "/input0", sizeof(usbtouch->phys));

	input_dev->name = usbtouch->name;
	input_dev->phys = usbtouch->phys;
	usb_to_input_id(udev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;


	input_set_drvdata(input_dev, usbtouch);

	input_dev->open = usbtouch_open;
	input_dev->close = usbtouch_close;
	
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);


	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, type->min_xc, type->max_xc, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, type->min_yc, type->max_yc, 0, 0);

	mutex_init(&usbtouch->tp_timeout_mutex);
	usbtouch->tp_timer_count = -1;
	usbtouch->tp_queue= create_singlethread_workqueue("tp_queue");
	INIT_DELAYED_WORK(&usbtouch->tp_work, tp_timeout_func);
	//queue_delayed_work(tp_queue, &tp_work, msecs_to_jiffies(TP_TIMEOUT));
	
	//input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	//input_set_abs_params(input_dev, ABS_X, type->min_xc, type->max_xc, 0, 0);
	//input_set_abs_params(input_dev, ABS_Y, type->min_yc, type->max_yc, 0, 0);

	//if (type->max_press)
	//	input_set_abs_params(input_dev, ABS_PRESSURE, type->min_press,
	//	                     type->max_press, 0, 0);


	if (usb_endpoint_type(endpoint) == USB_ENDPOINT_XFER_INT)
		usb_fill_int_urb(usbtouch->irq, udev,
			 usb_rcvintpipe(udev, endpoint->bEndpointAddress),
			 usbtouch->data, type->rept_size,
			 usbtouch_irq, usbtouch, endpoint->bInterval);
	else
		usb_fill_bulk_urb(usbtouch->irq, udev,
			 usb_rcvbulkpipe(udev, endpoint->bEndpointAddress),
			 usbtouch->data, type->rept_size,
			 usbtouch_irq, usbtouch);

	usbtouch->irq->dev = udev;
	usbtouch->irq->transfer_dma = usbtouch->data_dma;
	usbtouch->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* device specific allocations */
	if (type->alloc) {
		err = type->alloc(usbtouch);
		if (err) {
			dbg("%s - type->alloc() failed, err: %d", __func__, err);
			goto out_free_urb;
		}
	}

	/* device specific initialisation*/
	if (type->init) {
		err = type->init(usbtouch);
		if (err) {
			dbg("%s - type->init() failed, err: %d", __func__, err);
			goto out_do_exit;
		}
	}

	err = input_register_device(usbtouch->input);
	if (err) {
		dbg("%s - input_register_device failed, err: %d", __func__, err);
		goto out_do_exit;
	}

	usb_set_intfdata(intf, usbtouch);

	if (usbtouch->type->irq_always) {
		/* this can't fail */
		usb_autopm_get_interface(intf);
		err = usb_submit_urb(usbtouch->irq, GFP_KERNEL);
		if (err) {
			usb_autopm_put_interface(intf);
			err("%s - usb_submit_urb failed with result: %d",
			    __func__, err);
			goto out_unregister_input;
		}
	}

	return 0;

out_unregister_input:
	input_unregister_device(input_dev);
	input_dev = NULL;
out_do_exit:
	if (type->exit)
		type->exit(usbtouch);
out_free_urb:
	usb_free_urb(usbtouch->irq);
out_free_buffers:
	usbtouch_free_buffers(udev, usbtouch);
out_free:
	input_free_device(input_dev);
	kfree(usbtouch);
	return err;
}

static void usbtouch_disconnect(struct usb_interface *intf)
{
	struct usbtouch_usb *usbtouch = usb_get_intfdata(intf);

	dbg("%s - called", __func__);

	if (!usbtouch)
		return;

	dbg("%s - usbtouch is initialized, cleaning up", __func__);
	mutex_destroy(&usbtouch->tp_timeout_mutex);	
	usb_set_intfdata(intf, NULL);
	/* this will stop IO via close */
	input_unregister_device(usbtouch->input);
	usb_free_urb(usbtouch->irq);
	if (usbtouch->type->exit)
		usbtouch->type->exit(usbtouch);
	usbtouch_free_buffers(interface_to_usbdev(intf), usbtouch);
	kfree(usbtouch);
}


static void GetUserCfg(void)
{
struct file *fd;
char buffer[256];
loff_t pos;

mm_segment_t old_fs = get_fs();
set_fs(KERNEL_DS);

memset((char *)&buffer[0],0,256);

fd= filp_open("/etc/metouch.cfg",O_RDONLY,0);

if (IS_ERR(fd)) 
	{
	printk("Unable to open metouch config information.\n");
	goto exithere;
      	}
else
	{
        printk("open metouch config file ok.\n");

	pos=0;
       	vfs_read(fd,buffer,256,&pos);

	if (pos < 0)
    	     {
    	     printk("reach the end of file\n");
	     }
        else
             { 
             AnlysCmd(buffer);
	     }
        filp_close(fd,NULL);
	}
exithere:;
set_fs(old_fs);
}

static void AnlysCmd(char *cmd)
{
//static int swapx=0;
//static int swapy=0;
//static int v_shift=0;
//static int h_shift=0;
char *pstr=NULL;
char upx[10];
char upy[10];
char vsh[10];
char hsh[10];
int  i=0;

//clear all invisible char
DeleteAllSpace(cmd);

//find UPSIDEX
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"UPSIDEX=");
if (pstr!=NULL)
{
strncpy(upx,(char *)(pstr+strlen("UPSIDEX=")),4);

i=0;
while (i<4)
   {
   if (upx[i]<0x20)
       {
       upx[i]=0;
       swapx = myatoi(upx);
       break;
       }
   i++;
   }
}

//find UPSIDEY
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"UPSIDEY=");
if (pstr!=NULL)
{
strncpy(upy,(char *)(pstr+strlen("UPSIDEY=")),4);

i=0;
while (i<4)
   {
   if (upy[i]<0x20)
       {
       upy[i]=0;
       swapy = myatoi(upy);
       break;
       }
   i++;
   }
}

//find V_SHIFT
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"V_SHIFT=");
if (pstr!=NULL)
{
strncpy(vsh,(char *)(pstr+strlen("V_SHIFT=")),4);
i=0;
while (i<4)
   {
   if (vsh[i]<0x20)
       {
       vsh[i]=0;
       v_shift = myatoi(vsh);
       break;
       }
   i++;
   }
}

//find H_SHIFT
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"H_SHIFT=");
if (pstr!=NULL)
{
strncpy(hsh,(char *)(pstr+strlen("H_SHIFT=")),4);
i=0;
while (i<4)
   {
   if (hsh[i]<0x20)
       {
       hsh[i]=0;
       h_shift = myatoi(hsh);
       break;
       }
   i++;
   }
}
//v_flag
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"V_FLAG=");
if (pstr!=NULL)
{
strncpy(hsh,(char *)(pstr+strlen("V_FLAG=")),4);

i=0;
while (i<4)
   {
   if (hsh[i]<0x20)
       {
       hsh[i]=0;
       v_flag = myatoi(hsh);
       break;
       }
   i++;
   }
}

//H_flag
pstr=NULL;
pstr=strstr((const char*)cmd,(const char*)"H_FLAG=");
if (pstr!=NULL)
{
strncpy(hsh,(char *)(pstr+strlen("H_FLAG=")),4);
i=0;
while (i<4)
   {
   if (hsh[i]<0x20)
       {
       hsh[i]=0;
       h_flag = myatoi(hsh);
       break;
       }
   i++;
   }
}

printk("%d\n",v_flag);
printk("%d\n",h_flag);
printk("%d\n",swapx);
printk("%d\n",swapy);
printk("%d\n",v_shift);
printk("%d\n",h_shift);
}

static void DeleteAllSpace(char *cmd)
{
char tmp[256];
int i=0;
int j=0;
memset((char *)&tmp,0,256);
for (i=0;i<250;i++)
    {
    //printk("%02x ",cmd[i]&0xff);

    if ((cmd[i]!=0x09) && (cmd[i]!=0x20))
	{    
	tmp[j]=cmd[i];
        j++;
	}  
    }
//printk("%s\n",cmd);

//printk("\n");
tmp[j]=0;
//for (i=0;i<250;i++)
//    {
    //printk("%02x ",tmp[i]&0xff);
//    }
//printk("%s\n",tmp);

strncpy(cmd,tmp,250);
}

static int myatoi(char *str)
{
int i;
int num=0;
int len = strlen(str);


for (i=0;i<len;i++)
    {
   // printk("%02x ",str[i]);
    if (str[i]<0x30)
        break;
    else 
        {
       // printk("s=%x d=%x s=%x ",str[i],str[i]-0x30,mypow(len-i-1));
        num = num + mypow(i)*(str[len-i-1]-0x30);
       // printk("num = %d\n",num);
	}
    }
//printk("\n");
return num; 
}

int mypow(int t)
{
if (t==0) return 1;
if (t==1) return 10;
if (t==2) return 100;
if (t==3) return 1000;
if (t==4) return 10000;
else
return 0;
}

static struct usb_driver usbtouch_driver = {
	.name		= "metusb",
	.probe		= usbtouch_probe,
	.disconnect	= usbtouch_disconnect,
	.suspend	= usbtouch_suspend,
	.resume		= usbtouch_resume,
	.reset_resume	= usbtouch_reset_resume,
	.id_table	= metusb_table,
	.supports_autosuspend = 1,
};


static int __init usbtouch_init(void)
{
	return usb_register(&usbtouch_driver);
}

static void __exit usbtouch_cleanup(void)
{
	usb_deregister(&usbtouch_driver);
}

module_init(usbtouch_init);
module_exit(usbtouch_cleanup);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_ALIAS("metusb");
