#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

//----------------- VideoBlaster definitions -----------------

#define DOTCLK 1 // NOTE edp: orig was 1   // Pixel clock (0 for 8MHz, 1 for 4MHz)
#define HSYNC 124 // 122? NOTE edp: original value 132  // Hsync frequency (divided from Fcpu)
#define LINES 261  // Lines per field -1 (261 for NTSC, 311 for PAL)
#define SYNCPIN 0b10000000 // NOTE edp: orig was "4"  // Pin in PORTD that that is connected for sync
#define INTERLACE 1 // 0 for interlace, 1 for non interlace. Running with interlaced video gives more cycles to the application
#define CHARS_PER_ROW 20

volatile byte VBE = 0;   // Video blanking status. If this is not zero you should sleep to keep the video smooth

#define WAIT_VBE while (VBE==1) sleep_cpu();

unsigned int scanline = 0; // Dont touch, not volatile
unsigned int videoptr = 0; // Dont touch, not volatile
byte row;                // Dont touch, not volatile
byte lace;               // Dont touch, not volatile

// This is the video character ROM (8x8 font definition)
const unsigned char charROM [8] [128] PROGMEM = { 0x1C , 0x18 , 0x7C , 0x1C , 0x78 , 0x7E , 0x7E , 0x1C , 0x42 , 0x1C , 0xE ,
                                                  0x42 , 0x40 , 0x42 , 0x42 , 0x18 , 0x7C , 0x18 , 0x7C , 0x3C , 0x3E , 0x42 , 0x42 , 0x42 , 0x42 , 0x22 , 0x7E , 0x42 , 0x42 ,
                                                  0x18 , 0x0 , 0x0 , 0x0 , 0x8 , 0x24 , 0x24 , 0x8 , 0x0 , 0x30 , 0x4 , 0x4 , 0x20 , 0x8 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x3C ,
                                                  0x8 , 0x3C , 0x3C , 0x4 , 0x7E , 0x1C , 0x7E , 0x3C , 0x3C , 0x0 , 0x0 , 0xE , 0x0 , 0x70 , 0x3C , 0x0 , 0x8 , 0x10 , 0x0 ,
                                                  0x0 , 0x0 , 0x0 , 0x20 , 0x4 , 0x0 , 0x8 , 0x8 , 0x80 , 0x80 , 0x1 , 0xFF , 0xFF , 0x0 , 0x0 , 0x36 , 0x40 , 0x0 , 0x81 ,
                                                  0x0 , 0x8 , 0x2 , 0x8 , 0x8 , 0xA0 , 0x8 , 0x0 , 0xFF , 0x0 , 0xF0 , 0x0 , 0xFF , 0x0 , 0x80 , 0xAA , 0x1 , 0x0 , 0xFF ,
                                                  0x3 , 0x8 , 0x0 , 0x8 , 0x0 , 0x0 , 0x0 , 0x8 , 0x0 , 0x8 , 0xC0 , 0xE0 , 0x7 , 0xFF , 0xFF , 0x0 , 0x1 , 0x0 , 0xF , 0x8 ,
                                                  0xF0 , 0xF0 , 0x22 , 0x24 , 0x22 , 0x22 , 0x24 , 0x40 , 0x40 , 0x22 , 0x42 , 0x8 , 0x4 , 0x44 , 0x40 , 0x66 , 0x62 , 0x24 ,
                                                  0x42 , 0x24 , 0x42 , 0x42 , 0x8 , 0x42 , 0x42 , 0x42 , 0x42 , 0x22 , 0x2 , 0x18 , 0x18 , 0x24 , 0x8 , 0x0 , 0x0 , 0x8 , 0x24 ,
                                                  0x24 , 0x1E , 0x62 , 0x48 , 0x8 , 0x8 , 0x10 , 0x2A , 0x8 , 0x0 , 0x0 , 0x0 , 0x2 , 0x42 , 0x18 , 0x42 , 0x42 , 0xC , 0x40 ,
                                                  0x20 , 0x42 , 0x42 , 0x42 , 0x0 , 0x0 , 0x18 , 0x0 , 0x18 , 0x42 , 0x0 , 0x1C , 0x10 , 0x0 , 0x0 , 0xFF , 0x0 , 0x20 , 0x4 ,
                                                  0x0 , 0x8 , 0x8 , 0x80 , 0x40 , 0x2 , 0x80 , 0x1 , 0x3C , 0x0 , 0x7F , 0x40 , 0x0 , 0x42 , 0x3C , 0x1C , 0x2 , 0x1C , 0x8 ,
                                                  0x50 , 0x8 , 0x0 , 0x7F , 0x0 , 0xF0 , 0x0 , 0x0 , 0x0 , 0x80 , 0x55 , 0x1 , 0x0 , 0xFE , 0x3 , 0x8 , 0x0 , 0x8 , 0x0 , 0x0 ,
                                                  0x0 , 0x8 , 0x0 , 0x8 , 0xC0 , 0xE0 , 0x7 , 0xFF , 0xFF , 0x0 , 0x1 , 0x0 , 0xF , 0x8 , 0xF0 , 0xF0 , 0x4A , 0x42 , 0x22 ,
                                                  0x40 , 0x22 , 0x40 , 0x40 , 0x40 , 0x42 , 0x8 , 0x4 , 0x48 , 0x40 , 0x5A , 0x52 , 0x42 , 0x42 , 0x42 , 0x42 , 0x40 , 0x8 ,
                                                  0x42 , 0x42 , 0x42 , 0x24 , 0x22 , 0x4 , 0x24 , 0x24 , 0x3C , 0x1C , 0x10 , 0x0 , 0x8 , 0x24 , 0x7E , 0x28 , 0x64 , 0x48 ,
                                                  0x10 , 0x10 , 0x8 , 0x1C , 0x8 , 0x0 , 0x0 , 0x0 , 0x4 , 0x46 , 0x28 , 0x2 , 0x2 , 0x14 , 0x78 , 0x40 , 0x4 , 0x42 , 0x42 ,
                                                  0x8 , 0x8 , 0x30 , 0x7E , 0xC , 0x2 , 0x0 , 0x3E , 0x10 , 0x0 , 0xFF , 0x0 , 0x0 , 0x20 , 0x4 , 0x0 , 0x8 , 0x8 , 0x80 , 0x20 ,
                                                  0x4 , 0x80 , 0x1 , 0x7E , 0x0 , 0x7F , 0x40 , 0x0 , 0x24 , 0x42 , 0x2A , 0x2 , 0x3E , 0x8 , 0xA0 , 0x8 , 0x1 , 0x3F , 0x0 ,
                                                  0xF0 , 0x0 , 0x0 , 0x0 , 0x80 , 0xAA , 0x1 , 0x0 , 0xFC , 0x3 , 0x8 , 0x0 , 0x8 , 0x0 , 0x0 , 0x0 , 0x8 , 0x0 , 0x8 , 0xC0 ,
                                                  0xE0 , 0x7 , 0x0 , 0xFF , 0x0 , 0x1 , 0x0 , 0xF , 0x8 , 0xF0 , 0xF0 , 0x56 , 0x7E , 0x3C , 0x40 , 0x22 , 0x78 , 0x78 , 0x4E ,
                                                  0x7E , 0x8 , 0x4 , 0x70 , 0x40 , 0x5A , 0x4A , 0x42 , 0x7C , 0x42 , 0x7C , 0x3C , 0x8 , 0x42 , 0x24 , 0x5A , 0x18 , 0x1C ,
                                                  0x18 , 0x42 , 0x42 , 0x42 , 0x2A , 0x20 , 0x0 , 0x8 , 0x0 , 0x24 , 0x1C , 0x8 , 0x30 , 0x0 , 0x10 , 0x8 , 0x3E , 0x3E , 0x0 ,
                                                  0x7E , 0x0 , 0x8 , 0x5A , 0x8 , 0xC , 0x1C , 0x24 , 0x4 , 0x7C , 0x8 , 0x3C , 0x3E , 0x0 , 0x0 , 0x60 , 0x0 , 0x6 , 0xC , 0x0 ,
                                                  0x7F , 0x10 , 0xFF , 0x0 , 0x0 , 0x0 , 0x20 , 0x4 , 0x0 , 0x4 , 0x10 , 0x80 , 0x10 , 0x8 , 0x80 , 0x1 , 0x7E , 0x0 , 0x7F ,
                                                  0x40 , 0x0 , 0x18 , 0x42 , 0x77 , 0x2 , 0x7F , 0x8 , 0x50 , 0x8 , 0x3E , 0x1F , 0x0 , 0xF0 , 0x0 , 0x0 , 0x0 , 0x80 , 0x55 ,
                                                  0x1 , 0x0 , 0xF8 , 0x3 , 0x8 , 0x0 , 0x8 , 0x0 , 0x0 , 0x0 , 0x8 , 0x0 , 0x8 , 0xC0 , 0xE0 , 0x7 , 0x0 , 0x0 , 0x0 , 0x1 ,
                                                  0x0 , 0xF , 0x8 , 0xF0 , 0xF0 , 0x4C , 0x42 , 0x22 , 0x40 , 0x22 , 0x40 , 0x40 , 0x42 , 0x42 , 0x8 , 0x4 , 0x48 , 0x40 ,
                                                  0x42 , 0x46 , 0x42 , 0x40 , 0x4A , 0x48 , 0x2 , 0x8 , 0x42 , 0x24 , 0x5A , 0x24 , 0x8 , 0x20 , 0x7E , 0x42 , 0x7E , 0x8 ,
                                                  0x7F , 0x0 , 0x0 , 0x0 , 0x7E , 0xA , 0x10 , 0x4A , 0x0 , 0x10 , 0x8 , 0x1C , 0x8 , 0x0 , 0x0 , 0x0 , 0x10 , 0x62 , 0x8 ,
                                                  0x30 , 0x2 , 0x7E , 0x2 , 0x42 , 0x10 , 0x42 , 0x2 , 0x0 , 0x0 , 0x30 , 0x7E , 0xC , 0x10 , 0xFF , 0x7F , 0x10 , 0x0 , 0x0 ,
                                                  0x0 , 0x0 , 0x20 , 0x4 , 0xE0 , 0x3 , 0xE0 , 0x80 , 0x8 , 0x10 , 0x80 , 0x1 , 0x7E , 0x0 , 0x3E , 0x40 , 0x3 , 0x18 , 0x42 ,
                                                  0x2A , 0x2 , 0x3E , 0xFF , 0xA0 , 0x8 , 0x54 , 0xF , 0x0 , 0xF0 , 0xFF , 0x0 , 0x0 , 0x80 , 0xAA , 0x1 , 0xAA , 0xF0 , 0x3 ,
                                                  0xF , 0xF , 0xF , 0xF8 , 0x0 , 0xF , 0xFF , 0xFF , 0xF8 , 0xC0 , 0xE0 , 0x7 , 0x0 , 0x0 , 0x0 , 0x1 , 0xF0 , 0x0 , 0xF8 , 0x0 ,
                                                  0xF , 0x20 , 0x42 , 0x22 , 0x22 , 0x24 , 0x40 , 0x40 , 0x22 , 0x42 , 0x8 , 0x44 , 0x44 , 0x40 , 0x42 , 0x42 , 0x24 , 0x40 ,
                                                  0x24 , 0x44 , 0x42 , 0x8 , 0x42 , 0x18 , 0x66 , 0x42 , 0x8 , 0x40 , 0x42 , 0x24 , 0x42 , 0x8 , 0x20 , 0x0 , 0x0 , 0x0 , 0x24 ,
                                                  0x3C , 0x26 , 0x44 , 0x0 , 0x8 , 0x10 , 0x2A , 0x8 , 0x8 , 0x0 , 0x18 , 0x20 , 0x42 , 0x8 , 0x40 , 0x42 , 0x4 , 0x44 , 0x42 ,
                                                  0x10 , 0x42 , 0x4 , 0x8 , 0x8 , 0x18 , 0x0 , 0x18 , 0x0 , 0x0 , 0x1C , 0x10 , 0x0 , 0x0 , 0x0 , 0xFF , 0x20 , 0x4 , 0x10 ,
                                                  0x0 , 0x0 , 0x80 , 0x4 , 0x20 , 0x80 , 0x1 , 0x7E , 0x0 , 0x1C , 0x40 , 0x4 , 0x24 , 0x42 , 0x8 , 0x2 , 0x1C , 0x8 , 0x50 ,
                                                  0x8 , 0x14 , 0x7 , 0x0 , 0xF0 , 0xFF , 0x0 , 0x0 , 0x80 , 0x55 , 0x1 , 0x55 , 0xE0 , 0x3 , 0x8 , 0xF , 0x0 , 0x8 , 0x0 , 0x8 ,
                                                  0x0 , 0x8 , 0x8 , 0xC0 , 0xE0 , 0x7 , 0x0 , 0x0 , 0xFF , 0x1 , 0xF0 , 0x0 , 0x0 , 0x0 , 0xF , 0x1E , 0x42 , 0x7C , 0x1C ,
                                                  0x78 , 0x7E , 0x40 , 0x1C , 0x42 , 0x1C , 0x38 , 0x42 , 0x7E , 0x42 , 0x42 , 0x18 , 0x40 , 0x1A , 0x42 , 0x3C , 0x8 , 0x3C ,
                                                  0x18 , 0x42 , 0x42 , 0x8 , 0x7E , 0x42 , 0x18 , 0x42 , 0x8 , 0x10 , 0x0 , 0x8 , 0x0 , 0x24 , 0x8 , 0x46 , 0x3A , 0x0 , 0x4 ,
                                                  0x20 , 0x8 , 0x0 , 0x8 , 0x0 , 0x18 , 0x40 , 0x3C , 0x3E , 0x7E , 0x3C , 0x4 , 0x38 , 0x3C , 0x10 , 0x3C , 0x38 , 0x0 , 0x8 ,
                                                  0xE , 0x0 , 0x70 , 0x10 , 0x0 , 0x3E , 0x10 , 0x0 , 0x0 , 0x0 , 0x0 , 0x20 , 0x4 , 0x8 , 0x0 , 0x0 , 0x80 , 0x2 , 0x40 , 0x80 ,
                                                  0x1 , 0x3C , 0xFF , 0x8 , 0x40 , 0x8 , 0x42 , 0x3C , 0x8 , 0x2 , 0x8 , 0x8 , 0xA0 , 0x8 , 0x14 , 0x3 , 0x0 , 0xF0 , 0xFF , 0x0 ,
                                                  0x0 , 0x80 , 0xAA , 0x1 , 0xAA , 0xC0 , 0x3 , 0x8 , 0xF , 0x0 , 0x8 , 0xFF , 0x8 , 0x0 , 0x8 , 0x8 , 0xC0 , 0xE0 , 0x7 , 0x0 ,
                                                  0x0 , 0xFF , 0x1 , 0xF0 , 0x0 , 0x0 , 0x0 , 0xF , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 ,
                                                  0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x8 , 0x0 , 0x0 , 0x0 ,
                                                  0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x10 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 ,
                                                  0x0 , 0x0 , 0x0 , 0x0 , 0x10 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x0 , 0x10 , 0x0 , 0x0 , 0x0 , 0x0 , 0x20 , 0x4 , 0x8 , 0x0 , 0x0 ,
                                                  0xFF , 0x1 , 0x80 , 0x80 , 0x1 , 0x0 , 0x0 , 0x0 , 0x40 , 0x8 , 0x81 , 0x0 , 0x0 , 0x2 , 0x0 , 0x8 , 0x50 , 0x8 , 0x0 , 0x1 ,
                                                  0x0 , 0xF0 , 0xFF , 0x0 , 0xFF , 0x80 , 0x55 , 0x1 , 0x55 , 0x80 , 0x3 , 0x8 , 0xF , 0x0 , 0x8 , 0xFF , 0x8 , 0x0 , 0x8 , 0x8 ,
                                                  0xC0 , 0xE0 , 0x7 , 0x0 , 0x0 , 0xFF , 0xFF , 0xF0 , 0x0 , 0x0 , 0x0 , 0xF
                                                };


