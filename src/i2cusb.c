/* Name: i2cusb.c

 * i2c_tiny_usb on LittleWire
 * Version 2.01
 * Copyright (C) 2015 Jay Carlson. See licensing terms below.
 * Modified by Jay Carlson, 2015-03-31
 * Modified by e7p, 2017-07-30
 *
 * This is a crude cut&paste of Till Harbaum's Linux i2c driver into
 * Ihsan Kehribar's LittleWire firmware, which uses obdev.at's software
 * USB stack.
 *

 -----------------------------------------------------------
 - Little Wire 
 - Firmware Version: 1.3
 - Modified by: ihsan Kehribar, 2013 September
 -----------------------------------------------------------

 -----------------------------------------------------------
 - Little Wire 
 - Firmware Version: 1.2
 - Modified by: ihsan Kehribar, 2013 April
 -----------------------------------------------------------

 -----------------------------------------------------------
 - Little Wire 
 - Firmware Version: 1.1
 - Modified by: ihsan Kehribar, 2012 July
 -----------------------------------------------------------

  modified by ihsan Kehribar, 2011 November
  
  created by chris chung, 2010 April

  based on the works found in

  v-usb framework http://www.obdev.at/vusb/
	 Project: Thermostat based on AVR USB driver
	 Author: Christian Starkjohann
    
  usbtiny isp http://www.xs4all.nl/~dicks/avr/usbtiny/
  	Dick Streefland
  
  please observe licensing term from the above two projects

	Copyright (C) 2010  chris chung

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay_basic.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdlib.h>
#include <string.h>
#include "usbdrv/usbdrv.h"

#include "temperature.h"

#define sbi(register,bit) (register|=(1<<bit))
#define cbi(register,bit) (register&=~(1<<bit))

static uint8_t *EE_addr = (uint8_t *)32;
int usbDescriptorStringSerialNumber[] = {
    USB_STRING_DESCRIPTOR_HEADER( USB_CFG_SERIAL_NUMBER_LEN ),
    USB_CFG_SERIAL_NUMBER
};

#define DEBUGF(s, ...)

#define ENABLE_SCL_EXPAND

/* commands from USB, must e.g. match command ids in kernel driver */
#define CMD_ECHO       0
#define CMD_GET_FUNC   1
#define CMD_SET_DELAY  2
#define CMD_GET_STATUS 3

#define CMD_I2C_IO     4
#define CMD_I2C_BEGIN  1  // flag fo I2C_IO
#define CMD_I2C_END    2  // flag fo I2C_IO

/* linux kernel flags */
#define I2C_M_TEN		0x10	/* we have a ten bit chip address */
#define I2C_M_RD		0x01
#define I2C_M_NOSTART		0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800

/* To determine what functionality is present */
#define I2C_FUNC_I2C			0x00000001
#define I2C_FUNC_10BIT_ADDR		0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING	0x00000004 /* I2C_M_{REV_DIR_ADDR,NOSTART,..} */
#define I2C_FUNC_SMBUS_HWPEC_CALC	0x00000008 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_READ_WORD_DATA_PEC  0x00000800 /* SMBus 2.0 */ 
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA_PEC 0x00001000 /* SMBus 2.0 */ 
#define I2C_FUNC_SMBUS_PROC_CALL_PEC	0x00002000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL_PEC 0x00004000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0x00008000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_QUICK		0x00010000 
#define I2C_FUNC_SMBUS_READ_BYTE	0x00020000 
#define I2C_FUNC_SMBUS_WRITE_BYTE	0x00040000 
#define I2C_FUNC_SMBUS_READ_BYTE_DATA	0x00080000 
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA	0x00100000 
#define I2C_FUNC_SMBUS_READ_WORD_DATA	0x00200000 
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA	0x00400000 
#define I2C_FUNC_SMBUS_PROC_CALL	0x00800000 
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0x01000000 
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000 
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK	0x04000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK	0x08000000 /* w/ 1-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK_2	 0x10000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK_2 0x20000000 /* w/ 2-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA_PEC  0x40000000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC 0x80000000 /* SMBus 2.0 */

