#include <Arduino.h>
#include <SPI.h>
#include <RA8875.h>
#include "tinyflash.h"
#include <EEPROM.h>

// externs
void kbd_setup();
uint16_t process_key(bool wait);

//#define TEST
#define LED 13

// defines on command line
//#define USETOUCH
//#define KEYBOARD
//#define DEBUG

#define FW_SOURCE_LEN 5478

#define RA8875_CS 10
#define RA8875_RESET 255 //any pin, if you want to disable just set at 255 or not use at all
//RA8875(CSp,RSTp,mosi_pin,sclk_pin,miso_pin);
RA8875 tft = RA8875(RA8875_CS, RA8875_RESET, 11, 14, 12);
/* Teensy LC
    No reset
    CS   10
    mosi 11
    miso 12
    sclk 14 (alt)
*/

/* Uses a W25Q80BV flash to store the touch screen firmware (as not enough in teensy)
    On SPI1 as cannot share with TFT
    CS   6
    miso 1
    mosi 2
    sclk 20

    #define FLASHFW to flash the firmware
    #define VERIFYFW to check it
    Make sure both are undefined to run program
*/
#ifdef USETOUCH
TinyFlash flash;
uint32_t capacity = 0;
#endif

bool has_touch = false;
uint16_t screen_width, screen_height;
uint16_t char_width, char_height;

#ifdef USETOUCH
// externals
void gsl_final_setup();
void gsl_setup();
void gsl_load_fw(uint8_t addr, uint8_t Wrbuf[4]);

void load_touch_fw()
{
    gsl_setup();

#ifdef DEBUG
    Serial.println("Loading Touch FIRMWARE");
#endif

    tft.println("Loading Touch FIRMWARE...");
    if(!flash.beginRead(0)) {
#ifdef DEBUG
        Serial.println("beginread failed");
#endif
        tft.println("flash read failed");
        digitalWrite(LED, 0);
        return;
    }

    uint16_t source_len = FW_SOURCE_LEN;

    for (uint32_t source_line = 0; source_line < source_len; source_line++) {
        uint8_t addr = flash.readNextByte();
        flash.readNextByte();
        flash.readNextByte();
        flash.readNextByte();

        uint8_t buf[4];
        buf[0] = flash.readNextByte();
        buf[1] = flash.readNextByte();
        buf[2] = flash.readNextByte();
        buf[3] = flash.readNextByte();
        gsl_load_fw(addr, buf);
    }
    flash.endRead();
#ifdef DEBUG
    Serial.println("Touch Firmware loaded ok");
#endif
    tft.println("...Loaded Touch FIRMWARE");
    gsl_final_setup();
}
#endif

// these are to be configurable
int baudrate = 9600;
int rotation = 0; // 1 is portrait
int font_size = 0;
uint16_t text_color = RA8875_GREEN;
bool local_echo = true;
bool lfcrlf = true; // convert lf to crlf
bool crcrlf = true; // convert cr to crlf

void clear_screen()
{
    tft.fillWindow(RA8875_BLACK);
    tft.setCursor(0, 0);
}

void save_settings()
{
    EEPROM.write(0, 0xA5);
    uint8_t br;
    switch(baudrate) {
        case 1200:   br= 0; break;
        case 2400:   br= 1; break;
        case 4800:   br= 2; break;
        case 9600:   br= 3; break;
        case 19200:  br= 4; break;
        case 115200: br= 5; break;
        default:     br= 3; break;
    }
    EEPROM.write(1, br);
    EEPROM.write(2, rotation);
    EEPROM.write(3, font_size);
    EEPROM.write(4, text_color & 0xFF);
    EEPROM.write(5, text_color >> 8);
    EEPROM.write(6, local_echo);
    EEPROM.write(7, lfcrlf);
    EEPROM.write(8, crcrlf);
}

void get_settings()
{
    // read config settings stored in eeprom
    // first time settings are set will write the defaults and any changes
    uint8_t value = EEPROM.read(0);
    if(value == 0xA5) {
        // configs have been stored
        value = EEPROM.read(1);
        switch(value) {
            case 0: baudrate = 1200; break;
            case 1: baudrate = 2400; break;
            case 2: baudrate = 4800; break;
            case 3: baudrate = 9600; break;
            case 4: baudrate = 19200; break;
            case 5: baudrate = 115200; break;
            default: baudrate = 9600; break;
        }
        rotation = EEPROM.read(2);
        font_size = EEPROM.read(3);
        text_color = (EEPROM.read(5) << 8) | EEPROM.read(4);
        local_echo = EEPROM.read(6) != 0;
        lfcrlf = EEPROM.read(7) != 0;
        crcrlf = EEPROM.read(8) != 0;
    }
#ifdef DEBUG
    else {
        Serial.println("No settings found in EEPROM");
    }
#endif
}

