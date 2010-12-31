/*
    cc1100.c - 
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

#include "global.h"
#include "lpc2220.h"
#include "kernel.h"
#include "cc1100.h"
#include "irq.h"

#include "cc1100_defs.h"

/* Settings generated by SmartRF Studio */
#include "smartrf_CC1100.h"


/* Interesting bits of this configuration:
	IOCFG0: 0x06: GD0 active high, asserts when sync received, de-asserts at end of packet
			Attention! We change this later to 0x07! TODO maybe just set it here
	MDMCFG2: 0x13: 30/32 sync word bits detected, no Manchester encoding, GFSK modulation
	MDMCFG1: 0x22: 4 preamble bytes, no FEC (works only with fixed packet length)
	PKTCTRL1: 0x1A: Address check and 0 broadcast, no status append, autoflush of RX FIFO
	PKTCTRL0: 0x45: variable packet length (first byte after sync), CRC enabled, whitening on
	PKTLEN: 0x3D: maximum packet length is 61 (allows room for 2 STATUS APPEND bytes)
	ADDR: 0x01 = Device address
	CHANNR: 0x01 = Channel 1
	MCSM2: 0x04: Time-Out for sync word: 
	WORCTRL: 0x78:  RC enabled, tEvent1 = 1.3 ms, enable RC calibration, WOR_RES = 0 => max timeout after 1.9 secs.
	EVENT0: 0x4650: 18000 = 1/2 sec. timeout
	MCSM1: 0x0C: Goto IDLE after TX, stay in RX after packet received
	MCSM0: 0x38: Autocalibrate every 4th time going from RX/TX to IDLE, PO_TIMEOUT=2	
	
	So our packet looks like this:
	<4 bytes preamble> <4 bytes sync> <length> <address> <data> ... <data> <2 bytes CRC16>
	-------------------------------->       stored in RX FIFO              <--------------
	
	We limit the maximum packet length to 61 because of a bug in the CC1100 (see Errata Sheet)
	
	Currently the interrupt pin GD0 is programmed to give a falling edge for each newly received packet
*/

/* Here we overwrite some settings done by SmartRF Studio */
#define SMARTRF_SETTING_PKTCTRL0	0x45
#define SPECIAL_SETTING_PKTCTRL1	0x0E
#define SPECIAL_SETTING_ADDR		DEVICE_ADDRESS

// recommended by Smart RFStudio for 0 dBm
#define PA_VALUE	0x8E
	
