#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>


unsigned long crc_table[256];

int isFileExist(char * fileName) {
	struct stat buf;
	int retVal = 0;

	retVal = stat(fileName, &buf);
	if (retVal == 0) {
		return 1;
	}
	return 0;
}

int getFWVersion(int moduleId) {
        uint16_t val = 0;
        uint8_t firmA_build_num, firmB_build_num;
        uint16_t firmA_X_ver, firmA_Y_ver;
        uint16_t firmB_X_ver, firmB_Y_ver;


        val = mdio_read(moduleId, 0xB051);
        if ((val & 0x1000) >> 12) {
                printf("Currently Running Image is A (val = 0x%x)\n", val);
        } else {
                printf("Currently Running Image is B (val = 0x%x)\n", val);
        }

        val = mdio_read(moduleId, 0x9050);
        firmA_build_num = val & 0xFF;
        firmB_build_num = (val >> 8) & 0xFF;
        printf("Currently Running Firmware A Build Number: 0x%x\n", firmA_build_num);
        printf("Currently Running Firmware B Build Number: 0x%x\n", firmB_build_num);

        firmA_X_ver = mdio_read(moduleId, 0x806C);
        firmA_Y_ver = mdio_read(moduleId, 0x806D);
        firmB_X_ver = mdio_read(moduleId, 0x807B);
        firmB_Y_ver = mdio_read(moduleId, 0x807C);

        printf("Firmware A Version %d.%d\n", firmA_X_ver, firmA_Y_ver);
        printf("Firmware B Version %d.%d\n", firmB_X_ver, firmB_Y_ver);
        return 0;
}


   /* Make the table for a fast CRC. */
void make_crc_table(void)
{
	unsigned long c;
	int n, k;

	for (n = 0; n < 256; n++) {
		c = (unsigned long) n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
   				c = 0xedb88320L ^ (c >> 1);
			else
   				c = c >> 1;
		}
		crc_table[n] = c;
	}
}

int main(int argc, char **argv) {
	char fileName[256] = {0};
	FILE *fptr = NULL;
	int fileLen = 0;
	uint32_t polynomial = 0;
	uint32_t crcTable[256] = {0};
	int i, j, k;
	uint32_t crcInit = 0, crcXor = 0, crcAccum = 0;
	uint32_t runningCrc = 0;
	uint16_t addr = 0, runningCrcLow, runningCrcHigh;
	uint8_t byte1, byte2, pointer;
	uint16_t data = 0;
	int moduleId = 0;
	uint16_t val;
	uint8_t cnt = 0;

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


	//Check for the current state
	val = mdio_read(moduleId, 0xB016);
	if (val != 0x0002) {
		printf("Module is not in low power state, hence existing\n");
		//return -1;
	} else {
		printf("Module is in Low Power State\n");
	}

	getFWVersion(moduleId);

	val = mdio_read(moduleId, 0xB04D);
	val = (val & 0xF000) >> 12;

	val = mdio_read(moduleId, 0xB051);
	if (val & 0x0080) {
		printf("Image Commit is B\n");
	} else {
		printf("Image Committed is A\n");
	}

	if (val & 0x1000) {
		printf("Image Running is B\n");
	} else {
		printf("Image Running is A\n");
	}
	

	// TODO:
	// Set the download block size - 256 bytes for AC100/AC100M, or 0x0080
	// mdioRdy(10);
	mdio_write(moduleId, 0xBC00, 0x0080);

	// TODO:
	// Request a firmware download via read/modify/write of address 0xB04D
	val = mdio_read(moduleId, 0xB04D) | 0x1000;
	// mdioRdy(10);
	mdio_write(moduleId, 0xB04D, val);
	val = mdio_read(moduleId, 0xB04D);

	cnt = 0;
	while (cnt < 30) {
		val = mdio_read(moduleId, 0xB051);
		if (((val & 0xC000) >> 14) == 0x2) {
			printf("Successful download start request\n");
			break;
		}
		cnt++;
		sleep(1);
	}

	if (cnt == 30) {
		printf("Failed download Start Request\n");
		return -1;
	}

	// Start reading kit file from the beginning.
	fseek(fptr, 0, SEEK_SET);

	// Set the CRC initialization and XOR values.
	crcInit = 0x00000000;
	crcXor = 0xFFFFFFFF;

	// Read 256 bytes at a time and write to MDIO 16 bits at a time &
	// calculate the CRC for 256 byte blocks
	for (i = 0; i < (fileLen/256); i++) {
		//printf("Index:%d\n", i);
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
		// mdioRdy(10);
		mdio_write(moduleId, 0xB04C, 0x8080);

		// set the self clearing Upgrade Data Block Ready bit (15) in register 0xB04C
		// TODO:
		while(1) {
			val = mdio_read(moduleId, 0xB04C);
			if ((val & 0x8000) == 0) {
				break;
			}
		}

		while (1) {
			val = mdio_read(moduleId, 0xB050);
			if ((val & 0x8000) == 0x8000) {
				break;
			}
		}

		val = mdio_read(moduleId, 0xB051);
		if ((val >> 14) != 0x2) {
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
			fclose(fptr);
			return -1;
		}
		if (i % 100 == 0) {
			printf("Image A status 0x%X\n", ((val & 0x0C00) >> 10));
			printf("Image B status 0x%X\n", ((val & 0x0300) >> 8));
		}
	}

	// Complete the downlad process by writing 0xB04D bits 15:12 to 0x2
	// TODO:
	// mdioRdy(10);
	val = mdio_read(moduleId, 0xB04D);
	val = (val & 0x0FFF) | 0x2000;
	mdio_write(moduleId, 0xB04D, val);
	// Confirm the successful image download (Bits 15:14 of 0xB051 = 0x1)
	cnt = 0;
	while (cnt < 30) {
		val = mdio_read(moduleId, 0xB051) >> 14;
		if (val == 0x1) {
			printf("Image download complete\n");
			break;
		}
		sleep(1);
		cnt++;
	}
	if (cnt == 30) {
		fprintf(stderr, "Image download failed\n");
	}

	// Done with Kit file
	fclose(fptr);

	getFWVersion(moduleId);
	return 0;
}
