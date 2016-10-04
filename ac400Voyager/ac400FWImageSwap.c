#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>


void getOperationalImage(int moduleId) {
	int val = 0;

	val = mdio_read(moduleId, 0xB051);
	if ((val & 0x1000) >> 12) {
		printf("Running Image is B\n");
	} else {
		printf("Running Image is A\n");
	}
}

void getCommittedImage(int moduleId) {
	int val = 0;

	val = mdio_read(moduleId, 0xB051);
	if ((val & 0x0080) >> 7) {
		printf("Committed Image is B\n");
	} else {
		printf("Committed Image is A\n");
	}
}

void getImageStat(int moduleId) {
	int val = 0;
	int val1 = 0;
	int val2 = 0;

	val = mdio_read(moduleId, 0xB051);
	val1 = (val & 0x0C00) >> 10;
	switch (val1) {
		case 0:
			printf("A: No Image\n");
			break;
		case 1:
			printf("A: Valid Image Present\n");
			break;
		case 2:
			printf("A: Image Bad\n");
			break;
		case 3:
			printf("A: Reserved\n");
			break;
	}
	val2 = (val & 0x0300) >> 8;
	switch (val2) {
		case 0:
			printf("B: No Image\n");
			break;
		case 1:
			printf("B: Valid Image Present\n");
			break;
		case 2:
			printf("B: Image Bad\n");
			break;
		case 3:
			printf("B: Reserved\n");
			break;
	}
	return;
}

int isTrafficAffecting(int moduleId) {
	int val = 0;

	val = mdio_read(moduleId, 0xB051);
	return ((val & 0x2000) >> 13);
}

void swap_committed_image(int moduleId) {
	int val = 0;

	val = mdio_read(moduleId, 0xB051);
	if ((val & 0x0080) >> 7) {
		printf("Swapping running Image from B to A\n");
		val = mdio_read(moduleId, 0xB04D);
		printf("Val: 0X%X\n", val);
		val = val & 0x0FFF;
		val = val | 0x8000;
		mdio_write(moduleId, 0xB04D, val);
	} else {
		printf("Swapping running Image from A to B\n");
		val = mdio_read(moduleId, 0xB04D);
		printf("Val: 0X%X\n", val);
		val = val & 0x0FFF;
		val = val | 0x9000;
		mdio_write(moduleId, 0xB04D, val);
	}
	
}


void swap_running_image(int moduleId) {
	int val = 0;

	val = mdio_read(moduleId, 0xB051);
	if ((val & 0x1000) >> 12) {
		printf("Swapping running Image from B to A\n");
		val = mdio_read(moduleId, 0xB04D);
		printf("Val: 0X%X\n", val);
		val = val & 0x0FFF;
		val = val | 0x3000;
		mdio_write(moduleId, 0xB04D, val);
	} else {
		printf("Swapping running Image from A to B\n");
		val = mdio_read(moduleId, 0xB04D);
		printf("Val: 0X%X\n", val);
		val = val & 0x0FFF;
		val = val | 0x4000;
		mdio_write(moduleId, 0xB04D, val);
	}
	
}

void getFirmwareVersion(int moduleId) {
        uint16_t firmA_X_ver, firmA_Y_ver;
        uint16_t firmB_X_ver, firmB_Y_ver;

        firmA_X_ver = mdio_read(moduleId, 0x806C);
        firmA_Y_ver = mdio_read(moduleId, 0x806D);
        firmB_X_ver = mdio_read(moduleId, 0x807B);
        firmB_Y_ver = mdio_read(moduleId, 0x807C);

        printf("Firmware A Version %d.%d\n", firmA_X_ver, firmA_Y_ver);
        printf("Firmware B Version %d.%d\n", firmB_X_ver, firmB_Y_ver);
	return;
}


int main(int argc, char **argv) {
	int moduleId = 0;

	moduleId = atoi(argv[1]);
	printf("Module Id : %d\n", moduleId);

	getOperationalImage(moduleId);
	getCommittedImage(moduleId);
	getImageStat(moduleId);
	getFirmwareVersion(moduleId);
	if (isTrafficAffecting(moduleId)) {
		printf("Image Swap will be traffic affecting\n");
	} else {
		printf("Image Swap will not be traffic affecting\n");
	}
	swap_running_image(moduleId);
	sleep(60);
	swap_committed_image(moduleId);
	sleep(60);
	getOperationalImage(moduleId);
	return 0;
}
