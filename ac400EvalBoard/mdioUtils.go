//Copyright [2016] [SnapRoute Inc]
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//	 Unless required by applicable law or agreed to in writing, software
//	 distributed under the License is distributed on an "AS IS" BASIS,
//	 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	 See the License for the specific language governing permissions and
//	 limitations under the License.
//
// _______  __       __________   ___      _______.____    __    ____  __  .___________.  ______  __    __
// |   ____||  |     |   ____\  \ /  /     /       |\   \  /  \  /   / |  | |           | /      ||  |  |  |
// |  |__   |  |     |  |__   \  V  /     |   (----` \   \/    \/   /  |  | `---|  |----`|  ,----'|  |__|  |
// |   __|  |  |     |   __|   >   <       \   \      \            /   |  |     |  |     |  |     |   __   |
// |  |     |  `----.|  |____ /  .  \  .----)   |      \    /\    /    |  |     |  |     |  `----.|  |  |  |
// |__|     |_______||_______/__/ \__\ |_______/        \__/  \__/     |__|     |__|      \______||__|  |__|
//

package main

import (
	"flag"
	"fmt"
	"net"
	"strconv"
	"sync"
	"time"
)

/*
#include <stdint.h>
#include "ac400FWUpgrade.h"
*/
import "C"

const (
	CONN_TIMEOUT              = 5
	MAX_RESPONSE_LEN          = 1024
	EXPECTED_RESPONSE_LEN     = 28
	MDIO_INTF_STATUS_REG      = 0xB050
	MDIO_INTF_READY_FOR_WRITE = 0x8000
)

//MDIO transaction byte streams
var addrCmdPrefix []byte = []byte{0x76, 0x30, 0x30, 0x32, 0x00, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}
var readCmdPrefix []byte = []byte{0x76, 0x30, 0x30, 0x32, 0x00, 0x05, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}
var writeCmdPrefix []byte = []byte{0x76, 0x30, 0x30, 0x32, 0x00, 0x05, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04}

type AC400EvalBrd struct {
	serverIP   string
	serverPort int
	conn       net.Conn
	mdioLock   sync.Mutex
}

var board AC400EvalBrd

func EvalBoardInit(targetIP string, targetPort int) error {
	board.serverIP = targetIP
	board.serverPort = targetPort
	timeout := CONN_TIMEOUT * time.Second
	conn, err := net.DialTimeout("tcp", targetIP+":"+strconv.Itoa(targetPort), timeout)
	if err != nil {
		fmt.Println("Failure dialing out to eval board on - ", board.serverIP+":"+strconv.Itoa(board.serverPort))
		return err
	} else {
		fmt.Println("Successfully connected to eval board")
	}
	//Save connection handle
	board.conn = conn
	return err
}

func EvalBoardDeInit() error {
	return (board.conn.Close())
}

//export MdioReadyForWrite
func MdioReadyForWrite() bool {
	ready := false
	for retryCnt := 0; retryCnt < 5; retryCnt++ {
		rval := MdioReadWord(C.uint16_t(MDIO_INTF_STATUS_REG))
		val := uint16(rval)
		if val&MDIO_INTF_READY_FOR_WRITE == MDIO_INTF_READY_FOR_WRITE {
			ready = true
			break
		}
		time.Sleep(time.Second)
	}
	return ready
}

//export MdioSendAddr
func MdioSendAddr(regAddr C.uint16_t) {
	var addr uint16 = uint16(regAddr)
	packet := make([]byte, len(addrCmdPrefix))
	copy(packet, addrCmdPrefix)
	packet = append(packet, byte((regAddr>>8)&0xFF), byte((regAddr & 0xFF)), 0x0, 0x0)
	//Acquire mdio lock
	board.mdioLock.Lock()
	_, err := board.conn.Write(packet)
	if err != nil {
		fmt.Println("MdioSendAddr failed - ", addr)
	}
	response := make([]byte, MAX_RESPONSE_LEN)
	_, err = board.conn.Read(response)
	if err != nil {
		fmt.Println("MdioSendAddr failed to receive response")
	}
	//Release mdio lock
	board.mdioLock.Unlock()
}

