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

wait_queue_head_t wq_write;
DECLARE_WAIT_QUEUE_HEAD (wq_write);

#define IOM_FND_MAJOR 261        // ioboard fpga device major number
#define IOM_FND_NAME "fpga_fnd"        // ioboard fpga device name

#define IOM_FND_ADDRESS 0x08000004 // pysical address

#define SET_OPTION 0x9999
#define COMMAND 0x10000

#define MAX_BUFF 32
#define LINE_BUFF 16

//Global variable
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

// define file_operations structure
struct file_operations inter_fops =
{
owner:        THIS_MODULE,
open:        iom_fpga_driver_open,
write:        iom_fpga_driver_write,
release:    iom_fpga_driver_release,
};


//the function which opens the drivers;
int iom_fpga_driver_open(struct inode *minode, struct file *mfile)
{
    printk("opend\n");
    if(fpga_fnd_port_usage != 0) return -EBUSY;
    if(kernel_timer_usage != 0) return -EBUSY;
    
    int irq;
    gpio_direction_input(IMX_GPIO_NR(1,11));
    irq = gpio_to_irq(IMX_GPIO_NR(1,11));
    request_irq (irq, home_handler, IRQF_TRIGGER_FALLING, "home", NULL);
    
    gpio_direction_input(IMX_GPIO_NR(1,12));
    irq = gpio_to_irq(IMX_GPIO_NR(1,12));
    request_irq (irq, back_handler, IRQF_TRIGGER_FALLING, "back", NULL);
    
    gpio_direction_input(IMX_GPIO_NR(2,15));
    irq = gpio_to_irq(IMX_GPIO_NR(2,15));
    request_irq (irq, vol_up_handler, IRQF_TRIGGER_FALLING, "vol_up", NULL);
    
    gpio_direction_input(IMX_GPIO_NR(5,14));
    irq = gpio_to_irq(IMX_GPIO_NR(5,14));
    request_irq (irq, vol_down_push_handler, IRQF_TRIGGER_FALLING, "vol_down_push", NULL);
    
    gpio_direction_input(IMX_GPIO_NR(5,14));
    irq = gpio_to_irq(IMX_GPIO_NR(5,14));
    request_irq (irq, vol_down_pull_handler, IRQF_TRIGGER_RISING, "vol_down_pull", NULL);
    
    fpga_fnd_port_usage = 1;
    kernel_timer_usage = 1;
    
    return 0;
}

int iom_fpga_driver_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos){
    printk("before sleep\n");
    interruptible_sleep_on(&wq_write);
    printk("after sleep\n");
    return 0;
}

//the function which releases the drivers;
int iom_fpga_driver_release(struct inode *minode, struct file *mfile)
{
    fpga_fnd_port_usage = 0;
    kernel_timer_usage = 0;
    
    free_irq(gpio_to_irq(IMX_GPIO_NR(1, 11)), NULL);
    free_irq(gpio_to_irq(IMX_GPIO_NR(1, 12)), NULL);
    free_irq(gpio_to_irq(IMX_GPIO_NR(2, 15)), NULL);
    free_irq(gpio_to_irq(IMX_GPIO_NR(5, 14)), NULL);
    
    return 0;
}

static int exit_signal = 0, exit_signal_down = 0;
static unsigned int fnd_value[4];

void up_cnt(){
    fnd_value[0]++;
    if(fnd_value[0] == 10){
        fnd_value[1]++;
    }
    
    if(fnd_value[1] == 6){
        fnd_value[2]++;
    }
    
    if(fnd_value[2] == 10){
        fnd_value[3]++;
    }
    
    if(fnd_value[3] == 6){
        fnd_value[3] = 0;
    }
}