#define I2C_FUNC_SMBUS_BYTE I2C_FUNC_SMBUS_READ_BYTE | \
                            I2C_FUNC_SMBUS_WRITE_BYTE
#define I2C_FUNC_SMBUS_BYTE_DATA I2C_FUNC_SMBUS_READ_BYTE_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_BYTE_DATA
#define I2C_FUNC_SMBUS_WORD_DATA I2C_FUNC_SMBUS_READ_WORD_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_WORD_DATA
#define I2C_FUNC_SMBUS_BLOCK_DATA I2C_FUNC_SMBUS_READ_BLOCK_DATA | \
                                  I2C_FUNC_SMBUS_WRITE_BLOCK_DATA
#define I2C_FUNC_SMBUS_I2C_BLOCK I2C_FUNC_SMBUS_READ_I2C_BLOCK | \
                                  I2C_FUNC_SMBUS_WRITE_I2C_BLOCK

#define I2C_FUNC_SMBUS_EMUL I2C_FUNC_SMBUS_QUICK | \
                            I2C_FUNC_SMBUS_BYTE | \
                            I2C_FUNC_SMBUS_BYTE_DATA | \
                            I2C_FUNC_SMBUS_WORD_DATA | \
                            I2C_FUNC_SMBUS_PROC_CALL | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC | \
                            I2C_FUNC_SMBUS_I2C_BLOCK

/* the currently support capability is quite limited */
const unsigned long func PROGMEM = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;

/* Scoped, this delay offset gives 102kHz. Close enough. */
#define DELAY_OVERHEAD 7UL

/* Scale up the delay to match 12MHz -> 16.5MHz clock */
#define DEFAULT_DELAY (((10UL * 1650UL + 60UL) / 1200UL) - DELAY_OVERHEAD)

static uint16_t clock_delay = DEFAULT_DELAY;
static uint16_t clock_delay2 = DEFAULT_DELAY / 2;

static uint16_t expected = 0;
static unsigned char saved_cmd;

/* ------------------------------------------------------------------------- */

struct i2c_cmd {
  unsigned char type;
  unsigned char cmd;
  unsigned short flags;
  unsigned short addr;
  unsigned short len;  
};

#define STATUS_IDLE          0
#define STATUS_ADDRESS_ACK   1
#define STATUS_ADDRESS_NAK   2

static uchar status = STATUS_IDLE;

uint8_t data_buffer[16];

static uchar i2c_do(struct i2c_cmd *cmd) {
  uchar addr;

  DEBUGF("i2c %s at 0x%02x, len = %d\n", 
	   (cmd->flags&I2C_M_RD)?"rd":"wr", cmd->addr, cmd->len); 

  /* normal 7bit address */
  addr = ( cmd->addr << 1 );
  if (cmd->flags & I2C_M_RD )
    addr |= 1;

  // check DEVICE address
  status = STATUS_ADDRESS_ACK;
  expected = cmd->len;
  uint16_t ds1721_value;
  switch (addr) {
  	case 0x90:
  	// accept
  	break;

  	case 0x91:
	ds1721_value = celsiusToDS1721(adcToCelsius(temperature_get()));
  	data_buffer[0] = (ds1721_value >> 8) & 0xFF;
  	data_buffer[1] = ds1721_value & 0xFF;
  	if (expected > 2) {
  		expected = 2;
  	}
  	break;

  	default:
	status = STATUS_ADDRESS_NAK;
  	break;
  }

  if (status == STATUS_ADDRESS_ACK) {
    saved_cmd = cmd->cmd;
  } else {
    expected = 0;
  }

  return(cmd->len?0xff:0x00);
}

