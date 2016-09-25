#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/io.h>
#include <pthread.h>

#define MDIO_ACCESS_TYPE_WRITE 0x0
#define MDIO_ACCESS_TYPE_READ  0x1

#define LPC_IO_ADDR 0x100
#define LPC_IO_NUMS 0xc0

#define MDIO_CMD_ADDR 0x61
#define MDIO_DEVICE_TYPE 0x1
#define MDIO_DEVICE_TYPE_ADDR 0x62
#define MDIO_PHY_ADDR 0x63
#define MDIO_REG_MSB_ADDR 0x64
#define MDIO_REG_LSB_ADDR 0x65
#define MDIO_WR_DATA_MSB_ADDR 0x66
#define MDIO_WR_DATA_LSB_ADDR 0x67
#define MDIO_RD_DATA_MSB_ADDR 0x68
#define MDIO_RD_DATA_LSB_ADDR 0x69

#define MDIO_START_SHIFT (6)
#define MDIO_START_MASK (0x1 << MDIO_START_SHIFT)
#define MDIO_OP_CODE_SHIFT (4)
#define MDIO_OP_CODE_MASK (0x3 << MDIO_OP_CODE_SHIFT)
#define MDIO_INTF_SEL_SHIFT (2)
#define MDIO_INTF_SEL_MASK (0x3 << MDIO_INTF_SEL_SHIFT)
#define MDIO_BUS_BUSY_SHIFT (1)
#define MDIO_BUS_BUSY_MASK (0x1 << MDIO_BUS_BUSY_SHIFT)
#define MDIO_READ_STATUS_SHIFT (0)
#define MDIO_READ_STATUS_MASK (0x1 << MDIO_READ_STATUS_SHIFT)
#define MDIO_DEVICE_TYPE_SHIFT (0)
#define MDIO_DEVICE_TYPE_MASK (0x1f << MDIO_DEVICE_TYPE_SHIFT)
#define MDIO_PHY_ADDR_SHIFT (0)
#define MDIO_PHY_ADDR_MASK (0x1f << MDIO_PHY_ADDR_SHIFT)

#define MDIO_OP_CODE_WITH_ADDRESS (0x0)
#define MDIO_OP_CODE_WITH_WRITE (0x1)
#define MDIO_OP_CODE_WITH_READ (0x3)

//#define MDIO_DELAY_TIME 10000
#define MDIO_DELAY_TIME 100
#define MDIO_DELAY_COUNT 100

#define MDIO_BUSY_ERROR (-1)
#define MDIO_TIMEOUT_ERROR (-2)
#define MDIO_WR_RD_ERROR (-3)

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int mdio_operate(int addr, uint16_t reg, int flag, uint16_t *value)
{
	int delay = 0;
	int mdio_sel = 0;
	unsigned char temp, mode;

	while(inb(LPC_IO_ADDR + MDIO_CMD_ADDR) & MDIO_BUS_BUSY_MASK) {
		if(delay++ < MDIO_DELAY_COUNT)
			usleep(MDIO_DELAY_TIME);
		else {
			return MDIO_BUSY_ERROR;
		}
	}

	/*mdio interface select ac400_1 or ac4002*/
	if(addr == 0) {
		mdio_sel = 0x0;
	} else {
		mdio_sel = 0x1;
	}
	temp = inb(LPC_IO_ADDR + MDIO_CMD_ADDR);
	temp = (temp & (~MDIO_INTF_SEL_MASK)) | mdio_sel << MDIO_INTF_SEL_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_CMD_ADDR);

	/*fill port address*/
	temp = inb(LPC_IO_ADDR + MDIO_PHY_ADDR);
	temp = (temp & (~MDIO_PHY_ADDR_MASK)) | addr << MDIO_PHY_ADDR_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_PHY_ADDR);
	

	/*fill device type*/
	temp = inb(LPC_IO_ADDR + MDIO_DEVICE_TYPE_ADDR);
	temp = (temp & (~MDIO_DEVICE_TYPE_MASK)) | MDIO_DEVICE_TYPE << MDIO_DEVICE_TYPE_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_DEVICE_TYPE_ADDR);

	/*fill register address*/
	temp = (unsigned char)((reg & 0xff00) >> 8);
	outb(temp, LPC_IO_ADDR + MDIO_REG_MSB_ADDR);
	temp = (unsigned char)(reg & 0x00ff);
	outb(temp, LPC_IO_ADDR + MDIO_REG_LSB_ADDR);

	/*fill OP code with address mode*/
	temp = inb(LPC_IO_ADDR + MDIO_CMD_ADDR);
	temp = (temp & (~MDIO_OP_CODE_MASK)) | MDIO_OP_CODE_WITH_ADDRESS << MDIO_OP_CODE_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_CMD_ADDR);

	/*MDIO control start, send address*/
	temp = inb(LPC_IO_ADDR + MDIO_CMD_ADDR);
	temp = (temp & (~MDIO_START_MASK)) | 0x1 << MDIO_START_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_CMD_ADDR);

	delay = 0;
	while(inb(LPC_IO_ADDR + MDIO_CMD_ADDR) & MDIO_BUS_BUSY_MASK) {
		if(delay++ < MDIO_DELAY_COUNT)
			usleep(MDIO_DELAY_TIME);
		else {
			return MDIO_TIMEOUT_ERROR;
		}
	}

	/*fill data if writing*/
	if(!flag) {
		/*write mode*/
		mode = MDIO_OP_CODE_WITH_WRITE;
		temp = (unsigned char)((*value & 0xff00) >> 8);
		outb(temp, LPC_IO_ADDR + MDIO_WR_DATA_MSB_ADDR);
		temp = (unsigned char)(*value & 0x00ff);
		outb(temp, LPC_IO_ADDR + MDIO_WR_DATA_LSB_ADDR);
	} else {
		/*read mode*/
		mode = MDIO_OP_CODE_WITH_READ;
	}

	/*fill OP code with read/write mode*/
	temp = inb(LPC_IO_ADDR + MDIO_CMD_ADDR);
	temp = (temp & (~MDIO_OP_CODE_MASK)) | mode << MDIO_OP_CODE_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_CMD_ADDR);

	/*MDIO control start, read or write operation*/
	temp = inb(LPC_IO_ADDR + MDIO_CMD_ADDR);
	temp = (temp & (~MDIO_START_MASK)) | 0x1 << MDIO_START_SHIFT;
	outb(temp, LPC_IO_ADDR + MDIO_CMD_ADDR);

	delay = 0;
	while(inb(LPC_IO_ADDR + MDIO_CMD_ADDR) & MDIO_BUS_BUSY_MASK) {
		if(delay++ < MDIO_DELAY_COUNT)
			usleep(MDIO_DELAY_TIME);
		else {
			return MDIO_WR_RD_ERROR;
		}
	}

	if(flag) {
		/*read mode*/
		if(inb(LPC_IO_ADDR + MDIO_CMD_ADDR) & MDIO_READ_STATUS_MASK) {
			temp = inb(LPC_IO_ADDR + MDIO_RD_DATA_LSB_ADDR);
			*value = temp;
			temp = inb(LPC_IO_ADDR + MDIO_RD_DATA_MSB_ADDR);
			*value |= temp << 8;
		} else {
			return MDIO_WR_RD_ERROR;
		}
	}

	return 0;
}

