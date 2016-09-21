#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>


int isFileExist(char * fileName) {
	struct stat buf;
	int retVal = 0;

	retVal = stat(fileName, &buf);
	if (retVal == 0) {
		return 1;
	}
	return 0;
}


int main(int argc, char **argv) {
	char fileName[256] = {0};
	FILE *fptr = NULL;
	int fileLen = 0;
	uint32_t polynomial = 0;
	uint32_t crcTable[256] = {0};
	int i, j;
	uint32_t crcInit = 0, crcXor = 0, crcAccum = 0;
	uint32_t runningCrc = 0;
	uint16_t addr = 0, runningCrcLow, runningCrcHigh;
	uint8_t byte1, byte2, pointer;
	uint16_t data = 0;
	int moduleId = 0;
	uint16_t val;

	if (argc != 3) {
		fprintf(stderr, "Invalid Argument\n");
		return -1;
	}

	strncpy(fileName, argv[1], 256);
	printf("FileName: %s\n", fileName);
	if (!isFileExist(fileName)) {
		fprintf(stderr, "File %s does not exist.\n", fileName);
		return -1;
	}

	moduleId = atoi(argv[2]);
	printf("ModuleId: %d\n", moduleId);

	// Open kit file and get the file length
	fptr = fopen(fileName, "rb");
	if (fptr ==  NULL) {
		fprintf(stderr, "Error opening the file(%s): %s\n", fileName, strerror(errno));
	}

	fseek(fptr, 0, SEEK_END);
	fileLen = ftell(fptr);

	printf("File length: %d\n", fileLen);
	// Create the 256 entry CRC table based on the generator polynomial for AC100/AC100M
	// polynomial = 0xEDB88320, endian = big, CRC Init = 0, CRC XOR = 0xFFFFFFFF
	polynomial = 0xEDB88320;
	for (i = 0; i < 256; i++) {
		crcAccum = i << 24;
		for (j = 0; j < 8; j++) {
			if (crcAccum & 0x80000000) {
				crcAccum = ((crcAccum << 1) & 0xFFFFFFFF) ^ polynomial;
			} else {
				crcAccum = (crcAccum << 1) & 0xFFFFFFFF;
			}
		}
		crcTable[i] = crcAccum;
	}

	printf("CRC Table:");
	for (i = 0; i < 256; i++) {
		printf("0x%x\t", crcTable[i]);
	}
	printf("\n");

	// TODO:
	// Set the download block size - 256 bytes for AC100/AC100M, or 0x0080
	// mdioRdy(10);
	mdio_write(moduleId, 0xBC00, 0x0080);

	// TODO:
	// Request a firmware download via read/modify/write of address 0xB04D
	val = mdio_read(moduleId, 0xB04D) | 0x1000;
	// mdioRdy(10);
	mdio_write(moduleId, 0xB04D, val);


	// Start reading kit file from the beginning.
	fseek(fptr, 0, SEEK_SET);

	// Set the CRC initialization and XOR values.
	crcInit = 0x00000000;
	crcXor = 0xFFFFFFFF;

	// Read 256 bytes at a time and write to MDIO 16 bits at a time &
	// calculate the CRC for 256 byte blocks
	for (i = 0; i < (fileLen/256); i++) {
		runningCrc = crcInit;
		// Write the 256 bytes starting from MDIO address 0xBC01
		addr = 0xBC01;
		// Read 2 bytes at a time and process
		for (j = 0; j < 128; j++) {
			byte1 = fgetc(fptr);
			byte2 = fgetc(fptr);

			data = (byte1 << 8) | byte2;
			// TODO:
			mdio_write(moduleId, addr, data);

			addr++;

			// Process the first byte for the block CRC
			pointer = ((runningCrc >> 24) & 0xFF) ^ byte1;
			runningCrc = (((runningCrc << 8) & 0xFFFFFFFF) ^ crcTable[pointer]);

			// Process the second byte for the block CRC
			pointer = ((runningCrc >> 24) & 0xFF) ^ byte2;
			runningCrc = (((runningCrc << 8) & 0xFFFFFFFF) ^ crcTable[pointer]);
		}

		// Process the running CRC and write the value for the block to MDIO
		// addresses 0xBC81 and 0xBC82
		runningCrc = runningCrc ^ crcXor;
		runningCrcHigh = (runningCrc >> 16) & 0xFFFF;
		runningCrcLow = runningCrc & 0xFFFF;

		// TODO:
		// mdioRdy(10);
		mdio_write(moduleId, 0xBC81, runningCrcHigh);
		// mdioRdy(10);
		mdio_write(moduleId, 0xBC82, runningCrcLow);

		// set the self clearing Upgrade Data Block Ready bit (15) in register 0xB04C
		// TODO:
		// mdioRdy(10);
		mdio_write(moduleId, 0xB04C, 0x8080);
	

		// Confirm the successful block download (Bits 15:14 of 0xB051 = 0x2)
		val = mdio_read(moduleId, 0xB051);
		if ((val >> 14) == 0x2) {
			fprintf(stderr, "Block Download failed\n");
			val = val & 0x0003;
			if (val == 0x1) {
				fprintf(stderr, "CRC Error\n");
			} else if (val == 0x2) {
				fprintf(stderr, "Length Error\n");
			} else if (val == 0x3) {
				fprintf(stderr, "Flash Error\n");
			} else if (val == 0x4) {
				fprintf(stderr, "Bad Image Error\n");
			} else {
				fprintf(stderr, "Unknown error\n");
			}
			break;
		}
	}

	// Complete the downlad process by writing 0xB04D bits 15:12 to 0x2
	// TODO:
	// mdioRdy(10);
	mdio_write(moduleId, 0xB04D, 0x2400);
	// Confirm the successful image download (Bits 15:14 of 0xB051 = 0x1)
	val = mdio_read(moduleId, 0xB051) >> 14;
	if (val == 0x1) {
	 	fprintf(stderr, "Image download failed\n");
	}


	// Done with Kit file
	fclose(fptr);

	// Put the module upgrade control into no operation mode
	// TODO:
	val = mdio_read(moduleId, 0xB04D) & 0x0FFF;
	// mdioRdy(10);
	mdio_write(moduleId, 0xB04D, val);
	printf("Image download complete\n");

	return 0;
}