static void kernel_timer_blink(unsigned long timeout) {
    printk("start blink\n");
    struct struct_mydata *p_data = (struct struct_mydata*)timeout;
    unsigned char string[33];
    printk("kernel_timer_blink %d\n", p_data->count);
    
    if(exit_signal) return;
    up_cnt();
    fnd_write(fnd_value);
    
    mydata.timer.expires = get_jiffies_64() + 100;
    mydata.timer.data = (unsigned long)&mydata;
    mydata.timer.function = kernel_timer_blink;
    
    add_timer(&mydata.timer);
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

static int timer_init = 0;

irqreturn_t home_handler( int irq, void * dev_id, struct pt_regs *regs ){
    printk("home handler\n");
    if(!timer_init){
        timer_init = 1;
        mydata.timer.expires = jiffies + 100;
        mydata.timer.data = (unsigned long)&mydata;
        mydata.timer.function = kernel_timer_blink;
        add_timer(&mydata.timer);
    }
    return IRQ_HANDLED;
}

irqreturn_t back_handler( int irq, void * dev_id, struct pt_regs *regs ){
    exit_signal = 1;
    timer_init = 0;
    return IRQ_HANDLED;
}

irqreturn_t vol_up_handler( int irq, void * dev_id, struct pt_regs *regs ){
    exit_signal = 1;
    timer_init = 0;
    int i = 0;
    for(i = 0; i < 4; i++) fnd_value[i] = 0;
    return IRQ_HANDLED;
}

static int end_three_sencond_flag = 0, end_three_sencond_cnt = 0, end_of_program = 0;


void end_three_sencond(){
    if(end_three_sencond_cnt >= 30){
        end_of_program = 1;
    }
    
    if(exit_signal_down){
        exit_signal_down = 0;
        end_three_sencond_cnt = 0;
        return ;
    }
    end_three_sencond_cnt++;
}

irqreturn_t vol_down_push_handler( int irq, void * dev_id, struct pt_regs *regs ){
    timer_init = 1;
    mydata.timer.expires = jiffies + 10;
    mydata.timer.data = (unsigned long)&mydata;
    mydata.timer.function = end_three_sencond;
    add_timer(&mydata.timer);
    return IRQ_HANDLED;
}

irqreturn_t vol_down_pull_handler( int irq, void * dev_id, struct pt_regs *regs ){
    int i = 0;
    exit_signal_down = 1;
    if(end_of_program){
        for(i = 0; i < 4; i++) fnd_value[i] = 0;
        fnd_write(fnd_value);
        wake_up_interruptible(&wq_write);
    }
    
    return IRQ_HANDLED;
}

static int inter_register_cdev(void)
{
    int error;
    inter_dev = MKDEV(242, 0);
    error = register_chrdev_region(inter_dev,1,"inter");
    if(error<0) {
        printk(KERN_WARNING "inter: can't get major %d\n", 242);
        return result;
    }
    printk(KERN_ALERT "major number = %d\n", 242);
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

int __init iom_fpga_driver_init(void)
{
    int result;
    if((result = inter_register_cdev()) < 0 )
        return result;
    printk(KERN_ALERT "Init Module Success \n");
    printk(KERN_ALERT "Device : /dev/inter, Major Num : 242 \n");
    
    //get io port address space
    iom_fpga_fnd_addr = ioremap(IOM_FND_ADDRESS, 0x4);
    end_of_program = 0;
    if(result < 0) {
        printk(KERN_WARNING"Can't get any major of FND\n");
        return result;
    }
    
    printk("kernel_timer_init\n");
    
    //timer init
    init_timer(&(mydata.timer));
    
    printk("init module\n");
    
    printk("init module, %s major number : %d\n", "dev drivers", 242);
    return 0;
}

void __exit iom_fpga_driver_exit(void)
{
    iounmap(iom_fpga_fnd_addr);
    
    printk("kernel_timer_exit\n");
    del_timer_sync(&mydata.timer);
    cdev_del(&inter_cdev);
    unregister_chrdev_region(inter_dev, 1);
}

module_init(iom_fpga_driver_init);
module_exit(iom_fpga_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huins");
