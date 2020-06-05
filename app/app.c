/* FPGA FND Test Application
File : fpga_test_fnd.c
Auth : largest@huins.com */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEVICE "/dev/stopwatch"

int main(int argc, char **argv)
{
	int dev = open(DEVICE, O_WRONLY);
    char data = 0;
    
    if (dev < 0) return -1;
    write(dev, &data, sizeof(data));
	close(dev);

	return 0;
}
