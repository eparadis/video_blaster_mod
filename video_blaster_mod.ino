#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

//----------------- VideoBlaster definitions -----------------

#define DOTCLK 0 // Pixel clock (0 for 8MHz, 1 for 4MHz)
#define SYNCPIN 0b10000000 // NOTE edp: orig was "4"  // Pin in PORTD that that is connected for sync
#define CHARS_PER_ROW 39
#define ROWS_PER_COL 25

#define SEND_BLANK_CHARACTER UDR0 = 0; while ((UCSR0A & _BV (UDRE0)) == 0) {}

volatile byte VBE = 0;   // Video blanking status. If this is not zero you should sleep to keep the video smooth

#define WAIT_VBE while (VBE==1) sleep_cpu();

// These should not be used outside the ISR
unsigned int scanline = 0;
unsigned int videoptr = 0;
byte row;

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

const byte MSPIM_SCK = 4;  // This is needed for the hardware to work
const byte MSPIM_SS = 5;   // This is needed for the hardware to work

//--------------------------------------------------------------

// orig 22 cols, 24 rows
#define MAX_VID_RAM (CHARS_PER_ROW*ROWS_PER_COL)
char videomem[MAX_VID_RAM];

// hold what we're currently considering sync on or off, depending on sync inversion
byte syncON, syncOFF;

ISR(TIMER0_COMPA_vect) {   // Video interrupt. This is called at every line in the frame.
  // start sync pulse
  PORTD = syncON;

  // update scanline number
  scanline++;

  // begin inverted (Vertical) sync after line 247
  if (scanline == 248) {
    syncON = 0b10000000;
    syncOFF = 0;
  } else if (scanline == 251) { // back to regular sync after line 250
    syncON = 0;
    syncOFF = 0b10000000;
  } else if (scanline == 263) { // start new frame after line 262
    scanline = 1;
    VBE = 0;
  }

  //adjust to make 5 us pulses
  _delay_us(3);

  //end sync pulse
  PORTD = syncOFF;

  if (scanline == 19 ) {
    videoptr = 0;
    row = 0;
  }

  // scanlines 40 to 231 are image lines
  if ((scanline >= 20) && (scanline < (20+8*ROWS_PER_COL))) {

    const register byte * linePtr = &charROM [ row & 0x07 ] [0];
    register byte * messagePtr = (byte *) & videomem [videoptr] ;

    UCSR0B = _BV(TXEN0);

    // center image on screen
    SEND_BLANK_CHARACTER
    SEND_BLANK_CHARACTER
    SEND_BLANK_CHARACTER
#if DOTCLK == 0
    SEND_BLANK_CHARACTER
#endif

    // send a line of data
    byte chars_per_row = CHARS_PER_ROW;
    while (chars_per_row --) {
      
      while ((UCSR0A & _BV (UDRE0)) == 0)
      {}
      UDR0 = ~(pgm_read_byte (linePtr + (* messagePtr++)));
    }
    while ((UCSR0A & _BV (UDRE0)) == 0)
    {}
    UCSR0B = 0;

    // update videoptr to the start of the next line of characters
    row++;
    videoptr = (row >> 3) * CHARS_PER_ROW;

    // signal that we're drawing the image, not in vertical blank
    VBE = 1;
  }
}

void setup () {
  pinMode (MSPIM_SS, OUTPUT);
  pinMode(7, OUTPUT); // video sync output
  UBRR0 = 0;
  UCSR0A = _BV (TXC0);
  pinMode (MSPIM_SCK, OUTPUT);
  UCSR0C = _BV (UMSEL00) | _BV (UMSEL01);
  UCSR0B = _BV (TXEN0);
  UBRR0 = DOTCLK;

  // setup horizontal sync ISR
  cli();
  TCCR0A = 0;
  TCCR0B = 0;
  TCNT0  = 0;
  // "In analog television systems the horizontal frequency is between 15.625 kHz and 15.750 kHz."
  // "...each line takes 63.5us to paint (15.75kHz)"
  // OCR0A = 127; // 15.625 khz
  OCR0A = 126; // 15.748 khz
  // OCR0A = 125; // 15.873 khz
  TCCR0A |= (1 << WGM01);
  TCCR0B |= (0 << CS02) | (1 << CS01) | (0 << CS00);
  TIMSK0 |= (1 << OCIE0A);
  set_sleep_mode (SLEEP_MODE_IDLE);
  sei();

  // the built-in LED
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  // TXD aka D1 (Arduino) aka PD1 (Atmel)
  pinMode(1, OUTPUT);   //A must for MSMSPI VIDEO to work
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