static const unsigned char conf[0x2F] = {
	SMARTRF_SETTING_IOCFG2, 	// CC1100_IOCFG2     0x00;
	0x2E,				// IOCFG1
	SMARTRF_SETTING_IOCFG0D,	// CC1100_IOCFG0D    0x02;
	SMARTRF_SETTING_FIFOTHR,	//Adr. 03 FIFOTHR   RXFIFO and TXFIFO thresholds.
	0xD3 , 0x91, 			// Adr. 4, Adr. 5 
	SMARTRF_SETTING_PKTLEN,		//Adr. 06 PKTLEN    Packet length.
	SPECIAL_SETTING_PKTCTRL1,	//Adr. 07 PKTCTRL1  Packet automation control.
	SMARTRF_SETTING_PKTCTRL0,	//Adr. 08 PKTCTRL0  Packet automation control.
	SPECIAL_SETTING_ADDR,		//Adr. 09 ADDR      Device address.
	SMARTRF_SETTING_CHANNR,		//Adr. 0A CHANNR    Channel number.
	SMARTRF_SETTING_FSCTRL1,	//Adr. 0B FSCTRL1   Frequency synthesizer control.
	SMARTRF_SETTING_FSCTRL0,	//Adr. 0C FSCTRL0   Frequency synthesizer control.
	SMARTRF_SETTING_FREQ2,		//Adr. 0D FREQ2     Frequency control word, high byte.
	SMARTRF_SETTING_FREQ1,		//Adr. 0E FREQ1     Frequency control word, middle byte.
	SMARTRF_SETTING_FREQ0,		//Adr. 0F FREQ0     Frequency control word, low byte.
	SMARTRF_SETTING_MDMCFG4,	//Adr. 10 MDMCFG4   Modem configuration.
	SMARTRF_SETTING_MDMCFG3,	//Adr. 11 MDMCFG3   Modem configuration.
	SMARTRF_SETTING_MDMCFG2,	//Adr. 12 MDMCFG2   Modem configuration.
	SMARTRF_SETTING_MDMCFG1,	//Adr. 13 MDMCFG1   Modem configuration.
	SMARTRF_SETTING_MDMCFG0,	//Adr. 14 MDMCFG0   Modem configuration.
	SMARTRF_SETTING_DEVIATN,	//Adr. 15 DEVIATN   Modem deviation setting (when FSK modulation is enabled).
	0x07,				// Adr. 16 MCSM2
	0x10,				// Adr. 17 MCSM1
	SMARTRF_SETTING_MCSM0,		//Adr. 18 MCSM0     Main Radio Control State Machine configuration.
	SMARTRF_SETTING_FOCCFG,		//Adr. 19 FOCCFG    Frequency Offset Compensation Configuration.
	SMARTRF_SETTING_BSCFG,		//Adr. 1A BSCFG     Bit synchronization Configuration.
	SMARTRF_SETTING_AGCCTRL2,	//Adr. 1B AGCCTRL2  AGC control.
	SMARTRF_SETTING_AGCCTRL1,	//Adr. 1C AGCCTRL1  AGC control.
	SMARTRF_SETTING_AGCCTRL0,	//Adr. 1D AGCCTRL0  AGC control.
	0x46 , 0x50 , 0x78,		// Adr. 1E, 1F, 20 
	SMARTRF_SETTING_FREND1,		//Adr. 21 FREND1    Front end RX configuration.
	SMARTRF_SETTING_FREND0,		//Adr. 22 FREND0    Front end TX configuration.
	SMARTRF_SETTING_FSCAL3,		//Adr. 23 FSCAL3    Frequency synthesizer calibration.
	SMARTRF_SETTING_FSCAL2,		//Adr. 24 FSCAL2    Frequency synthesizer calibration.
	SMARTRF_SETTING_FSCAL1,		//Adr. 25 FSCAL1    Frequency synthesizer calibration.
	SMARTRF_SETTING_FSCAL0,		//Adr. 26 FSCAL0    Frequency synthesizer calibration.
	0x41, 0x00,			// Adr. 27, 28
	SMARTRF_SETTING_FSTEST,		//Adr. 29 FSTEST    Frequency synthesizer calibration.
	0x7F , 0x3F,			// Adr. 2A, 2B
	SMARTRF_SETTING_TEST2,		//Adr. 2C TEST2     Various test settings.
	SMARTRF_SETTING_TEST1,		//Adr. 2D TEST1     Various test settings.
	SMARTRF_SETTING_TEST0,		//Adr. 2E TEST0     Various test settings.
};

static void
spi_init(){
	
	PCONP &= ~ (1<<PCSPI1);		// Bit 10 is 0, SPI1 is disabled
	PCONP |= (1<<PCSSP);		// Bit 21 is 1, SSP is enabled
	
	// Select SPI functionality for SCK1, MISO1 and MOSI1
	PINSEL1 &= ~( (1<<2) | (1<<3) | (1<<4) | (1<<5) | (1<<6) | (1<<7) | (1<<8) | (1<<9));
	PINSEL1 |= ((1<<3) | (1<<5) | (1<<7) | (1<<9));
	
	// NOTE this sets the bit clock. It seems to be PCLK/2 = 7.5 MHz, which is outside the CC1100 specification (6.5 MHz)
	// Assume a bit clock of 7.5 MHz. Then a single byte needs 8 clocks = 1.06 us.
	
	SSPCR0 = 0x0007;		// SPI format, 8 bits, CPOL=0, CPHA=0, SCR=0
	SSPCPSR = 0x02;			// Clock prescale divider = 2
	SSPCR1 = 0x02;			// Enable SPI
};