// command line based setup for now
void config_setup()
{
    clear_screen();

    bool done= false;
    while(!done) {
        tft.println("Enter the values to change,\r\nspace skips to next,\r\nq quits");
        char k;

        tft.print("font size (0,1,2,3) > ");
        k= process_key(true) & 0xFF; if(k == 'q') break;
        if(k >= '0' && k <= '3') {
            font_size= k - '0';
            tft.setFontScale(font_size);
            char_width = tft.getFontWidth();
            char_height = tft.getFontHeight();
            clear_screen();
        }
        tft.println();

        tft.print("rotation (0,1) > ");
        k= process_key(true) & 0xFF; if(k == 'q') break;
        if(k >= '0' && k <= '1') {
            rotation= k - '0';
            tft.setRotation(rotation);
            screen_width = tft.width();
            screen_height = tft.height();
            char_width = tft.getFontWidth();
            char_height = tft.getFontHeight();
            clear_screen();
        }
        tft.println();

        tft.print("local Echo (0,1) > ");
        k= process_key(true) & 0xFF; if(k == 'q') break;
        if(k >= '0' && k <= '1') {
            local_echo= (k == '1');
        }
        tft.println();

        tft.print("Save (y/n/r) > ");
        k= process_key(true) & 0xFF; if(k == 'q') break;
        if(k == 'r') {
            EEPROM.write(0, 0);
            tft.println("\r\nSettings restored");
        } else if(k == 'y') {
            save_settings();
            tft.println("\r\nSettings saved");
        }

        done= true;
    }

    tft.println("\r\nDone");
}

void setup()
{
#ifdef DEBUG
    Serial.begin (115200);   // debugging
    // wait up to 5 seconds for Arduino Serial Monitor
    unsigned long startMillis = millis();
    while (!Serial && (millis() - startMillis < 5000)) ;
    Serial.println("Starting Terminal.");
#endif

    get_settings();

    Serial1.setRX(3);
    Serial1.setTX(4);
    Serial1.begin(baudrate, SERIAL_8N1); // for I/O
    // Serial1.println("Hello world!");

    tft.begin(RA8875_800x480);
    tft.setRotation(rotation);

    tft.setFontScale(font_size); //font x1


    screen_width = tft.width();
    screen_height = tft.height();
    char_width = tft.getFontWidth();
    char_height = tft.getFontHeight();

#ifdef DEBUG
    Serial.printf("Screen width: %u, height: %u\n", screen_width, screen_height);
    Serial.printf("Font width: %u, height: %u\n", char_width, char_height);
    Serial.printf("Line lengths: %u x %u\n", screen_width / char_width, screen_height / char_height);
#endif

    tft.printf("Screen width: %u, height: %u\n", screen_width, screen_height);
    tft.printf("Font width: %u, height: %u\n", char_width, char_height);
    tft.printf("Line lengths: %u x %u\n", screen_width / char_width, screen_height / char_height);

    tft.showCursor(UNDER, true);

    //now set a text color, background transparent
    tft.println("Starting up...");

#ifdef KEYBOARD
    // setup the micro keyboard
    kbd_setup();
#endif

    pinMode(LED, OUTPUT);
    digitalWrite(LED, 0);

#ifdef USETOUCH
    capacity = flash.begin();
#ifdef DEBUG
    Serial.println(capacity);     // Chip size to host
#endif
    if(capacity > 0) {
        digitalWrite(LED, 1);
        load_touch_fw();
        has_touch = true;

    } else {
#ifdef DEBUG
        Serial.println("Bad flash");
#endif
        tft.println("Unable to setup Flash");
        digitalWrite(LED, 0);
        has_touch = false;
    }
#endif

    // set text color to green
    tft.setTextColor(text_color);
    tft.setCursor(0, 0);
    tft.fillWindow(RA8875_BLACK); //fill window black
    tft.sleep(false);
    tft.displayOn(true);

#if 0
    tft.println("Once upon a midnight dreary, while I pondered, weak and weary,");
    tft.println("Over many a quaint and curious volume of forgotten lore,");
    tft.println("While I nodded, nearly napping, suddenly there came a tapping,");
    tft.println("As of some one gently rapping, rapping at my chamber door.");
    tft.println("'Tis some visitor,' I muttered, 'tapping at my chamber door Only this, and nothing more.'");
    tft.println("");
    tft.println("Ah, distinctly I remember it was in the bleak December,");
    tft.println("And each separate dying ember wrought its ghost upon the floor.");
    tft.println("Eagerly I wished the morrow;- vainly I had sought to borrow");
    tft.println("From my books surcease of sorrow- sorrow for the lost Lenore-");
    tft.println("For the rare and radiant maiden whom the angels name Lenore-");
    tft.println("Nameless here for evermore.");
    tft.println("");
    tft.println("Once upon a midnight dreary, while I pondered, weak and weary,");
    tft.println("Over many a quaint and curious volume of forgotten lore,");
    tft.println("While I nodded, nearly napping, suddenly there came a tapping,");
    tft.println("As of some one gently rapping, rapping at my chamber door.");
    tft.println("'Tis some visitor,' I muttered, 'tapping at my chamber door Only this, and nothing more.'");
    tft.println("");
    tft.println("Ah, distinctly I remember it was in the bleak December,");
    tft.println("And each separate dying ember wrought its ghost upon the floor.");
    tft.println("Eagerly I wished the morrow;- vainly I had sought to borrow");
    tft.println("From my books surcease of sorrow- sorrow for the lost Lenore-");
    tft.println("For the rare and radiant maiden whom the angels name Lenore-");
    tft.println("Nameless here for evermore.");
#endif
}

