/*
  8-bit parallel.

  in 8 bit parallel.
    is the command still 16 bit.
    which byte is loaded first.


  - gfx library seems to be a library using a canvas rather than direct draw.
      eg. may be buffering change.

  - general
    https://controllerstech.com/interface-tft-display-with-stm32/

  https://www.displayfuture.com/Display/datasheet/controller/ILI9341.pdf

  8 bit parallel,
  https://github.com/sammyizimmy/ili9341/blob/master/ili9341.c

  simple,
  https://github.com/adafruit/Adafruit_ILI9341

  primitives
    https://github.com/adafruit/Adafruit-GFX-Library/blob/master/Adafruit_SPITFT.cpp

  optimised. 70 forks. but uses spi.
    https://github.com/PaulStoffregen/ILI9341_t3

  So we would have to bit-bash.
    we can use the millisec msleep to begin with.
    a nop loop for micro-second. just write the inner loop - execute it a thousand times and blink led -

  Note - rather than setting gpio individually - would have to left/shift values into place for the bus.
    but would likely be pretty damn fast.
  --
 */


#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "clock.h"
#include "Adafruit_ILI9341.h"

// GPIOE
#define LCD_PORT  GPIOE

#define LCD_RST   GPIO2   // reset
#define LCD_CS    GPIO3   // chip select
#define LCD_RS    GPIO4   // register select
#define LCD_WR    GPIO5   // write strobe
#define LCD_RD    GPIO6   // read


#define LCD_DATA_PORT  GPIOD

// LCD

static void led_setup(void)
{
  rcc_periph_clock_enable(RCC_GPIOE); // JA
  gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0); // JA
}


// should put control registers on different port - this would allow 16 bit parallel bit bashing
// without hardly any code changes.


// OK. really not sure if we have to pad everything to 16 bit.
// sendCommand16 is a specialization for 16 bit parallel path.
// eg. should only require 16 bits, if its unavoidable because the bus is genuinely 16 bit.

static inline void delay( uint16_t x )
{
  msleep(x);
}



static void send8( uint8_t x )
{
  gpio_port_write(LCD_DATA_PORT, x);    // setup port
  gpio_clear(LCD_PORT, LCD_WR);         // clear write strobe. falling edge. host asserts.
  delay(1);
  gpio_set(LCD_PORT, LCD_WR);           // write on rising edge. tft reads.
  delay(1);

  // gpio_clear(LCD_PORT, LCD_WR);         // clear write strobe
}

// OK. screen does same thing - whether we asset chip select or not.
// So, think we want to check...

// is the screen flickering a power supply issue?
/*
  p11.

  DCX
  This pin is used to select “Data or Command” in the parallel interface or
  4-wire 8-bit serial data interface. When DCX = ’1’, data is selected. When DCX
  = ’0’, command is selected. This pin is used serial interface clock in 3-wire
  9-bit / 4-wire 8-bit serial data interface

  RDX I MCU (VDDI/VSS)   8080-    /8080I-II system (RDX): Serves as a read
    signal and MCU read data at the rising edge. Fix to VDDI level when not in use.

  WRX (D/CX) I MCU (VDDI/VSS) - 8080-    /8080I-II system (WRX): Serves as a
  write signal and writes data at the rising edge. - 4-line system (D/CX): Serves
  as command or parameter selec

  RESX I MCU (VDDI/VSS) This signal will reset the device and must be applied
  to properly initialize the chip. Signal is active low.

  CSX I MCU (VDDI/VSS) Chip select input pin (“Low” enable). This pin can be
  permanently fixed “Low” in MPU interface mode only.* note1,2
  -------

  
  // p28 is the best. at showing the levels of everything
  // note also that reading, involves a write from host first to select what to read.
*/

static void sendCommand(uint8_t command, const uint8_t *dataBytes, uint8_t numDataBytes)
{
  gpio_clear(LCD_PORT, LCD_CS);   // assert chip select, check.
  
  gpio_clear(LCD_PORT, LCD_RS);   // low - to assert register, D/CX  p24
  send8(command);

  gpio_set(LCD_PORT, LCD_RS);     // high - to assert data
  for(unsigned i = 0; i < numDataBytes; ++i) {
    send8(dataBytes[ i ]);
  }

}