void 
cc1100_init(void) {
	/* P0.17 [SCK1], P0.18 [MISO1] and P0.19 [MOSI1]   are used for the SPI interface */
	/* Chip select is handled in software and is using P0.23 */
	
	// CS1, SCK1 and MOSI1 are output
	FIODIR0 |= CS1 | SCK1 | MOSI1;

	// SCK1 and MISO1 and CS1 are GPIO pins for the moment
	PINSEL1 &= ~( (1<<2) | (1<<3) | (1<<4) | (1<<5) | (1<<14) | (1<<15));
	
	
	FIOSET0 = SCK1;		// SCK1 = 1
	FIOCLR0 = MOSI1;	// MOSI1 = 0
	
	FIOCLR0 = CS1;		// CSn = 0
	delay(0x200);
	
	FIOSET0 = CS1;		// CSn = 1
	delay(0x200);	
	
	FIOCLR0 = CS1;		// CSn = 0
	FIOCLR0 = SCK1;		// SCK1 = 0
	while (FIOPIN0 & MISO1);
	
	spi_init();

	cc1100_strobe(SRES);		// Issue a software RESET to CC1100
	
	// Initialize configuration registers 0x00 - 0x2E
	cc1100_write((0x00 | BURST ),(unsigned char*)conf, 0x2f);
	
	cc1100_strobe(SIDLE);
}

/*
	The SSP SPI bus is used exclusively to access the CC1100 device.
	Every communication with CC1100 is initiated by this program as master by sending a header byte.
	We have different kind of access methods because the CC1100 has three different kind of registers:
		1. Configuration registers (0x00-0x2E), read and write, burst possible
		2. Status registers (0x30-0x3D), read only, burst bit must be set, but single byte transfer
		3. Command strobe registers (0x30-0x3D),  read and write, no burst bit allowed, access causes action
		4. Multi byte registers (0x3E-0x3F), read and write may access different registers, burst allowed 
	
	Some specifics:
		Write to configuration register (0x00-0x2E): R/W bit is cleared, send header, send byte(s), optional get status
		Read from configuration register (0x00-0x2E): R/W bit is set, send header, send null byte(s) and simultaneously read data bytes
			The burst bit can be set if multiple consecutive registers are read or written
		 
		Read from status register (0x30-0x3D): Burst Bit is set, R/W bit is set, send header, read 1 byte
			Burst mode is not possible (but burst bit must be set), write is not possible
			
		Send command strobe (0x30-0x3D): Burst Bit is cleared, R/W bit is don't care, send header, optional get status 
			Setting the R/W bit determines the interpretation of the returned status byte
			Attention: Commands SXOFF and SPWD are executed when CSn goes high
	
		Write to TXFIFO (0x3F): R/W bit is 0, burst bit can be set, send header, send byte(s)
			If we send only one byte, CSn can remain low

		Read from RXFIFO (0x3F): R/W bit is 1, burst bit can be set, send header, send null byte(s), get data byte(s) 
			If we read only one byte, CSn can remain low
			
		Read from PATABLE (0x3E): R/W bit is 1, send header, send up to 8 null bytes, read data bytes, set CSn high
		Write to PATABLE (0x3E): R/W bit is 0, send header, send up to 8 data bytes, set CSn high
	
	Two caveats: 
		The status byte and any status register bytes read may be incorrect when radio is operating
		The last byte in the RX FIFO might be doubled, if we read it when radio is receiving


	The spi_write and spi_read routines must be used as atomic operations, that is they are not reentrant
*/
// We hope that the SSP SPI is inactive when calling this routine and that SPI_RXFIFO and SPI_TXFIFO are empty
// If not, we have to wait some time and discard some data. This really should not occur 
int 
spi_write(unsigned int addr, uint8 *buf, unsigned int len){
	unsigned char status;
	int i;
	
	FIOCLR0 = CS1;			// Assert Chip Select for CC1100
	
	while (SPI_BSY);	// Make sure that SPI is not busy and SPI_TXFIFO is empty
	
	while (SPI_RNE) status = SSPDR;	// Make sure that SPI_RXFIFO is empty
	
	// Wait for SO to go low (immediately if CC1100 was in active mode, but takes longer than 150 us if it was in sleep mode )
	i=0;
	while (FIOPIN0 & MISO1){
		delay(5);
		if (i++ > 10000) 
			return -5;	
	};
	
	SSPDR = addr;		// send address (BURST and/or READ/WRITE must have been set before)
	while (SPI_BSY);	// Wait while SSP is busy
	status = SSPDR;

	for (i=0; i < len; i++) {
//		while (!(SSPSR & TNF)) {};	// Wait while the TX fifo is full 
		SSPDR = buf[i];			// send data
		while (SPI_BSY);		// Wait while SSP is busy
		status=SSPDR;
	};
	
	FIOSET0 = CS1;
	
	return(status);
};