#ifdef USETOUCH
struct _coord {
    uint32_t x, y;
    uint8_t finger;
};

struct _ts_event {
    uint8_t  n_fingers;
    struct _coord coords[5];
};

#include "RingBuffer.h"
using touch_event_t = struct _ts_event;
template<class T> using touch_events_t = RingBuffer<T, 16>;
touch_events_t<touch_event_t> touch_events;
enum touch_state_t { UP, DOWN };
touch_state_t touch_state = UP;
bool moved = false;

extern "C" void add_touch_event(struct _ts_event *e)
{
    touch_events.push_back(*e);
}

int tog = 0;
uint32_t fc = 0, time = 0;
int max_depth = 0;
uint32_t lastx, lasty;
struct _coord last_coords[5] = {0};
int nf = 0;
bool last_finger_down[5] = {false};
#endif

// wait for char and return it
char getcharw()
{
#ifdef DEBUG
    while(true) {
        if (Serial1.available()) {
            return Serial1.read();
        }
        if (Serial.available()) {
            return Serial.read();
        }
    }
#else
    while (!Serial1.available()) ;
    return Serial1.read();
#endif
}

void scroll_up()
{
    tft.BTE_move(0, char_height, screen_width, screen_height - char_height, 0, 0);
    delay(50);
    tft.fillRect(0, screen_height - char_height, screen_width, char_height, RA8875_BLACK);
}

void scroll_down()
{
    tft.BTE_move(screen_width - 1, screen_height - char_height - 1, screen_width, screen_height - char_height, screen_width - 1, screen_height - 1, 0, 0, false, RA8875_BTEROP_SOURCE, false, true);
    delay(50);
    tft.fillRect(0, 0, screen_width, char_height, RA8875_BLACK);
}

void move_cursor(char dir, int n)
{
    int16_t currentX, currentY;
    tft.getCursor(currentX, currentY);
    int16_t x = currentX, y = currentY;
    switch(dir) {
        case 'A':
            // moves cursor up n lines
            y = currentY - (n * char_height);
            if(y < 0) y = 0;
            break;

        case 'B':
            // moves cursor down n lines
            y = currentY + (n * char_height);
            if(y >= screen_height) y = screen_height - char_height;
            break;

        case 'C':
            // cursor right n characters
            x = currentX + (n * char_width);
            if(x >= screen_width) x = screen_width - char_width;
            break;

        case 'D':
            // moves cursor left n characters
            x = currentX - (n * char_width);
            if(x < 0) x = 0;
            break;

        case 'E':
            // moves cursor to start of n next lines
            x = 0;
            y = currentY + (n * char_height);
            if(y >= screen_height) y = screen_height - char_height;
            break;

        case 'F':
            // moves cursor to start of n previous lines
            x = 0;
            y = currentY - (n * char_height);
            if(y < 0) y = 0;
            break;

        case 'G':
            // moves cursor to column n
            --n;
            x = (n * char_width);
            if(x >= screen_width) x = screen_width - char_width;
            break;
    }

    tft.setCursor(x, y);
}

