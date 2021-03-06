/*
    cc1100.h
    Copyright (C) 2007

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef cc1100_H
#define cc1100_H

#define MISO1			RST
#define MOSI1			OCB
#define SCK				OCC
#define CS				KB1
#define GDO0			KB6

#define WRITE			0x00
#define BURST			0x40
#define READ			0x80
#define TX_fifo			0x3F
#define RX_fifo			0x3F

#define SRES			0x30
#define SFSTXON			0x31
#define SXOFF			0x32
#define SCAL			0x33
#define SRX				0x34
#define STX 			0x35
#define SIDLE			0x36
#define SWOR			0x38
#define SPWD			0x39
#define SFRX			0x3A
#define SFTX			0x3B
#define SWORRST			0x3C
#define SNOP			0x3D
#define PATABLE			0x3E

// status register of the CC1100
// These registers are read only, so we can give their address with the highest two bits set.
#define MARCSTATE		0xF5
#define PKTSTATUS		0xF8
#define TXBYTES			0xFA
#define RXBYTES			0xFB

// status register contents
#define MARCSTATE_IDLE		0x01
#define MARCSTATE_RX		0x0D
#define MARCSTATE_RXFIFO_OVERFLOW 0x11
#define MARCSTATE_TXFIFO_UNDERFLOW 0x16

#define CC1100_VERSION		0x31

// Mask to get the state bits from chip status byte
#define STATE_MASK 0x70

// Chip status state
#define CHIP_IDLE		0x00
#define CHIP_RX			0x10
#define CHIP_TX 		0x20
#define CHIP_RX_OVFL	0x60
#define CHIP_TX_UNFL	0x70

// This bit in the second appended status byte is the crc bit
#define CRC_OK	0x80

/* Set to 2 if sender (Betty) appends status bytes, else 0 */
#define RX_USE_STATUS	2

/* Set to 1 if we use address check when receiving (Betty sends address), else 0 */
#define RX_USE_ADDR 1

#define MAX_PKTLEN  255

/* 
	Number of actual data bytes that we can receive/send at once over the radio link.
	This number does not include the length byte, the address byte and the appended status bytes.
*/
#define PAYLOAD_SIZE (MAX_PKTLEN - 1 - USE_ADDR - USE_STATUS) 

/* This is the maximum valid value of the length byte 
	The length byte includes the address and the data bytes.
*/
#define MAX_LEN (MAX_PKTLEN - 1 - RX_USE_STATUS) 

/* 
	The answer from MPD can be very long, so we have to disassemble the answer into small packets
	and the receiver (Betty) has to assemble them again.
	So we can simply restrict our packet length when sending to a value which fits in our TX_FIFO
	and the receivers RX_FIFO (64 bytes including protocol overhead). 
*/

/* Set to 2 if we append status bytes in TX, else 0 */
#define TX_USE_STATUS	2

/* Set to 1 if we use address check in TX , else 0 */
#define TX_USE_ADDR 1

/* Maximum number of payload bytes in TX */
#define MAX_TX_PAYLOAD	(64 - 1 - TX_USE_ADDR - TX_USE_STATUS)


/* Our own device address */
#define DEV_ADDR	0x01

void cc1100_init(void);
unsigned char cc1100_write(unsigned char addr, unsigned char* dat, unsigned char length);
unsigned char cc1100_write1(unsigned char addr, unsigned char dat);
unsigned char cc1100_read1(unsigned char addr);
unsigned char cc1100_strobe(unsigned char cmd);
void switch_to_idle();
unsigned char cc1100_tx_finished() ;
unsigned char cc1100_read_rxstatus();

/* Read the MARCSTATE register on the fly and mask its value */
#define cc1100_marcstate() (cc1100_read_status_reg_otf(MARCSTATE) & 0x1f)
 
/* Write a single byte to the CC1100 TX_FIFO */
#define cc1100_write_fifo(x) cc1100_write1(TX_fifo, (x))

/* read a single byte from the CC1100 RX_FIFO */
#define cc1100_read_fifo() cc1100_read1(RX_fifo | READ)

/* returns TRUE iff cc1100 is no longer in TX state */
#define tx_finished() (cc1100_tx_finished())
//#define tx_finished() ((cc1100_read_rxstatus() & STATE_MASK) != CHIP_TX)


#endif