// ----------------------------------------------------------------------
// Handle an IN packet.
// ----------------------------------------------------------------------
uint8_t x = 0;
uchar usbFunctionRead(uchar *data, uchar len)
{
  uchar i;

  DEBUGF("read %d bytes, %d exp\n", len, expected);

  if(status == STATUS_ADDRESS_ACK) {
    if(len > expected) {
      DEBUGF("exceeds!\n");
      len = expected;
    }

    // consume bytes
    uint8_t *data_buffer_ptr = data_buffer;
    for(i=0;i<len;i++) {
      expected--;
      *data = *(data_buffer_ptr++);
      DEBUGF("data = %x\n", *data);
      data++;
    }
  } else {
    DEBUGF("not in ack state\n");
    memset(data, 0, len);
  }
  return len;

}

// ----------------------------------------------------------------------
// Handle an OUT packet.
// ----------------------------------------------------------------------
uchar usbFunctionWrite(uchar *data, uchar len)
{
  uchar i;

  DEBUGF("write %d bytes, %d exp\n", len, expected);

  if(status == STATUS_ADDRESS_ACK) {
    if(len > expected) {
      DEBUGF("exceeds!\n");
      len = expected;
    }

    // consume bytes
    for(i=0;i<len;i++) {
      expected--;
      DEBUGF("data = %x\n", *data);
      // TODO: read the data bytes... (?)
    }
  } else {
    DEBUGF("not in ack state\n");
    memset(data, 0, len);
  }

  return len;
}


/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */
uchar	usbFunctionSetup(uchar data[8])
{
  static uchar replyBuf[4];
  usbMsgPtr = replyBuf;
  DEBUGF("Setup %x %x %x %x\n", data[0], data[1], data[2], data[3]);

  switch(data[1]) {

  case CMD_ECHO: // echo (for transfer reliability testing)
    replyBuf[0] = data[2];
    replyBuf[1] = data[3];
    return 2;
    break;

  case CMD_GET_FUNC:
    memcpy_P(replyBuf, &func, sizeof(func));
    return sizeof(func);
    break;

  case CMD_SET_DELAY:
    break;

  case CMD_I2C_IO:
  case CMD_I2C_IO + CMD_I2C_BEGIN:
  case CMD_I2C_IO                 + CMD_I2C_END:
  case CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END:
    // these are only allowed as class transfers

    return i2c_do((struct i2c_cmd*)data);
    break;

  case CMD_GET_STATUS:
    replyBuf[0] = status;
    return 1;
    break;

  default:
    // must not happen ...
    break;
  }

  return 0;  // reply len

}

/* ------------------------------------------------------------------------- */
/* ------------------------ Oscillator Calibration ------------------------- */
/* ------------------------------------------------------------------------- */

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void)
{
uchar       step = 128;
uchar       trialValue = 0, optimumValue;
int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

    /* do a binary search: */
    do{
        OSCCAL = trialValue + step;
        x = usbMeasureFrameLength();    /* proportional to current real frequency */
        if(x < targetValue)             /* frequency still too low */
            trialValue += step;
        step >>= 1;
    }while(step > 0);
    /* We have a precision of +/- 1 for optimum OSCCAL here */
    /* now do a neighborhood search for optimum value */
    optimumValue = trialValue;
    optimumDev = x; /* this is certainly far away from optimum */
    for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
        x = usbMeasureFrameLength() - targetValue;
        if(x < 0)
            x = -x;
        if(x < optimumDev){
            optimumDev = x;
            optimumValue = OSCCAL;
        }
    }
    OSCCAL = optimumValue; 
}

void usbEventResetReady(void)
{
    calibrateOscillator();
    eeprom_write_byte(0, OSCCAL);   /* store the calibrated value in EEPROM */
}

void usb_setup(void) {
	uchar i;
	uchar   calibrationValue;
	
    calibrationValue = eeprom_read_byte(0); /* calibration value from last time */
    if(calibrationValue != 0xff){
        OSCCAL = calibrationValue;
    }

    usbDeviceDisconnect();
    for(i=0;i<20;i++){  /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();

    wdt_enable(WDTO_1S);

    usbInit();
    sei();
}

void usb_loop(void) {
	wdt_reset();
	usbPoll();
}
