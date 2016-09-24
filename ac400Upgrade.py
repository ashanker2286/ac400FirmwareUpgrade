#-------------------------------------------------------------------------------
# Name:AC100/AC100M FW Update
# Purpose: Load updated FW for MSA compliant device/module
#
# Author: gmiller
#
# Created: 11/21/2014
# Copyright: (c) Acacia Communications, Inc. 2014
#-------------------------------------------------------------------------------
#!/usr/bin/python
import sys
import string
import binascii
import re
import time
from socket import *
################################################################################
## TestManager class - creates the MDIO reads/writes for the Acacia eval card
## This TestManager class is not required for users that have direct MDIO access
################################################################################

class TestManager:

    def __init__(self, logfile=None, sock=None):
        if sock is None:
            self.sock = socket(AF_INET, SOCK_STREAM)
            self.sock.settimeout (20) # raise timeout exception after blocking 20 seconds
        else:
            self.sock = sock

    def connect(self, host, port = 34111):
        self.host = host
        self.port = port
        self.sock.connect((host, port))

    def sendAddress(self, addr):
        packet = '\x41\x63\x61\x63\x00\x05\x00\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04' + chr(addr>>8) + chr(addr&0xff) + '\x00\x00'
        self.sock.send(str(packet))
        data = self.sock.recv(1024)

    def readMdioWord(self, addr):
        return self.readWord(addr)

    def readWord(self, addr):
        # Address phase
        self.sendAddress(addr)
        # Read phase
        packet = '\x41\x63\x61\x63\x00\x05\x00\x09\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04' + chr(addr>>8) + chr(addr&0xff) + '\x00\x00'
        self.sock.send(str(packet))
        data = self.sock.recv(1024)
        # Convert and return
        if len (data) < 28:
            raise Warning ('runt response %d bytes 0x%s' % (len(data), binascii.hexlify (data)))
        bdata = bytearray(data[26:])
        return (bdata[0]<<8 | bdata[1])

    def writeMdioWord(self, addr, value):
        self.writeWord(addr, value)

    def writeWord(self, addr, value):
        # Address phase
        self.sendAddress(addr)
        # Write phase
        packet ='\x41\x63\x61\x63\x00\x05\x00\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x04' + chr(addr>>8) + chr(addr&0xff) + chr(value>>8) + chr(value&0xff)
        self.sock.send(str(packet))
        data = self.sock.recv(1024)
        return

    # Check that the MDIO is ready for write
    def mdioRdy(self):
        print "Checking for MDIO ready."
        for i in range (1,10):
            mdio_rdy = self.readMdioWord(0xB050)
            print "Checking for MDIO ready.", mdio_rdy
            if((mdio_rdy & 0x8000) == 0):
                time.sleep(1)
            else:
                break
        if (i == 10):
            print("ERROR: Ready for write bit not ready"); return
        return


    def acPrint(self, s, newline = True, tstamp = False):
        if(tstamp):
            s = "%s - %s" % (self.getTimeStamp(), s)
        if newline: print s
        else: print s,
        sys.stdout.flush()
        if self.logfile:
            try:
                with open(self.logfile, 'a') as fd:
                    if(newline):
                        fd.write(s + '\n')
                    else:
                        fd.write(s)
            except Exception: pass

