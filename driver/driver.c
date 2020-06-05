#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/ioctl.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <mach/gpio.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <asm/irq.h>
#include <mach/gpio.h>
#include <linux/platform_device.h>
#include <asm/gpio.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/cdev.h>

static int inter_major=242, inter_minor=0;
static int result;
static dev_t inter_dev;
static struct cdev inter_cdev;
static int inter_open(struct inode *, struct file *);
static int inter_release(struct inode *, struct file *);
static int inter_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler3(int irq, void* dev_id, struct pt_regs* reg);
irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg);

static inter_usage=0;
int interruptCount=0;

wait_queue_head_t wq_write;
DECLARE_WAIT_QUEUE_HEAD(wq_write);

#define IOM_FND_MAJOR 261        // ioboard fpga device major number
#define IOM_FND_NAME "fpga_fnd"        // ioboard fpga device name

#define IOM_FND_ADDRESS 0x08000004 // pysical address

static int fpga_fnd_port_usage = 0;
static unsigned char *iom_fpga_fnd_addr;
static struct cdev inter_cdev;
static dev_t inter_dev;
static int kernel_timer_usage = 0;

static struct struct_mydata {
    struct timer_list timer;
    int count;
};

struct struct_mydata mydata;
struct struct_mydata mydata2;

static int blink_cnt = 1;
// define functions...
int iom_fpga_driver_open(struct inode *minode, struct file *mfile);
int iom_fpga_driver_release(struct inode *minode, struct file *mfile);
int iom_fpga_driver_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);
irqreturn_t home_handler( int irq, void * dev_id, struct pt_regs *regs );
irqreturn_t back_handler( int irq, void * dev_id, struct pt_regs *regs );
irqreturn_t vol_up_handler( int irq, void * dev_id, struct pt_regs *regs );
irqreturn_t vol_down_push_handler( int irq, void * dev_id, struct pt_regs *regs );
irqreturn_t vol_down_pull_handler( int irq, void * dev_id, struct pt_regs *regs );
int fnd_write(unsigned int value[4]);


static struct file_operations inter_fops =
{
	.open = inter_open,
	.write = inter_write,
	.release = inter_release,
};


static int exit_signal = 0;
static unsigned int fnd_value[4];
static int timer_init = 0;
static int first_push = 1;

void up_cnt(){
    fnd_value[0]++;
    if(fnd_value[0] == 10){
        fnd_value[0] = 0;
        fnd_value[1]++;
    }
    
    if(fnd_value[1] == 6){
        fnd_value[1] = 0;
        fnd_value[2]++;
    }
    
    if(fnd_value[2] == 10){
        fnd_value[2] = 0;
        fnd_value[3]++;
    }
    
    if(fnd_value[3] == 6){
        fnd_value[3] = 0;
    }
}

int fnd_write(unsigned int _value[4]){
    unsigned int value[4];
    int i = 0;
    //print it reverse order
    //because
    //arr -> [0] [1] [2] [3]
    //fnd -> [3] [2] [1] [0]
    for(i = 0; i < 4; i++){
        value[i] = _value[3-i];
    }
    unsigned short int value_short = 0;
    
    value_short = value[0] << 12 | value[1] << 8 |value[2] << 4 |value[3];
    outw(value_short,(unsigned int)iom_fpga_fnd_addr);
    
    return 0;
}

static int blinking_cnt = 0;

static void kernel_timer_blink(unsigned long timeout) {
    printk("start blink\n");
    struct struct_mydata *p_data = (struct struct_mydata*)timeout;
    unsigned char string[33];
    printk("kernel_timer_blink %d\n", p_data->count);
    
    if(exit_signal){
        return;
    }
    if(blinking_cnt >= 10){
        blinking_cnt = 0;
        up_cnt();
        fnd_write(fnd_value);
    }
    mydata.timer.expires = get_jiffies_64() + HZ/10;
    mydata.timer.data = (unsigned long)&mydata;
    mydata.timer.function = kernel_timer_blink;
    
    add_timer(&mydata.timer);
    blinking_cnt++;
}


irqreturn_t inter_handler1(int irq, void* dev_id, struct pt_regs* reg) {
	printk(KERN_ALERT "interrupt1!!! = %x\n", gpio_get_value(IMX_GPIO_NR(1, 11)));

	printk("home handler\n");
    if(!timer_init){
        printk("hi!\n");
        exit_signal = 0;
        timer_init = 1;
        mydata.timer.expires = jiffies + HZ/10;
        mydata.timer.data = (unsigned long)&mydata;
        mydata.timer.function = kernel_timer_blink;
        add_timer(&mydata.timer);
    }
	return IRQ_HANDLED;
}

irqreturn_t inter_handler2(int irq, void* dev_id, struct pt_regs* reg) {
        printk(KERN_ALERT "interrupt2!!! = %x\n", gpio_get_value(IMX_GPIO_NR(1, 12)));
    exit_signal = 1;
    timer_init = 0;
    return IRQ_HANDLED;
}

irqreturn_t inter_handler3(int irq, void* dev_id,struct pt_regs* reg) {
        printk(KERN_ALERT "interrupt3!!! = %x\n", gpio_get_value(IMX_GPIO_NR(2, 15)));
    exit_signal = 1;
    timer_init = 0;
    int i = 0;
    for(i = 0; i < 4; i++) fnd_value[i] = 0;
    fnd_write(fnd_value);
    return IRQ_HANDLED;
}