// do some basic VT100/ansi escape sequence handling
void process(char data)
{
    int16_t currentX, currentY;
    if (data == '\r') {
        // start of current line
        tft.getCursor(currentX, currentY);
        int16_t x = 0;
        int16_t y = currentY;
        if(crcrlf) y += char_height; // if CR is converted to CRLF
        tft.setCursor(x, y);

    } else if (data == '\n') {
        // next line, potentially scroll
        tft.getCursor(currentX, currentY);
        int16_t x = currentX;
        int16_t y = currentY + char_height;
        if(y >= screen_height) {
            scroll_up();
            y = screen_height - char_height;
        }
        if(lfcrlf) x = 0; // if LF is converted to CRLF
        tft.setCursor(x, y);

    } else if (data == 8) { // BS
        // backspace move cursor left one
        tft.getCursor(currentX, currentY);
        uint16_t x = currentX - char_width;
        if(x < 0) x = 0;
        tft.setCursor(x, currentY);

    } else if (data == 27) { // ESC
        //If it is an escape character then get the following characters to interpret
        //them as an ANSI escape sequence
        uint8_t escParam1 = 0;
        uint8_t escParam2 = 0;
        char serInChar = getcharw();

        if (serInChar == '[') {
            serInChar = getcharw();
            // Process a number after the "[" character
            while (serInChar >= '0' && serInChar <= '9') {
                serInChar = serInChar - '0';
                if (escParam1 < 100) {
                    escParam1 = escParam1 * 10;
                    escParam1 = escParam1 + serInChar;
                }
                serInChar = getcharw();
            }
            // If a ";" then process the next number
            if (serInChar == ';') {
                serInChar = getcharw();
                while (serInChar >= '0' && serInChar <= '9') {
                    serInChar = serInChar - '0';
                    if (escParam2 < 100) {
                        escParam2 = escParam2 * 10;
                        escParam2 = escParam2 + serInChar;
                    }
                    serInChar = getcharw();
                }
            }

#ifdef DEBUG
            Serial.printf("Esc[ escP1: %d, escP2: %d, %c\n", escParam1, escParam2, serInChar);
#endif

            if(serInChar >= 'A' && serInChar <= 'G') {
                // Handle cursor incremental move commands
                tft.getCursor(currentX, currentY);
                if (escParam1 < 1) escParam1 = 1;
                // Esc[nA moves cursor up n lines
                // Esc[nB moves cursor down n lines
                // Esc[nC moves cursor right n characters
                // Esc[nD moves cursor left n characters
                // Esc[nE moves cursor to start of n next lines
                // Esc[nF moves cursor to start of n previous lines
                // Esc[nG moves cursor to column n
                move_cursor(serInChar, escParam1);
            }
            // Esc[line;ColumnH or Esc[line;Columnf moves cursor to that coordinate
            else if (serInChar == 'H' || serInChar == 'f') {
                if (escParam1 > 0) {
                    escParam1--;
                }
                if (escParam2 > 0) {
                    escParam2--;
                }
                int16_t x = escParam1 * char_width;
                int16_t y = escParam2 * char_height;
                tft.setCursor(x, y);
            }
            //Esc[J=clear from cursor down, Esc[1J=clear from cursor up, Esc[2J=clear complete screen
            else if (serInChar == 'J') {
                tft.getCursor(currentX, currentY);
                if (escParam1 == 0) {
                    if(currentX != 0) {
                        // clear to end of line
                        int16_t x = currentX;
                        int16_t y = currentY;
                        int16_t w = screen_width - currentX;
                        int16_t h = char_height;
                        tft.fillRect(x, y, w, h, RA8875_BLACK);
                    }
                    // clear to end of screen
                    int16_t y = currentY + char_height;
                    tft.fillRect(0, y, screen_width, screen_height - y, RA8875_BLACK);

                } else if (escParam1 == 1) {
                    if(currentX != 0) {
                        // clear to start of line
                        int16_t y = currentY;
                        int16_t w = currentX;
                        int16_t h = char_height;
                        tft.fillRect(0, y, w, h, RA8875_BLACK);
                    }
                    // clear to start of screen
                    int16_t y = currentY - char_height;
                    tft.fillRect(0, 0, screen_width, y, RA8875_BLACK);

                } else if (escParam1 == 2) {
                    clear_screen();
                }
            }
            // Esc[K = erase to end of line, Esc[1K = erase to start of line
            else if (serInChar == 'K') {
                tft.getCursor(currentX, currentY);
                if (escParam1 == 0) {
                    // clear to end of line
                    int16_t x = currentX;
                    int16_t y = currentY;
                    int16_t w = screen_width - currentX;
                    int16_t h = char_height;
                    tft.fillRect(x, y, w, h, RA8875_BLACK);

                } else if (escParam1 == 1) {
                    // clear to start of line
                    int16_t y = currentY;
                    int16_t w = currentX;
                    int16_t h = char_height;
                    tft.fillRect(0, y, w, h, RA8875_BLACK);

                } else if (escParam1 == 2) {
                    // clear entire line
                    int16_t y = currentY;
                    int16_t w = screen_width;
                    int16_t h = char_height;
                    tft.fillRect(0, y, w, h, RA8875_BLACK);
                }

            }
            // Esc[nT = scroll down. optional n is number of lines to scroll
            else if (serInChar == 'T') {
                if (escParam1 == 0) escParam1 = 1;
                for (int i = 0; i < escParam1; ++i) {
                    scroll_down();
                }
            }
            // Esc[nS = scroll up. optional n is number of lines to scroll
            else if (serInChar == 'S') {
                if (escParam1 == 0) escParam1 = 1;
                for (int i = 0; i < escParam1; ++i) {
                    scroll_up();
                }
            }
        } // end of in char = '['

        // EscM scroll up
        else if (serInChar == 'M') {
            scroll_up();
        }
        // EscL scroll down
        else if (serInChar == 'L') {
            scroll_down();
        }
    } // end of in char = 27
    else if (data > 31 && data < 128) {
        // display character, scroll if hit end of screen
        tft.getCursor(currentX, currentY);
        if(currentX >= 0 && currentY >= screen_height) {
            scroll_up();
        }
        tft.print(data);
    }
}