//export MdioReadWord
func MdioReadWord(regAddr C.uint16_t) C.uint16_t {
	var rval C.uint16_t
	MdioSendAddr(regAddr)
	packet := make([]byte, len(readCmdPrefix))
	copy(packet, readCmdPrefix)
	packet = append(packet, byte((regAddr>>8)&0xFF), byte((regAddr & 0xFF)), 0x0, 0x0)
	//Acquire mdio lock
	board.mdioLock.Lock()
	_, err := board.conn.Write(packet)
	if err != nil {
		fmt.Println("MdioReadWord failed - ", regAddr)
		rval = 0
	}
	response := make([]byte, MAX_RESPONSE_LEN)
	numBytesRead, err := board.conn.Read(response)
	if err != nil {
		fmt.Println("MdioReadWord failed to receive response")
	}
	//Release mdio lock
	board.mdioLock.Unlock()
	if numBytesRead < EXPECTED_RESPONSE_LEN {
		fmt.Println("MdioReadWord - Runt reponse received")
		rval = 0
	} else {
		val := uint16(response[EXPECTED_RESPONSE_LEN-2])<<8 | uint16(response[EXPECTED_RESPONSE_LEN-1])
		rval = C.uint16_t(val)
	}
	return rval
}

//export MdioWriteWord
func MdioWriteWord(addr C.uint16_t, value C.uint16_t) C.int {
	var rval C.int = C.int(0)
	if MdioReadyForWrite() {
		MdioSendAddr(addr)
		packet := make([]byte, len(writeCmdPrefix))
		copy(packet, writeCmdPrefix)
		packet = append(packet, byte((addr>>8)&0xFF), byte((addr & 0xFF)), byte((value>>8)&0xFF), byte(value&0xFF))
		//Acquire mdio lock
		board.mdioLock.Lock()
		_, err := board.conn.Write(packet)
		if err != nil {
			fmt.Println("MdioWriteWord failed - ", addr)
			rval = C.int(-1)
		}
		response := make([]byte, MAX_RESPONSE_LEN)
		_, err = board.conn.Read(response)
		if err != nil {
			fmt.Println("MdioWriteWord failed to receive response")
			rval = C.int(-1)
		}
		//Release mdio lock
		board.mdioLock.Unlock()
	} else {
		fmt.Println("MdioWriteWord - MDIO interface not ready for write")
		rval = C.int(-1)
	}
	return rval
}

//export MdioReadModifyWriteWord
func MdioReadModifyWriteWord(addr, clr, set C.uint16_t) C.int {
	fmt.Println(fmt.Sprintf("MDIO RdMdfyWr - %x, %x, %x", addr, clr, set))
	curVal := MdioReadWord(addr)
	fmt.Println(fmt.Sprintf("CUR VAL - %x", curVal))
	newVal := curVal
	newVal &= ^clr
	newVal = newVal | set
	fmt.Println(fmt.Sprintf("NEW VAL - %x", newVal))
	return (MdioWriteWord(addr, newVal))
}

func main() {
	targetIP := flag.String("IP", "", "Evaluation Board IP Address")
	targetPort := flag.Int("PORT", 0, "Evaluation Board Port Number")
	fileName := flag.String("FILE", "", "Evaluation Board FW file Name")
	flag.Parse()
	err := EvalBoardInit(*targetIP, *targetPort)
	if err != nil {
		fmt.Println("Error Initializing Evaluation Board")
		return
	}

	fmt.Println("FileName:", *fileName)
	C.AC400EvalUpgrade((C.CString)(*fileName), C.int(0))
	//C.AC400EvalUpgrade()
	return
}