/* Read some bytes via the SPI bus 
	Starting from register addr, reading len bytes, storing them into buf
	The BURST and R/W bits must be correctly set in addr
	Returns the current status of CC1100

// We hope that the SSP SPI is inactive when calling this routine and that RX FIFO and TX FIFO is empty
// If not, we have to wait some time and discard some data. This really should not occur 	
*/
int 
spi_read(unsigned int addr, unsigned char *buf, unsigned int len) {

	unsigned short i;
	unsigned char status;
			
	FIOCLR0 = CS1;				// chip select for CC1100
	
	while (SPI_BSY);		// Make sure that SPI is not busy and TXFIFO is empty
	
	while (SPI_RNE) status = SSPDR;	// Make sure that SPI RXFIFO is empty

	// Wait for MISO1 to go low
	i=0;
	while (FIOPIN0 & MISO1){
		delay(5);
		if (i++ > 10000) 
			return -5;	
	};
	
	SSPDR = addr;				// send address
	while (SPI_BSY);			// Wait while SSP is busy
	status = SSPDR;

			
	for (i=0; i < len; i++) {
		SSPDR = 0x00;			// get next data byte
		while (SPI_BSY);		// Wait while SSP is busy
		buf[i]=SSPDR;			// read data into buffer
	};
	
	FIOSET0 = CS1;				// no chip select

	return(status);	
}


#if 0
static volatile unsigned char * spi_rx_buf;
static volatile unsigned int spi_rx_cnt;
static volatile unsigned char * spi_tx_buf;
static volatile unsigned int spi_tx_cnt; 

// We get this interrupt, if the transmit fifo is at least half empty.
// We must send new data (if any).
void spi_irq(){
	
	/* Read any data so far */
	while (SPI_RNE) spi_rx_buf[spi_rx_cnt++] = SSPDR;
	
	while (SPI_TNF){
		if (spi_tx_cnt >= spi_tx_lim){
			SSPIMSC &= ~TXIM; 		// Disable further TX interrupts. We finished the transfer.
			return ;
};	
		SSPDR = spi_tx_buf[spi_tx_cnt++];
};						// Now we have stuffed the TX fifo. The IRQ should have vanished.
};
#endif

/* Write multiple bytes to cc1100 */
int 
cc1100_write(unsigned char addr,	unsigned char *data, unsigned char length) {
	return (spi_write (addr|WRITE, data, length));
};

/* Write a single byte to cc1100 */
int 
cc1100_write1(unsigned char addr,unsigned char data) {
	return (spi_write(addr|WRITE, &data, 1));
};

/* Read a single byte from cc1100 */
unsigned char
cc1100_read1(unsigned char addr) {
	unsigned char r;
	spi_read (addr|READ, &r, 1); 
	return r;
}

/* Read multiple bytes from cc1100 into given data buffer */
int 
cc1100_read(unsigned char addr, unsigned char* data, unsigned char length) {
	return (spi_read (addr|READ, data, length));

};

/* Read a status register (0x30 - 0x3D) of CC1100 on the fly.
	Means we have to read until no change occurs because of a bug in the CC1100
	Status registers have to be read with the BURST bit set (and the READ bit)
*/
uint8 
cc1100_read_status_reg_otf(uint8 reg){
	uint8 res1, res2;
	
	reg = reg | BURST | READ;
	
	spi_read (reg, &res2, 1);
	do {
		res1 = res2;				// last value read => res1
		spi_read (reg, &res2, 1);	// new value => res2
	} while (res1 != res2);

	if (res1 == -5)
		dbg("spi failed");
	return res1;
};

/* Send a command strobe, the burst bit in cmd must be already cleared! */
int 
cc1100_strobe(uint8 cmd) {
	return (spi_write(cmd, (void *) 0, 0));
}; 