static u64 prev_hz = 1;
static int end_of_program_ = 0;

irqreturn_t inter_handler4(int irq, void* dev_id, struct pt_regs* reg) {
    printk(KERN_ALERT "interrupt4!!! = %x\n", gpio_get_value(IMX_GPIO_NR(5, 14)));
    if (first_push){
        printk("first push");
        first_push = 0;
        prev_hz = get_jiffies_64();
    }
    else{
        printk("n'th push");
        u64 cur_hz = get_jiffies_64();
        if (cur_hz - prev_hz >= 2.9*HZ){
            end_of_program_ = 1;
            exit_signal = 1;
        }
        prev_hz = cur_hz;
        first_push = 1;
    }
    
    int i;
    if(end_of_program_){
        for(i = 0; i < 4; i++) fnd_value[i] = 0;
        fnd_write(fnd_value);
        first_push = 1;
        end_of_program_ = 0;
        del_timer_sync(&mydata.timer);
        wake_up_interruptible(&wq_write);
    }
    return IRQ_HANDLED;
}

static int inter_open(struct inode *minode, struct file *mfile){
	int ret;
	int irq;

    init_timer(&(mydata.timer));
    init_timer(&(mydata2.timer));
    
	printk(KERN_ALERT "Open Module\n");

	// int1
	gpio_direction_input(IMX_GPIO_NR(1,11));
	irq = gpio_to_irq(IMX_GPIO_NR(1,11));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler1, IRQF_TRIGGER_FALLING, "home", 0);

	// int2
	gpio_direction_input(IMX_GPIO_NR(1,12));
	irq = gpio_to_irq(IMX_GPIO_NR(1,12));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler2, IRQF_TRIGGER_FALLING, "back", 0);

	// int3
	gpio_direction_input(IMX_GPIO_NR(2,15));
	irq = gpio_to_irq(IMX_GPIO_NR(2,15));
	printk(KERN_ALERT "IRQ Number : %d\n",irq);
	ret=request_irq(irq, inter_handler3, IRQF_TRIGGER_FALLING, "volup", 0);

    gpio_direction_input(IMX_GPIO_NR(5,14));
    irq = gpio_to_irq(IMX_GPIO_NR(5,14));
    printk(KERN_ALERT "IRQ Number : %d\n",irq);
    request_irq (irq, inter_handler4, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "vol_down", 0);
    
    fpga_fnd_port_usage = 1;
    kernel_timer_usage = 1;
    
	return 0;
}

static int inter_release(struct inode *minode, struct file *mfile){
    del_timer_sync(&mydata2.timer);
    
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 11)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(1, 12)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(2, 15)), NULL);
	free_irq(gpio_to_irq(IMX_GPIO_NR(5, 14)), NULL);
	
    fpga_fnd_port_usage = 0;
    kernel_timer_usage = 0;
    
    exit_signal = 0;
    int i = 0;
    for(i = 0; i < 4; i++) fnd_value[i] = 0;
    fnd_write(fnd_value);
    timer_init = 0;
    first_push = 1;
    prev_hz = 1;
    end_of_program_ = 0;
    blinking_cnt = 0;
    
    printk(KERN_ALERT "Release Module\n");
	return 0;
}

static int inter_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos ){
    printk("sleep on\n");
    interruptible_sleep_on(&wq_write);
	printk("write\n");
	return 0;
}

static int inter_register_cdev(void)
{
	int error;
	if(inter_major) {
		inter_dev = MKDEV(inter_major, inter_minor);
		error = register_chrdev_region(inter_dev,1,"stopwatch");
	}else{
		error = alloc_chrdev_region(&inter_dev,inter_minor,1,"stopwatch");
		inter_major = MAJOR(inter_dev);
	}
	if(error<0) {
		printk(KERN_WARNING "inter: can't get major %d\n", inter_major);
		return result;
	}
	printk(KERN_ALERT "major number = %d\n", inter_major);
	cdev_init(&inter_cdev, &inter_fops);
	inter_cdev.owner = THIS_MODULE;
	inter_cdev.ops = &inter_fops;
	error = cdev_add(&inter_cdev, inter_dev, 1);
	if(error)
	{
		printk(KERN_NOTICE "inter Register Error %d\n", error);
	}
	return 0;
}

static int __init inter_init(void) {
	int result;
	if((result = inter_register_cdev()) < 0 )
		return result;
	printk(KERN_ALERT "Init Module Success \n");
	printk(KERN_ALERT "Device : /dev/inter, Major Num : 242 \n");
    
    end_of_program_ = 0;
    iom_fpga_fnd_addr = ioremap(IOM_FND_ADDRESS, 0x4);
	return 0;
}

static void __exit inter_exit(void) {
    iounmap(iom_fpga_fnd_addr);
    cdev_del(&inter_cdev);
	unregister_chrdev_region(inter_dev, 1);
	printk(KERN_ALERT "Remove Module Success \n");
}

module_init(inter_init);
module_exit(inter_exit);
MODULE_LICENSE("GPL");
