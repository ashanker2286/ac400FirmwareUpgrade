#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>


int main(int argc, char **argv) {
	uint16_t val = 0;
	int moduleId = 0;
	uint8_t firmA_build_num, firmB_build_num;
	uint16_t firmA_X_ver, firmA_Y_ver;
	uint16_t firmB_X_ver, firmB_Y_ver;


        moduleId = atoi(argv[1]);
        printf("ModuleId: %d\n", moduleId);

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
