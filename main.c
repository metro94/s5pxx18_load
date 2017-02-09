#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libusb.h>
#include "s5p_usb.h"
#include "s5p_boot.h"

#define BOOTLOADER_MAX_SIZE	100 * 1024 * 1024

typedef unsigned char uint8;
typedef unsigned int  uint32;

unsigned char mem[BOOTLOADER_MAX_SIZE];

void write32LE(uint8 *addr, uint32 data)
{
	*(addr)     = (uint8)(data & 0xff);
	*(addr + 1) = (uint8)((data >> 8) & 0xff);
	*(addr + 2) = (uint8)((data >> 16) & 0xff);
	*(addr + 3) = (uint8)((data >> 24) & 0xff);
}

void initBootloaderHeader(uint8 *buf, unsigned int size, unsigned int load_addr, unsigned int launch_addr)
{
	// Clear buffer
	memset(buf, 0x00, 512);

	write32LE(buf + 0x40, 0x00000000);
	write32LE(buf + 0x44, size);
	write32LE(buf + 0x48, load_addr);
	write32LE(buf + 0x4c, launch_addr);

	write32LE(buf + 0x1fc, 0x4849534e);
}

int readBin(uint8 *buf, const char *filepath)
{
	FILE *fin;

	int size;

	fin = fopen(filepath, "rb");
	if (fin == NULL)
		return -1;
	size = (int)fread(buf, 1, BOOTLOADER_MAX_SIZE, fin);
	fclose(fin);
	if (size <= 0)
		return size;

	// Size must be aligned by 16bytes
	if (size % 16 != 0)
		size = ((size / 16) + 1) * 16;

	return size;
}

int usbBoot(uint8 *buf, int size)
{
	libusb_context        *ctx;
	libusb_device        **list;
	libusb_device_handle  *dev_handle;

	int ret;
	int transfered;

	ret = libusb_init(&ctx);
	if (ret < 0)
		return ret;

	libusb_set_debug(ctx, LIBUSB_LOG_LEVEL_INFO);

	ret = (int)libusb_get_device_list(ctx, &list);
	if (ret < 0)
		return ret;

	dev_handle = libusb_open_device_with_vid_pid(ctx, S5P6818_VID, S5P6818_PID);
	if (dev_handle == NULL)
		return -1;

	libusb_free_device_list(list, 1);

	ret = libusb_claim_interface(dev_handle, S5P6818_INTERFACE);
	if (ret < 0)
		return ret;

	printf("Start transfer, size = %d\n", size);
	ret = libusb_bulk_transfer(dev_handle, S5P6818_EP_OUT, buf, size, &transfered, 0);
	if (ret < 0)
		return ret;
	printf("Finish transfer, actual size = %d\n", transfered);
	if (size == transfered)
		printf("Second Boot Successed\n");
	else
		printf("Second Boot Might Fail\n");

	libusb_release_interface(dev_handle, 0);

	libusb_close(dev_handle);

	libusb_exit(ctx);
	
	return 0;
}

int main(int argc, const char *argv[])
{
	int ret;
	unsigned int size;
	unsigned int load_addr;
	unsigned int launch_addr;

	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Wrong Parameters\n");
		printf("Usage: load bootloader load_addr [launch_addr]\n");
		printf("If launch_addr is not passed or launch_addr equals to 0, bootloader will be loaded without executing it.\n");
		return -1;
	}
	
	if (sscanf(argv[2], "%x", &load_addr) != 1) {
		fprintf(stderr, "Invalid Parameters \"load_addr\"\n");
		printf("Usage: load bootloader load_addr [launch_addr]\n");
		printf("If launch_addr is not passed or launch_addr equals to 0, bootloader will be loaded without executing it.\n");
	}
	
	if (argc == 3) {
		launch_addr = 0;
	} else if (sscanf(argv[3], "%x", &launch_addr) != 1) {
		fprintf(stderr, "Invalid Parameters \"launch_addr\"\n");
		printf("Usage: load bootloader load_addr [launch_addr]\n");
		printf("If launch_addr is not passed or launch_addr equals to 0, bootloader will be loaded without executing it.\n");
	}

	size = readBin(mem + 0x200, argv[1]);
	if (size <= 0) {
		fprintf(stderr, "Failed when loading bootloader\n");
		return -1;
	}
	initBootloaderHeader(mem, size, load_addr, launch_addr);
	size += 0x200;

	ret = usbBoot(mem, size);
	if (ret < 0) {
		fprintf(stderr, "Failed when starting bootloader\n");
		return -1;
	}

	return 0;
}