################################################################################
## Test17: FW Image Download
## Users with direct MDIO access can replace the readMdioWord and writeMdioWord
## functions below with direct MDIO accesses
################################################################################
    def test17(testMgr):
        print "Running Image download."
        # Validate input parameter(s)
        if(testMgr is None): return
        # Initialize
        err = False
        # Check that the MDIO is ready for write

        def mdioRdy(count):
            for i in range (1,count):
                mdio_rdy = testMgr.readMdioWord(0xB050)
                if((mdio_rdy & 0x8000) == 0):
                    time.sleep(1)
                else:
                    break
            if (i == count):
                print("ERROR: Ready for write bit not ready"); return
            return
        try:
            # Open kit file and get the file length
            kitFile = open("cfp_ab_kit_01_03_36.ac100m", "rb")
            kitFile.seek(0, 2)
            fileLength = kitFile.tell()
            # Create the 256 entry CRC table based on the generator polynomial for AC100/AC100M
            # polynomial = 0xEDB88320, endian = big, CRC Init = 0, CRC XOR = 0xFFFFFFFF
            polynomial = 0xEDB88320L
            crcTable = []
            for i in range (256):
                crcAccum = i << 24
                for j in range (8):
                    if crcAccum & 0x80000000L:
                        crcAccum = ((crcAccum << 1) & 0xFFFFFFFF) ^ polynomial
                    else:
                        crcAccum = (crcAccum << 1) & 0xFFFFFFFF
                crcTable.append(crcAccum)
            # Set the download block size - 256 bytes for AC100/AC100M, or 0x0080
            mdioRdy(10)
            testMgr.writeMdioWord(0xBC00, 0x0080)
            
            # Request a firmware download via read/modify/write of address 0xB04D
            regB04D = testMgr.readMdioWord(0xB04D) | 0x1000
            mdioRdy(10)
            testMgr.writeMdioWord(0xB04D, regB04D)

            # Start reading kit file from the beginning.
            kitFile.seek(0, 0)
            # Set the CRC initialization and XOR values.
            crcInit = 0x00000000
            crcXor = 0xFFFFFFFF

            # Read 256 bytes at a time and write to MDIO 16 bits at a time &
            # calculate the CRC for 256 byte blocks
            for i in range (fileLength/256):
                runningCrc = crcInit
                # Write the 256 bytes starting from MDIO address 0xBC01
                addr = 0xBC01
                # Read 2 bytes at a time and process
                for j in range (128):
                    byte1 = ord(kitFile.read(1))
                    byte2 = ord(kitFile.read(1))
                    data = byte1 << 8 | byte2 << 0
                    testMgr.writeMdioWord(addr, data)
                    addr += 1
                    # Process the first byte for the block CRC
                    pointer = ((runningCrc >> 24) & 0xFF) ^ byte1
                    runningCrc = (((runningCrc << 8) & 0xFFFFFFFF) ^ crcTable[pointer])

                    # Process the second byte for the block CRC
                    pointer = ((runningCrc >> 24) & 0xFF) ^ byte2
                    runningCrc = (((runningCrc << 8) & 0xFFFFFFFF) ^ crcTable[pointer])
            
                # Process the running CRC and write the value for the block to MDIO
                # addresses 0xBC81 and 0xBC82
                runningCrc = runningCrc ^ crcXor
                runningCrcHigh = (runningCrc >> 16) & 0xFFFF
                runningCrcLow = runningCrc & 0xFFFF
                mdioRdy(10)
                testMgr.writeMdioWord(0xBC81, runningCrcHigh)
                mdioRdy(10)
                testMgr.writeMdioWord(0xBC82, runningCrcLow)
                # set the self clearing Upgrade Data Block Ready bit (15) in register 0xB04C
                mdioRdy(10)
                testMgr.writeMdioWord(0xB04C, 0x8080)

                # Confirm the successful block download (Bits 15:14 of 0xB051 = 0x2)
                if (testMgr.readMdioWord(0xB051) >> 14 <> 0x2):
                    print "Block download failed."
                    if (testMgr.readMdioWord(0xB051) & 0x0003 == 0x1): print "CRC Error"
                    elif (testMgr.readMdioWord(0xB051) & 0x0003 == 0x2): print "Length Error"
                    elif (testMgr.readMdioWord(0xB051) & 0x0003 == 0x3): print "Flash Error"
                    elif (testMgr.readMdioWord(0xB051) & 0x0003 == 0x4): print "Bad Image Error"
                    else: priint "Unknown Error"
                    break

            # Complete the downlad process by writing 0xB04D bits 15:12 to 0x2
            mdioRdy(10)
            testMgr.writeMdioWord(0xB04D, 0x2400)
            # Confirm the successful image download (Bits 15:14 of 0xB051 = 0x1)
            if (testMgr.readMdioWord(0xB051) >> 14 <> 0x1):
                print "Image download failed."

            # Done with kit file
            kitFile.close()
            # Put the module upgrade control into no operation mode
            regB04D = testMgr.readMdioWord(0xB04D) & 0x0FFF
            mdioRdy(10)
            testMgr.writeMdioWord(0xB04D, regB04D)
            print "Image download complete."

        finally:
            return(err)

################################################################################
def runFwLoad(TEST):
################################################################################
    # T17 = Commit image to Flash
    testMgr = TestManager(None)

    # Run tests - connect to eval card at 10.50.2.28, port 34111
    testMgr.connect("10.50.2.28")

    for t in re.findall (r'\bT\d+\b', TEST):
        try:
            error = eval (t.replace('T','test') + '(testMgr)')
        except Exception:
            error = 1
            testMgr.acPrint (''.join(traceback.format_exception(*sys.exc_info())))

        if error:
            regError = 1
            if EXIT_ON_FAIL:
                break

TEST = "T17"
runFwLoad(TEST)