// "At 16 mhz, each NOP takes 62.5 NANOseconds."
// 64 NOPs = 4.0 uS
// "In the 525-line NTSC system it is a 4.85 µs-long pulse at 0 V."
// 4.85uS ~= 77.6 NOPs
#define SYNCDELAY   // This makes up the sync pulse low time delay
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /*  8 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 16 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 24 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 32 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 40 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 48 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 56 */ \
asm("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); /* 64 */

const byte MSPIM_SCK = 4;  // This is needed for the hardware to work
const byte MSPIM_SS = 5;   // This is needed for the hardware to work

//--------------------------------------------------------------

#define MAX_VID_RAM 528
char videomem[MAX_VID_RAM];
//char videomem[528]; // orig 22 cols, 24 rows

#define TOPLINES 3
#define MIDLINES 40

ISR(TIMER0_COMPA_vect) {   //Video interrupt. This is called at every line in the frame.
  byte back_porch = 8;  // Back porch (original value: 8)
  byte left_blank = 3;  // Left Blank (original value: 4)
  byte chars_per_row = CHARS_PER_ROW; //Chars per row

  // sync pin low
  PORTD = 0;

  // scanlines 3 to 39  and  232 to LINES have sync pin high
  // these are also the virtical blank period
  if ((scanline >= TOPLINES) && (scanline < MIDLINES) || (scanline > 231)) {
    SYNCDELAY
    PORTD = SYNCPIN;
    VBE = 0;
  }

  // scanlines 0, 1 and 2 have no image, and sync pin low
  if (scanline < TOPLINES) {
    SYNCDELAY
    PORTD = 0;
    videoptr = 0;
    row = 0;
  }

  // scanlines 40 to 231 are image lines
  if ((scanline >= MIDLINES) && (scanline <= 231)) {
    SYNCDELAY
    PORTD = SYNCPIN;
    if ( lace & 1 | INTERLACE) {
      const register byte * linePtr = &charROM [ row & 0x07 ] [0];
      register byte * messagePtr = (byte *) & videomem [videoptr] ;
      while (back_porch--) {
        asm("nop\n");
        asm("nop\n");
      }
      UCSR0B = _BV(TXEN0);
      while (left_blank--) {
        while ((UCSR0A & _BV (UDRE0)) == 0) {}
        UDR0 = 0;
      }
      while (chars_per_row --) {
        UDR0 = pgm_read_byte (linePtr + (* messagePtr++));
        while ((UCSR0A & _BV (UDRE0)) == 0)
        {}
      }
      while ((UCSR0A & _BV (UDRE0)) == 0)
      {}
      UCSR0B = 0;
      row++;
      // NOTE edp: pretty sure the 22 is the chars_per_row,
      //           but "videoptr=(row>>3)*CHARS_PER_ROW;" didn't look right
      videoptr = (row >> 3) * CHARS_PER_ROW;
      VBE = 1;
    }
  }
  scanline++;
  if (scanline > LINES) {
    scanline = 0;
    lace++;
  }
}

void setup () {
  pinMode (MSPIM_SS, OUTPUT);  //A must for MSMSPI VIDEO to work
  pinMode (MSPIM_SCK, OUTPUT); //A must for MSMSPI VIDEO to work
  pinMode(2, OUTPUT); //Set D2 as output for Sync. A must for MSMSPI VIDEO to work
  // NOTE edp: what the heck, try every pin on port D
  pinMode(3, OUTPUT); //NOTE edp: why does arduino Pin D4 seem to have the right signal?
  // there is totally some sort of semi-sync'd signal on arduino pin 4, aka AVR PD4 aka T0
  pinMode(4, OUTPUT); //NOTE edp: why does arduino Pin D4 seem to have the right signal?
  pinMode(5, OUTPUT); //NOTE edp: why does arduino Pin D4 seem to have the right signal?
  pinMode(6, OUTPUT); //NOTE edp: why does arduino Pin D4 seem to have the right signal?
  pinMode(7, OUTPUT); //NOTE edp: why does arduino Pin D4 seem to have the right signal?
  UBRR0 = 0;
  UCSR0A = _BV (TXC0);
  UCSR0C = _BV (UMSEL00) | _BV (UMSEL01);
  UCSR0B = _BV (TXEN0);
  UBRR0 = DOTCLK;
  cli();
  TCCR0A = 0;
  TCCR0B = 0;
  TCNT0  = 0;
  // "In analog television systems the horizontal frequency is between 15.625 kHz and 15.750 kHz."
  OCR0A = HSYNC;// NOTE edp: did he mean 16*1e6 ??? orig: // = (16*10^6) / (15625*8) - 1 (must be <256)
  TCCR0A |= (1 << WGM01);
  TCCR0B |= (1 << CS01) | (0 << CS00);
  TIMSK0 |= (1 << OCIE0A);
  set_sleep_mode (SLEEP_MODE_IDLE);
  sei();

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  pinMode(1, OUTPUT); //A must for MSMSPI VIDEO to work
  digitalWrite(1, LOW); //A must for MSMSPI VIDEO to work

  for (int i = 0; i < MAX_VID_RAM; i++) {
    //    videomem[i] = 126; // graphical character of half height square
    //    videomem[i] = (i&1?0xF0:0x0F); // alternate two specific characters
    videomem[i] = 48 + (i % 10); // count through the digits
    //    videomem[i] = i%128; // cycle through every character
    //    videomem[i] = 31; // fill with a single character. 31 is a left horizontal arrow
    //    videomem[i] = (i%26); // the first 26 characters are A-Z (nonascii)
    //    videomem[i] = 32; // fill with blanks, 32 = ' ' in this charset
  }
}

void loop () {
  WAIT_VBE

  // do stuff here during the VBlank

}