void doreset()
{

}

// read data from UART and display
// process VT100 escape sequences
void loop()
{
    char data;
#ifdef DEBUG
    while (Serial.available()) {
        data = Serial.read();
        process(data);
    }
#endif

    while (Serial1.available()) {
        data = Serial1.read();
        process(data);
    }

#ifdef KEYBOARD
    // get a key from the keyboard
    uint16_t c = process_key(false);

    uint8_t mods = c >> 8; // modifier keys
    if(c != 0) {
        #ifdef DEBUG
        Serial.printf("Got key %04X\n", c);
        #endif

        c = c & 0xFF;
        if((mods & 0x06) == 0x06 && c == 0x7F) {
            // we have ctrl-alt-del
            doreset();
        } else {
            char ansic = 0;
            if(c == 0x81) ansic = 'A'; // up
            else if(c == 0x82) ansic = 'B'; // down
            else if(c == 0x83) ansic = 'D'; // left
            else if(c == 0x84) ansic = 'C'; // right
            if(ansic != 0) {
                // send ansi cursor sequence
                Serial1.write(27); // ESC
                Serial1.write('[');
                Serial1.write(ansic);
                if(local_echo) {
                    move_cursor(ansic, 1);
                }

            } else if(c == 0x8A) { //winkey is clear screen
                clear_screen();

            } else if(c == 0x85) { // today key goes into setup
                config_setup();

            } else {
                Serial1.write(c);
                if(local_echo) {
                    if(c == '\r' || c == '\n' || c == 8) process(c);
                    else if(c < ' ' || c >= 0x80) tft.printf("\\x%02X", c);
                    else if(c >= ' ') tft.printf("%c", c);
                }
            }
        }
    }
#endif

#ifdef TEST
    bool dir = false;

    while(true) {
        clear_screen();
        tft.setCursor(0, 0);

        tft.println("Once upon a midnight dreary, while I pondered, weak and weary,");
        tft.println("Over many a quaint and curious volume of forgotten lore,");
        tft.println("While I nodded, nearly napping, suddenly there came a tapping,");
        tft.println("As of some one gently rapping, rapping at my chamber door.");
        tft.println("'Tis some visitor,' I muttered, 'tapping at my chamber door Only this, and nothing more.'");
        tft.println("");
        tft.println("Ah, distinctly I remember it was in the bleak December,");
        tft.println("And each separate dying ember wrought its ghost upon the floor.");
        tft.println("Eagerly I wished the morrow;- vainly I had sought to borrow");
        tft.println("From my books surcease of sorrow- sorrow for the lost Lenore-");
        tft.println("For the rare and radiant maiden whom the angels name Lenore-");
        tft.println("Nameless here for evermore.");
        delay(1000);

        if(dir) {
            for (int i = 0; i < 12; ++i) {
                scroll_up();
                delay(100);
            }
        } else {
            for (int i = 0; i < 12; ++i) {
                scroll_down();
                delay(1000);
            }
        }
        dir = !dir;
    }
#endif
}