static void sendCommand0(uint8_t command)
{
  gpio_clear(LCD_PORT, LCD_CS);   // assert chip select, check.
  
  gpio_clear(LCD_PORT, LCD_RS);   // low - to assert register, D/CX  p24
  send8(command);
}



// https://github.com/juj/fbcp-ili9341/blob/master/ili9341.cpp  <- explains more

// static const uint8_t PROGMEM initcmd[] = {
static uint8_t initcmd[] = {
  0xEF, 3, 0x03, 0x80, 0x02,
  0xCF, 3, 0x00, 0xC1, 0x30,
  0xED, 4, 0x64, 0x03, 0x12, 0x81,
  0xE8, 3, 0x85, 0x00, 0x78,
  0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
  0xF7, 1, 0x20,
  0xEA, 2, 0x00, 0x00,
  ILI9341_PWCTR1  , 1, 0x23,             // Power control VRH[5:0]
  ILI9341_PWCTR2  , 1, 0x10,             // Power control SAP[2:0];BT[3:0]
  ILI9341_VMCTR1  , 2, 0x3e, 0x28,       // VCM control
  ILI9341_VMCTR2  , 1, 0x86,             // VCM control2
  ILI9341_MADCTL  , 1, 0x48,             // Memory Access Control
  ILI9341_VSCRSADD, 1, 0x00,             // Vertical scroll zero
  ILI9341_PIXFMT  , 1, 0x55,
  ILI9341_FRMCTR1 , 2, 0x00, 0x18,
  ILI9341_DFUNCTR , 3, 0x08, 0x82, 0x27, // Display Function Control
  0xF2, 1, 0x00,                         // 3Gamma Function Disable
  ILI9341_GAMMASET , 1, 0x01,             // Gamma curve selected
  ILI9341_GMCTRP1 , 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, // Set Gamma
    0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
  ILI9341_GMCTRN1 , 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, // Set Gamma
    0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
  ILI9341_SLPOUT  , 0x80,                // Exit Sleep
  ILI9341_DISPON  , 0x80,                // Display on
  0x00                                   // End of list
};


static uint8_t pgm_read_byte(const uint8_t *addr) { return *addr; } 








static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
#if 0
    // column address set
    ILI9341_WriteCommand(0x2A); // CASET
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // row address set
    ILI9341_WriteCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        ILI9341_WriteData(data, sizeof(data));
    }

    // write to RAM
    ILI9341_WriteCommand(0x2C); // RAMWR
#endif
    {
        uint8_t data[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
        // ILI9341_WriteData(data, sizeof(data));
        sendCommand(ILI9341_CASET, data, sizeof(data) ); // 2A
    }

    // ILI9341_WriteCommand(0x2B); // RASET
    {
        uint8_t data[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
        sendCommand(ILI9341_PASET, data, sizeof(data) ); // 2B
        // ILI9341_WriteData(data, sizeof(data));
    }

    //

    // write to RAM
    // ILI9341_WriteCommand(0x2C); // RAMWR
    // sendCommand(ILI9341_RAMWR, 0 , 0 ); // 2C
}



static void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    // if((x >= ILI9341_WIDTH) || (y >= ILI9341_HEIGHT))
    //  return;
/*

    ILI9341_Select();

    ILI9341_SetAddressWindow(x, y, x+1, y+1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ILI9341_WriteData(data, sizeof(data));

    ILI9341_Unselect();
*/

    ILI9341_SetAddressWindow(x, y, x+50, y+50);
    uint8_t data[] = { color >> 8, color & 0xFF };

    // ILI9341_WriteData(data, sizeof(data));


    sendCommand(ILI9341_RAMWR, data , sizeof(data)  ); // 2C
}


// Good example, https://github.com/afiskon/stm32-ili9341/blob/master/Lib/ili9341/ili9341.c

// stm32 8 bit. https://github.com/nopnop2002/STM32_TFT_8bit/blob/master/STM32_TFT_8bit.cpp

// stm32 very short  https://github.com/afiskon/stm32-ili9341/blob/master/Lib/ili9341/ili9341.c

