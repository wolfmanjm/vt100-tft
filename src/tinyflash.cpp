// Barebones library for Winbond W25Q80BV serial flash memory.  Written
// with the limited space of the ATtiny85 in mind, but works on regular
// Arduinos too.  Provides basic functions needed for audio playback
// system only: chip and sector erase, block write, sequential byte read.
// Other than possibly adding support for other Winbond flash in the
// future, the plan is to NOT bloat this out with all bells and whistles;
// functions like block reads or buffered writes can be implemented in
// client code where RAM can be better managed in the context of the
// overall application (1 flash page = 1/2 the ATtiny85's RAM).
// Written by Limor Fried and Phillip Burgess for Adafruit Industries.
// MIT license.

#ifdef USETOUCH
#include "tinyflash.h"

#define MYSPI SPI1
#define CMD_PAGEPROG     0x02
#define CMD_READDATA     0x03
#define CMD_WRITEDISABLE 0x04
#define CMD_READSTAT1    0x05
#define CMD_WRITEENABLE  0x06
#define CMD_SECTORERASE  0x20
#define CMD_CHIPERASE    0x60
#define CMD_ID           0x90

#define STAT_BUSY        0x01
#define STAT_WRTEN       0x02

// Currently rigged for W25Q80BV only
#define CHIP_BYTES       1L * 1024L * 1024L

#include <SPI.h>
#define CHIP_SELECT   { MYSPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0)); digitalWrite(cs_pin, LOW); }
#define CHIP_DESELECT { digitalWrite(cs_pin, HIGH); MYSPI.endTransaction(); }
#define spi_xfer(n)   MYSPI.transfer(n)

// Constructor
TinyFlash::TinyFlash(uint8_t cs) {
	cs_pin= cs;
}

// Select chip and issue command (don't deselect; data may follow)
void TinyFlash::cmd(uint8_t c) {
	CHIP_SELECT
	(void)spi_xfer(c);
}

// Initialize SPI pins, validate chip is present
uint32_t TinyFlash::begin(void) {
	uint8_t manID, devID;
	MYSPI.setMOSI(0);
	MYSPI.setMISO(1);
	MYSPI.setSCK(20);
	pinMode(cs_pin, OUTPUT);
	digitalWrite(cs_pin, HIGH);
	MYSPI.begin();
	// Resistor-based 5V->3.3V logic conversion is a little sloppy, so:
	//MYSPI.setClockDivider(SPI_CLOCK_DIV8); // 500 KHz

	cmd(CMD_ID);
	for(uint8_t i=0; i<4; i++) manID = spi_xfer(0); // 3 dummy bytes first
	devID = spi_xfer(0);
	CHIP_DESELECT

	// Chip capacity is hardcoded for now
	return ((manID == 0xEF) && (devID == 0x13)) ? CHIP_BYTES : 0L;
}

// Poll status register until busy flag is clear or timeout occurs
boolean TinyFlash::waitForReady(uint32_t timeout) {
	uint8_t  status;
	uint32_t startTime = millis();

	do {
		cmd(CMD_READSTAT1);
		status = spi_xfer(0);
		CHIP_DESELECT
		if((millis() - startTime) > timeout) return false;
	} while(status & STAT_BUSY);

	return true;
}

// Set up a read operation (no data is returned yet)
boolean TinyFlash::beginRead(uint32_t addr) {

	if((addr >= CHIP_BYTES) || !waitForReady()) return false;

	cmd(CMD_READDATA);
	(void)spi_xfer(addr >> 16);
	(void)spi_xfer(addr >>  8);
	(void)spi_xfer(addr      );
	// Chip is held in selected state until endRead()

	return true;
}

// Read next byte (call N times following beginRead())
uint8_t TinyFlash::readNextByte(void) {
	return spi_xfer(0);
}

// Stop read operation
void TinyFlash::endRead(void) {
	CHIP_DESELECT
}

// Erase the whole chip.  Boom, gone.  Use with caution.
boolean TinyFlash::eraseChip(void) {

	if(!waitForReady() || !writeEnable()) return false;

	// Might want to have this clear the block protect bits

	cmd(CMD_CHIPERASE);
	CHIP_DESELECT

	if(!waitForReady(10000L)) return false; // Datasheet says 6S max

	writeDisable();

	return true;
}

// Erase one 4K sector
boolean TinyFlash::eraseSector(uint32_t addr) {

	if(!waitForReady() || !writeEnable()) return false;

	cmd(CMD_SECTORERASE);
	(void)spi_xfer(addr >> 16); // Chip rounds this down to
	(void)spi_xfer(addr >>  8); // prior 4K sector boundary;
	(void)spi_xfer(0         ); // lowest bits are ignored.
	CHIP_DESELECT

	if(!waitForReady(1000L)) return false; // Datasheet says 400ms max

	writeDisable();

	return true;
}

// Private function used by write and erase operations
boolean TinyFlash::writeEnable(void) {
	uint8_t status;

	cmd(CMD_WRITEENABLE);
	CHIP_DESELECT

	// Verify write-enable status
	cmd(CMD_READSTAT1);
	status = spi_xfer(0);
	CHIP_DESELECT
	return (status & STAT_WRTEN) ? true : false;
}

// Companion to above function, used after write and erase operations
void TinyFlash::writeDisable(void) {
	cmd(CMD_WRITEDISABLE);
	CHIP_DESELECT
}

// Writes one page: 256 bytes, starting at 256 byte boundary.  There are
// no other options.  This is the ONLY write method provided by the library;
// other capabilities (if needed) may be implemented in client code.
boolean TinyFlash::writePage(uint32_t addr, uint8_t *data) {
	if((addr >= CHIP_BYTES) || !waitForReady() || !writeEnable())
		return false;

	cmd(CMD_PAGEPROG);
	(void)spi_xfer(addr >> 16);
	(void)spi_xfer(addr >>  8);
	(void)spi_xfer(0); // If len=256, page boundary only (datasheet 7.2.21)
	for(int i=0; i<256; i++) {
		(void)spi_xfer(data[i]);
	}
	CHIP_DESELECT // Write occurs after the CS line is de-asserted

	delay(3);     // Max page program time according to datasheet

	if(!waitForReady()) return false;

	writeDisable();

	return true;
}
#endif