static int mdio_access(int connection, int accessType, uint16_t addr, uint16_t *val)
{
	int ret;

    //Lock mutex before MDIO access
    pthread_mutex_lock(&mutex);
	if(ioperm(LPC_IO_ADDR, LPC_IO_NUMS, 1)) {
		syslog(LOG_ERR, "Error: ioperm setting Failed!\n");
        pthread_mutex_unlock(&mutex);
		return -1;
	}
	ret = mdio_operate(connection, addr, accessType, val);
	if(ret < 0) {
		syslog(LOG_ERR, "Error: mdio_operate Failed %d!\n", ret);
        pthread_mutex_unlock(&mutex);
		return -1;
	}
	if(ioperm(LPC_IO_ADDR, LPC_IO_NUMS, 0)) {
		syslog(LOG_ERR, "Error: ioperm release Failed!\n");
        pthread_mutex_unlock(&mutex);
		return -1;
	}
    //Free mutex after MDIO access
    pthread_mutex_unlock(&mutex);
	return 0;
}

static uint8_t lpc_io_read(uint16_t port)
{
	uint8_t val = 0;
	//Add LPC offset to port addr
	port += LPC_IO_ADDR;
	if (ioperm(port, 1, 1))
	{
		syslog(LOG_ERR, "%s : Failed to get ioperm", __func__);
		return 0;
	}
	val = inb(port);
	//FIXME : From celestica code, determine if this is really needed
	usleep(100000);
	if (ioperm(port, 1, 0))
	{
		syslog(LOG_ERR, "%s : Failed to release ioperm", __func__);
		return 0;
	}
	return val;
}

static int lpc_io_write(uint16_t port, uint8_t val)
{
	//Add LPC offset to port addr
	port += LPC_IO_ADDR;
	if (ioperm(port, 1, 1))
	{
		syslog(LOG_ERR, "%s : Failed to get ioperm", __func__);
		return -1;
	}
	outb(val, port);
	//FIXME : From celestica code, determine if this is really needed
	usleep(100000);
	if (ioperm(port, 1, 0))
	{
		syslog(LOG_ERR, "%s : Failed to release ioperm", __func__);
		return -1;
	}
	return 0;
}

static int lpc_io_rdmdfywr(uint16_t port, uint8_t clr, uint8_t set)
{
	uint8_t val;
	val = lpc_io_read(port);
	val &= ~clr;
	val |= set;
	return(lpc_io_write(port, val));
}

uint16_t mdio_read(int connection, uint16_t addr) {
    uint16_t value;
    mdio_access(connection, MDIO_ACCESS_TYPE_READ, addr, &value);
   
    //printf("MDIO READ Addr:0x%X ---------> Val:0x%X\n", addr, value);
    return value;
}

int mdio_write(int connection, uint16_t addr, uint16_t val) {
    //printf("MDIO WRITE Addr:0x%X Val:0x%X\n", addr, val);
    int rv; 
    uint16_t value = val;
    rv = mdio_access(connection, MDIO_ACCESS_TYPE_WRITE, addr, &value);
    return rv; 
}