#define MADCTL_MY 0x80  ///< Bottom to top
#define MADCTL_MX 0x40  ///< Right to left
#define MADCTL_MV 0x20  ///< Reverse Mode
#define MADCTL_ML 0x10  ///< LCD refresh Bottom to top
#define MADCTL_RGB 0x00 ///< Red-Green-Blue pixel order
#define MADCTL_BGR 0x08 ///< Blue-Green-Red pixel order
#define MADCTL_MH 0x04  ///< LCD refresh right to left

/*
  - try 5V instead with 3.3 signals
  - OKK. try to read some data out of the thing. check registers. to know that comms are working.
      - would confirm that can write registers and data correctly. 
  - need uart serial.
  - unhook the other stm32... to get the bus pirate?
  - check data on other side of tranceiver pins. with scope.

  - make sure there is voltage on the 5V rail also. eg. there is analog and digital circuits.
        and may expect 5V.

  - somethingnn to do to flush the buffers, or sync the output, or update?
  ----------
  we really need to check if we are writing regs correctly. before anything more complicated.

  
*/
// or adjust the backlighting.

// or is there someting needed to refresh...

int main(void)
{
  clock_setup();
  led_setup();

  rcc_periph_clock_enable( RCC_GPIOE );
  rcc_periph_clock_enable( RCC_GPIOD );


  gpio_mode_setup(LCD_DATA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, 0xff ); // JA first 8 bits.
  gpio_mode_setup(LCD_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LCD_RST | LCD_CS | LCD_RS | LCD_WR | LCD_RD);


  gpio_clear(LCD_PORT, LCD_RD);   // doesn't like to be high. think that tranceivers block.
                                   // lcd will not blink or do anything... when high. 

  // hardware reset - review
  gpio_set(  LCD_PORT, LCD_RST);    // high
  msleep(150);
  gpio_clear(LCD_PORT, LCD_RST);   // low
  msleep(150);
  gpio_set(  LCD_PORT, LCD_RST);   // high
  msleep(150);

  // not sure that the correct commands and data are being sent...
  // display off, or changing the brightness should have done something.

#if 0
     SPI_TRANSFER(0x01/*Software Reset*/);
    usleep(5*1000);
    SPI_TRANSFER(0x28/*Display OFF*/);
#endif
/*
  sendCommand0(0x01 ); // software reset
  msleep(5);
  sendCommand0(0x28 ); // display off
*/
  // OK. this should have dimmed stuff...
 #if 0 
  {
    uint8_t data[] = { 0x0/*VCOMH=4.250V*/, 0x0/*VCOML=-1.500V*/ };
    sendCommand(0xC5/*VCOM Control 1*/, data, sizeof(data) );   
  }
  {
    uint8_t data[] = { 0x0 /*VCOMH=VMH-58,VCOML=VML-58*/ };
     sendCommand(0xC7/*VCOM Control 2*/, data, sizeof(data) );
    msleep(1000);
  }
  #endif

  // ok running this - changes the screen brightness down a bit. or something
  //  
  uint8_t cmd, x, numArgs;
  const uint8_t *addr = initcmd;
  while ((cmd = pgm_read_byte(addr++)) > 0) {
    x = pgm_read_byte(addr++);
    numArgs = x & 0x7F;
    sendCommand(cmd, addr, numArgs);
    addr += numArgs;
    // Ok, hi bit determines if needs a delay... horrible
    if (x & 0x80)
      delay(150);
  }

/*  
    // EXIT SLEEP
    // ILI9341_WriteCommand(0x11);
    // HAL_Delay(120);
    sendCommand0(0x11);
    msleep( 120);

    // TURN ON DISPLAY
    // ILI9341_WriteCommand(0x29);
    sendCommand0(0x29);
    msleep( 120);
*/

  // normal orientation
  uint8_t m = (MADCTL_MX | MADCTL_BGR);
  sendCommand(ILI9341_MADCTL, &m, 1);


  ILI9341_DrawPixel(50, 50, 0xf777 ); 


  // bool invert = 0;
 	while (1) {

    // gpio_toggle(GPIOD, 1 << 5 );  // blink

    // sendCommand(invert ? ILI9341_INVON : ILI9341_INVOFF, 0 , 0 );
    // sendCommand(invert ? ILI9341_DISPON : ILI9341_DISPOFF , 0 , 0 );
    // invert = ! invert;



    gpio_toggle(GPIOE, GPIO0);  // toggle led
    msleep(300);
	}

  return 0;
}


