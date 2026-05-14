
/*
 * This SmartMatrix Library example displays GIF animations loaded from a SD Card connected to the Teensy 3/4 and ESP32
 *
 * This example requires SmartMatrix Library 4.0 and AnimatedGIF Library to be installed, you can do this from Arduino Library Manager
 *   - https://github.com/pixelmatix/SmartMatrix
 *   - https://github.com/bitbank2/AnimatedGIF
 *
 * Wiring is on the default Teensy 3.2 SPI pins, and chip select can be on any GPIO,
 * set by defining SD_CS in the code below.  For Teensy 3.5/3.6/4.1 with the onboard SDIO, SD_CS should be the default BUILTIN_SDCARD
 * Function     | Pin
 * DOUT         |  11
 * DIN          |  12
 * CLK          |  13
 * CS (default) |  15
 *
 * Wiring for ESP32 follows the default for the ESP32 SD Library, see: https://github.com/espressif/arduino-esp32/tree/master/libraries/SD
 *
 * This code first looks for .gif files in the /gifs/ directory
 * (customize below with the GIF_DIRECTORY definition) then plays random GIFs in the directory,
 * looping each GIF for DISPLAY_TIME_SECONDS
 *
 * If you find any GIFs that won't play properly, please attach them to a new
 * Issue post in the GitHub repo here:
 * https://github.com/pixelmatix/AnimatedGIFs/issues
 */

/*
 * CONFIGURATION:
 *  - Set the chip select pin for your board.  On Teensy 3.5/3.6/4.1, the onboard microSD CS pin is "BUILTIN_SDCARD"
 *  - For ESP32 used with large panels, you don't need to lower the refreshRate, but you can lower the frameRate (number of times the refresh buffer
 *    is updaed with new data per second), giving more time for the CPU to decode the GIF.
 *    Use matrix.setMaxCalculationCpuPercentage() or matrix.setCalcRefreshRateDivider()
 */


#define SUPPRESS_ERROR_MESSAGE_FOR_BEGIN

// Uncomment to use Adafruit Bluefruit LE UART Friend instead of HM-10
#define USE_ADAFRUIT_BLUEFRUIT

// Flow control pins for Adafruit Bluefruit LE UART Friend
#define BLUEFRUIT_CTS_PIN 22 // Teensy pin pulling Bluefruit CTS to GND
#define BLUEFRUIT_RTS_PIN 19 // Teensy pin reading Bluefruit RTS
#define BLUEFRUIT_MOD_PIN -1 // Disabled: Settings already permanently saved to Bluefruit flash

#ifdef SIMULATOR_MODE
  #include <GifDecoder.h>
  #include <IRremote.hpp>
  #include <MatrixHardware_Teensy4_ShieldV5.h>
  #include <SD.h>
  #include <SPI.h>
  #define USE_ADAFRUIT_GFX_LAYERS
  #include <SmartMatrix.h>
#else
  #include "src/GifDecoder/src/GifDecoder.h"
  #include <IRremote.hpp>
  #include "src/SmartMatrix/src/MatrixHardware_Teensy4_ShieldV5.h"        // SmartLED Shield for Teensy 4 (V5)
  #include <SD.h>
  #include <SPI.h>
  #define USE_ADAFRUIT_GFX_LAYERS
  #include "src/SmartMatrix/src/SmartMatrix.h"
#endif

#include "gimpbitmap.h"

// 64x64 image bitmaps.
#include "bitmaps/bm_brat.c"
#include "bitmaps/bm_surprised_pikachu.c"

#include "SnakeGame.h"
#include "PacmanGame.h"
#include "FroggerGame.h"
#include "TetrisGame.h"
#include "Leaderboard.h"
#include "Visualizations.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeMono24pt7b.h>

enum AppMode {
    MODE_GIF,
    MODE_SNAKE,
    MODE_PACMAN,
    MODE_FROGGER,
    MODE_TETRIS,
    MODE_VISUALIZATIONS,
    MODE_TEXT,
    MODE_LAYER
};
static AppMode current_mode = MODE_GIF;

// --- Layer Mixer State ---
static int layer_bg_idx = -1;
static int layer_anim_idx = -1;
static int layer_fg_idx = -1;
static int layer_txt_idx = -1;
static rgb24 layer_gif_buffer[64][64];

// GIF Bitmaps.
#include "bitmaps/bm_ariel_dance.c"
const uint8_t * gifsList[] = { bm_ariel_dance };
const int gifsSizeList[] = { sizeof(bm_ariel_dance) };

#include "FilenameFunctions.h"

#define DISPLAY_TIME_SECONDS 10
#define NUMBER_FULL_CYCLES   100

// Teensy 4.0 using CS0.
// If use_sd == false, SD is not read and colors stand in for images.
#define SD_CS 0
// Teensy 4.1 with builtin
// #define SD_CS BUILTIN_SDCARD

bool use_sd = true;
// The SmartMatrix takes up SPI0. Use SPI1 instead.
const bool use_spi1 = true;

// Teensy SD Library requires a trailing slash in the directory name
#define GIF_DIRECTORY "/gifs/"

// Data pin the IR receiver is hooked up to.
#define IR_RECEIVE_PIN 16

// range 0-255 technically, but battery drives less than that. Stop it
// at 180.
const int max_brightness = 180;
// Start at low brightness - 26.
static int brightness = 24;

const rgb24 COLOR_BLACK = {
    0, 0, 0 };
const rgb24 COLOR_WHITE = {
    255, 255, 255 };
const rgb24 COLOR_RED = {
    255, 0, 0 };
const rgb24 COLOR_GREEN = {
    0, 255, 0 };
const rgb24 COLOR_BLUE = {
    0, 0, 255 };

/* SmartMatrix configuration and memory allocation */
#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 64;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;  // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SMARTMATRIX_OPTIONS_C_SHAPE_STACKING);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

/* template parameters are maxGifWidth, maxGifHeight, lzwMaxBits
 * 
 * lzwMaxBits is included for backwards compatibility reasons, but isn't used anymore
 */
GifDecoder<kMatrixWidth, kMatrixHeight, 12> decoder;

GIFIMAGE gif;
int iGIFWidth, iGIFHeight;
uint8_t *pGIFBuf;


void screenClearCallback(void) {
  if (current_mode == MODE_LAYER) {
      for (int y = 0; y < 64; y++) {
          for (int x = 0; x < 64; x++) {
              rgb24 tmp = {1, 1, 1};
              layer_gif_buffer[y][x] = tmp; // Magic transparent color
          }
      }
  } else {
      backgroundLayer.fillScreen({0,0,0});
  }
}

void updateScreenCallback(void) {
  if (current_mode != MODE_LAYER) {
      backgroundLayer.swapBuffers();
  }
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
    if (current_mode == MODE_LAYER) {
        if (red == 1 && green == 1 && blue == 1) red = 2; // Avoid transparent color collision
        if (x >= 0 && x < 64 && y >= 0 && y < 64) {
            rgb24 tmp = {red, green, blue};
            layer_gif_buffer[y][x] = tmp;
        }
    } else {
        backgroundLayer.drawPixel(x, y, {red, green, blue});
    }
}

int wrap_enumerateGIFFiles(const char *directoryName, bool displayFilenames) {
    if (use_sd) {
        return enumerateGIFFiles(directoryName, displayFilenames);
    }
    return 4;
}

// Remote Layout:
//
// BUT_VOL_DOWN     BUT_PLAY        BUT_VOL_UP
// BUT_SETUP        BUT_UP          BUT_STOP
// BUT_LEFT         BUT_ENTER       BUT_RIGHT
// BUT_0            BUT_DOWN        BUT_xBACK
// BUT_1            BUT_2           BUT_3
// BUT_4            BUT_5           BUT_6
// BUT_7            BUT_8           BUT_9


#define BUT_VOL_DOWN    0xFF00BF00
#define BUT_PLAY        0xFE01BF00
#define BUT_VOL_UP      0xFD02BF00
#define BUT_SETUP       0xFB04BF00
#define BUT_UP          0xFA05BF00
#define BUT_STOP        0xF906BF00
#define BUT_LEFT        0xF708BF00
#define BUT_ENTER       0xF609BF00
#define BUT_RIGHT       0xF50ABF00
#define BUT_0           0xF30CBF00
#define BUT_DOWN        0xF20DBF00
#define BUT_BACK        0xF10EBF00
#define BUT_1           0xEF10BF00
#define BUT_2           0xEE11BF00
#define BUT_3           0xED12BF00
#define BUT_4           0xEB14BF00
#define BUT_5           0xEA15BF00
#define BUT_6           0xE916BF00
#define BUT_7           0xE718BF00
#define BUT_8           0xE619BF00
#define BUT_9           0xE51ABF00

static unsigned long last_debug_write_time = 0; // stored in millis
static bool allow_debug_clear = true;
void maybeClearDebugScreen(unsigned long now) {
    if (!allow_debug_clear) {
        return;
    }

    // Only clear debug screen if its been on for more than 3000 seconds and
    // no recent valid input. debug_buf should be empty right now.
    if (!(last_debug_write_time > 0 && now - last_debug_write_time > 3000)) {
        return;
    }

    indexedLayer.fillScreen(0);
    indexedLayer.swapBuffers();
    scrollingLayer.start("", -1);

    last_debug_write_time = 0;
}

// Writes debug string text. If end == true, clears
// debug text after a few seconds of no changes.
void writeDebugScreen(const char* text, unsigned long now, bool allow_clear = true) {
    allow_debug_clear = allow_clear;
    indexedLayer.fillScreen(0);
    indexedLayer.setIndexedColor(1, COLOR_BLACK);
    for(int row=0; row<6; row++) {
        for(int col=0; col<64; col++) {
            indexedLayer.drawPixel(col,row,1);
        }
    }
    indexedLayer.swapBuffers();
    scrollingLayer.start(text, -1);

    last_debug_write_time = now;
}

bool validatePressAndGetName(uint32_t button, char* buf) {
    switch(button) {
        case BUT_VOL_DOWN:
            strcat(buf, "VOL_DOWN");
            break;
        case BUT_PLAY:
            strcat(buf, "PLAY");
            break;
        case BUT_VOL_UP:
            strcat(buf, "VOL_UP");
            break;
        case BUT_SETUP:
            strcat(buf, "SETUP");
            break;
        case BUT_UP:
            strcat(buf, "UP");
            break;
        case BUT_STOP:
            strcat(buf, "STOP");
            break;
        case BUT_LEFT:
            strcat(buf, "LEFT");
            break;
        case BUT_ENTER:
            strcat(buf, "ENTER");
            break;
        case BUT_RIGHT:
            strcat(buf, "RIGHT");
            break;
        case BUT_0:
            strcat(buf, "0");
            break;
        case BUT_DOWN:
            strcat(buf, "DOWN");
            break;
        case BUT_BACK:
            strcat(buf, "BACK");
            break;
        case BUT_1:
            strcat(buf, "1");
            break;
        case BUT_2:
            strcat(buf, "2");
            break;
        case BUT_3:
            strcat(buf, "3");
            break;
        case BUT_4:
            strcat(buf, "4");
            break;
        case BUT_5:
            strcat(buf, "5");
            break;
        case BUT_6:
            strcat(buf, "6");
            break;
        case BUT_7:
            strcat(buf, "7");
            break;
        case BUT_8:
            strcat(buf, "8");
            break;
        case BUT_9:
            strcat(buf, "9");
            break;
        default:
            strcat(buf, "Unknown Button!");
            return false;
    }
    return true;
}

void adjustBrightness(int amount) {
    int next_brightness = brightness + amount;
    if (amount < 0) {
        brightness = max(1, next_brightness);
    } else {
        brightness = min(max_brightness, next_brightness);
    }
    matrix.setBrightness(brightness);
}

int num_files = 0;
static int cur_image_idx = 0;
bool is_first_frame = true;
void change_image_idx(int amount) {
    cur_image_idx = cur_image_idx + amount;

    // Wrap around images on overflow.
    if (cur_image_idx < 0) {
        cur_image_idx = num_files - 1;
    } else if (cur_image_idx >= num_files) {
        cur_image_idx = 0;
    }
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    is_first_frame = true;
}

void HandleIRInputs(unsigned long now) {
    char button_name[16];
    button_name[0] = 0;
    char debug_buf[300];
    debug_buf[0] = 0;
    static unsigned long lastAcceptedIRTimestamp = 0; // stored in millis

    if (!IrReceiver.decode()) {
        // Nothing received.
        return;
    }

    uint32_t received_data = IrReceiver.decodedIRData.decodedRawData;
    if (!validatePressAndGetName(received_data, button_name)){
        // Throw out invalid input for invalid reads.
        IrReceiver.resume(); // Receive the next value.
        return;
    }

    if (lastAcceptedIRTimestamp > 0 && now - lastAcceptedIRTimestamp < 400) {
        // Last valid input was too recent. Add a cooldown.
        return;
    }
    lastAcceptedIRTimestamp = now;

    if (lbIsActive()) {
        switch(received_data) {
            case BUT_UP:    lbHandleInput(0, -1, false); break;
            case BUT_DOWN:  lbHandleInput(0, 1, false); break;
            case BUT_LEFT:  lbHandleInput(-1, 0, false); break;
            case BUT_RIGHT: lbHandleInput(1, 0, false); break;
            case BUT_ENTER: lbHandleInput(0, 0, true); break;
        }
        IrReceiver.resume();
        return;
    }

    switch(received_data) {
        case BUT_VOL_DOWN:
            adjustBrightness(-6);
            strcat(debug_buf, "BRT: ");
            strcat(debug_buf, String(brightness).c_str());
            break;
        case BUT_VOL_UP:
            adjustBrightness(6);
            strcat(debug_buf, "BRT: ");
            strcat(debug_buf, String(brightness).c_str());
            break;
        case BUT_LEFT:
            change_image_idx(-1);
            break;
        case BUT_RIGHT:
            change_image_idx(1);
            break;
        default:
            // Unhandled buttons just display name.
            
            strcat(debug_buf, button_name);
            break;
    }

    if (strlen(debug_buf) > 0) {
        writeDebugScreen(debug_buf, now);
    }
    IrReceiver.resume(); // Receive the next value
}

static bool is_uploading_ble = false;
static File ble_upload_file;
long current_ble_baud = 9600; // Track the negotiated baud rate
static size_t ble_upload_expected_size = 0;
static size_t ble_upload_received_size = 0;
static unsigned long ble_upload_last_rx_time = 0;
static String ble_upload_filename = "";

void HandleBLEInputs(unsigned long now) {
    static String ble_buffer = "";
    
    static int last_percent = -1;
    
    // Auto-recover if a Bluetooth upload stalls (e.g. dropped connection)
    if (is_uploading_ble && (now - ble_upload_last_rx_time > 2500)) {
        Serial.println("BLE Upload TIMEOUT. Aborting...");
        if (ble_upload_file) {
            ble_upload_file.close();
            
            // Delete the corrupted partial file
            String fullPath = "/gifs/" + ble_upload_filename;
            if (SD.exists(fullPath.c_str())) {
                SD.remove(fullPath.c_str());
                Serial.println("Cleaned up corrupted file: " + fullPath);
            }
        }
        is_uploading_ble = false;
        ble_buffer = "";
        
        indexedLayer.fillScreen(0);
        indexedLayer.swapBuffers();
        last_percent = -1;
        
        writeDebugScreen("Upload Failed!", now);
    }
    
    while (Serial5.available() > 0) {
        if (is_uploading_ble) {
            ble_upload_last_rx_time = now;
            // Read bytes directly to file
            uint8_t buf[64];
            int to_read = min((int)Serial5.available(), (int)(ble_upload_expected_size - ble_upload_received_size));
            if (to_read > 64) to_read = 64;
            
            int bytes_read = Serial5.readBytes((char*)buf, to_read);
            if (bytes_read > 0) {
                if (ble_upload_file) {
                    ble_upload_file.write(buf, bytes_read);
                }
                ble_upload_received_size += bytes_read;
                
                int percent = (ble_upload_received_size * 100) / ble_upload_expected_size;
                if (percent != last_percent) {
                    last_percent = percent;
                    if (percent % 5 == 0) {
                        Serial.print("BLE Upload Progress: ");
                        Serial.print(percent);
                        Serial.println("%");
                    }
                    char pbuf[32];
                    sprintf(pbuf, "Up: %d%%", percent);
                    
                    // Clear the indexed layer so it doesn't overlay
                    indexedLayer.fillScreen(0);
                    indexedLayer.swapBuffers();
                    
                    // Draw directly to the main background layer
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.setFont(font5x7);
                    
                    // Center the text vertically (approx row 28)
                    backgroundLayer.drawString(5, 28, {255, 255, 255}, pbuf); 
                    backgroundLayer.swapBuffers();
                }
                
                if (ble_upload_received_size >= ble_upload_expected_size) {
                    // Done!
                    if (ble_upload_file) {
                        ble_upload_file.close();
                    }
                    is_uploading_ble = false;
                    Serial.println("BLE Upload Complete!");
                    
                    indexedLayer.fillScreen(0);
                    indexedLayer.swapBuffers();
                    last_percent = -1;
                    
                    writeDebugScreen("Upload Complete!", now);
                    
                    // Change mode based on file uploaded
                    lbDeactivate();
                    if (ble_upload_filename.equals("txt.bin")) {
                        current_mode = MODE_TEXT;
                        textInit(use_sd);
                    } else {
                        current_mode = MODE_GIF;
                        
                        invalidateGIFCache(); // Refresh the sorted array
                        // Re-index GIFs
                        num_files = enumerateGIFFiles("/gifs", false);
                        
                        // Find the index of the newly uploaded file
                        for (int i = 0; i < num_files; i++) {
                            char nameBuf[64];
                            getGIFFilenameByIndex("/gifs", i, nameBuf);
                            if (ble_upload_filename.equals(nameBuf)) {
                                cur_image_idx = i;
                                break;
                            }
                        }
                        
                        // Force refresh screen to start new GIF
                        backgroundLayer.fillScreen(COLOR_BLACK);
                        backgroundLayer.swapBuffers();
                        backgroundLayer.fillScreen(COLOR_BLACK);
                        backgroundLayer.swapBuffers();
                        is_first_frame = true;
                    }
                }
            }
            continue; // Skip the rest of the loop
        }
        
        char c = Serial5.read();
        
        Serial.print("BLE Packet Received: '");
        Serial.print(c);
        Serial.print("' (ASCII: ");
        Serial.print((int)c);
        Serial.println(")");
        
        // Add to buffer for string matching
        ble_buffer += c;
        if (ble_buffer.length() > 64) {
            ble_buffer.remove(0, ble_buffer.length() - 64); // Keep last 64 chars
        }
        
        // Check for PING command to sync baud rate with phone
        int pingIdx = ble_buffer.lastIndexOf("<PING>");
        if (pingIdx >= 0) {
            Serial.println("Phone app requested baud rate. Sending...");
            Serial5.print("<BAUD:");
            Serial5.print(current_ble_baud);
            Serial5.print(">");
            ble_buffer = "";
            continue;
        }

        // Check for GETANIM command
        int getAnimIdx = ble_buffer.lastIndexOf("<GETANIM>");
        
        if (getAnimIdx >= 0) {
            ble_buffer = ""; // Clear buffer immediately
            
            static unsigned long last_getanim_time = 0;
            if (millis() - last_getanim_time < 5000 && last_getanim_time != 0) {
                Serial.println("Dropping spam GETANIM request.");
                continue;
            }
            last_getanim_time = millis();
            
            Serial.println("Phone app requested Animation list. Pre-computing...");
            
            String anim_list = "";
            const char* const VISUALIZATION_NAMES[] = {
                "Vis: Plasma", "Vis: Concentric", "Vis: Julia", "Vis: Game of Life",
                "Vis: Fractal Tunnel", "Vis: Cubic Matrix", "Vis: DVD Logo"
            };
            const int NUM_VISUALIZATIONS = 7;
            
            for (int i = 0; i < NUM_VISUALIZATIONS; i++) {
                anim_list += String(VISUALIZATION_NAMES[i]);
                if (i < NUM_VISUALIZATIONS - 1 || use_sd) {
                    anim_list += ",";
                }
            }
            
            if (use_sd) {
                // Close active GIF handle to avoid SD card SPI collisions during enumeration
                if (my_sd_file) my_sd_file.close();
                is_first_frame = true; // Force decoder to restart cleanly
                invalidateGIFCache(); // Force a fresh read and sort for the app
                int total_gifs = enumerateGIFFiles("/gifs", false);
                for (int i = 0; i < total_gifs; i++) {
                    char nameBuf[64];
                    getGIFFilenameByIndex("/gifs", i, nameBuf);
                    
                    String fname = String(nameBuf);
                    if (fname.startsWith("/gifs/")) fname = fname.substring(6);
                    
                    anim_list += fname;
                    if (i < total_gifs - 1) {
                        anim_list += ",";
                    }
                }
            }
            
            int total_size = anim_list.length();
            Serial.print("Total Animation list size: ");
            Serial.println(total_size);
            
            // Send Header
            Serial5.print("<ANIM_START:");
            Serial5.print(total_size);
            Serial5.print(">");
            
            // Allow the BLE module a moment to transmit the header before blasting the payload
            delay(100); 
            
            // Transmit the pre-computed payload using Software-Assisted Hardware Flow Control
            int rts_blocks = 0;
            unsigned long total_rts_wait_time = 0;

            for (int i = 0; i < total_size; i++) {
                // Check if Bluefruit's 256-byte buffer is full
                if (digitalRead(BLUEFRUIT_RTS_PIN) == HIGH) {
                    rts_blocks++;
                    unsigned long wait_start = millis();
                    
                    // Pause until Bluefruit transmits packets and clears space
                    while (digitalRead(BLUEFRUIT_RTS_PIN) == HIGH) {
                        delay(1);
                    }
                    
                    total_rts_wait_time += (millis() - wait_start);
                }
                
                Serial5.print(anim_list[i]);
            }
            
            Serial.print("BLE Transmission complete. RTS blocked ");
            Serial.print(rts_blocks);
            Serial.print(" times, waiting a total of ");
            Serial.print(total_rts_wait_time);
            Serial.println(" ms.");
            
            // Flush any garbage or queued requests that accumulated during the blocking send
            while (Serial5.available()) Serial5.read();
            
            continue;
        }

        // Check for SETANIM command
        int setAnimIdx = ble_buffer.lastIndexOf("<SETANIM:");
        if (setAnimIdx >= 0) {
            int endIdx = ble_buffer.indexOf('>', setAnimIdx + 9);
            if (endIdx > setAnimIdx) {
                String idxStr = ble_buffer.substring(setAnimIdx + 9, endIdx);
                int target_idx = idxStr.toInt();
                Serial.print("Phone requested Animation index: ");
                Serial.println(target_idx);
                
                if (target_idx == -1) {
                    lbDeactivate();
                    current_mode = MODE_TEXT;
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                } else if (target_idx >= 0 && target_idx < 7) {
                    lbDeactivate();
                    current_mode = MODE_VISUALIZATIONS;
                    visSetCurrent(target_idx);
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                } else if (use_sd) {
                    int gif_idx = target_idx - 7;
                    int total_gifs = enumerateGIFFiles("/gifs", false);
                    if (gif_idx >= 0 && gif_idx < total_gifs) {
                        lbDeactivate();
                        current_mode = MODE_GIF;
                        cur_image_idx = gif_idx;
                        is_first_frame = true;
                        backgroundLayer.fillScreen(COLOR_BLACK);
                        backgroundLayer.swapBuffers();
                        backgroundLayer.fillScreen(COLOR_BLACK);
                        backgroundLayer.swapBuffers();
                    }
                }
                ble_buffer = "";
                continue;
            }
        }
        
        // Check for LAYER command
        int layerIdx = ble_buffer.lastIndexOf("<LAYER:");
        if (layerIdx >= 0) {
            int endIdx = ble_buffer.indexOf('>', layerIdx + 7);
            if (endIdx > layerIdx) {
                String cmdStr = ble_buffer.substring(layerIdx + 7, endIdx);
                // Expected: bg,anim,fg,txt
                int comma1 = cmdStr.indexOf(',');
                int comma2 = cmdStr.indexOf(',', comma1 + 1);
                int comma3 = cmdStr.indexOf(',', comma2 + 1);
                
                if (comma1 > 0 && comma2 > 0 && comma3 > 0) {
                    int bg = cmdStr.substring(0, comma1).toInt();
                    int anim = cmdStr.substring(comma1 + 1, comma2).toInt();
                    int fg = cmdStr.substring(comma2 + 1, comma3).toInt();
                    int txt = cmdStr.substring(comma3 + 1).toInt();
                    
                    Serial.print("Phone requested LAYER mix: BG=");
                    Serial.print(bg); Serial.print(" ANIM=");
                    Serial.print(anim); Serial.print(" FG=");
                    Serial.print(fg); Serial.print(" TXT=");
                    Serial.println(txt);
                    
                    lbDeactivate();
                    current_mode = MODE_LAYER;
                    
                    layer_bg_idx = bg;
                    layer_anim_idx = anim;
                    layer_fg_idx = fg;
                    layer_txt_idx = txt;
                    
                    // Clear the GIF buffer to our magic transparent color
                    for (int y = 0; y < 64; y++) {
                        for (int x = 0; x < 64; x++) {
                            rgb24 tmp = {1, 1, 1};
                            layer_gif_buffer[y][x] = tmp;
                        }
                    }
                    
                    // Initialize animation layer
                    if (layer_anim_idx >= 0) {
                        if (layer_anim_idx < 7) {
                            visSetCurrent(layer_anim_idx);
                        } else {
                            int total_gifs = enumerateGIFFiles("/gifs", false);
                            int gif_idx = layer_anim_idx - 7;
                            if (gif_idx >= 0 && gif_idx < total_gifs) {
                                cur_image_idx = gif_idx;
                                is_first_frame = true;
                            } else {
                                // Default to 0 if out of bounds to avoid crashes
                                cur_image_idx = 0;
                                is_first_frame = true;
                            }
                        }
                    }
                    
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                    backgroundLayer.fillScreen(COLOR_BLACK);
                    backgroundLayer.swapBuffers();
                }
                ble_buffer = "";
                continue;
            }
        }
        
        // Check for upload command
        int uIdx = ble_buffer.lastIndexOf("<U:");
        if (uIdx >= 0) {
            int endIdx = ble_buffer.indexOf('>', uIdx + 3);
            if (endIdx > uIdx) {
                // Found complete command!
                String cmdStr = ble_buffer.substring(uIdx + 3, endIdx);
                // Parse "filename.gif,12345"
                int commaIdx = cmdStr.indexOf(',');
                if (commaIdx > 0) {
                    String filename = cmdStr.substring(0, commaIdx);
                    int fileSize = cmdStr.substring(commaIdx + 1).toInt();
                    
                    if (fileSize > 0 && use_sd) {
                        Serial.print("Starting BLE Upload: ");
                        Serial.print(filename);
                        Serial.print(" size: ");
                        Serial.println(fileSize);
                        
                        String fullPath = String("/gifs/") + filename;
                        if (SD.exists(fullPath.c_str())) {
                            SD.remove(fullPath.c_str());
                        }
                        ble_upload_file = SD.open(fullPath.c_str(), FILE_WRITE);
                        ble_upload_expected_size = fileSize;
                        ble_upload_received_size = 0;
                        ble_upload_last_rx_time = now;
                        ble_upload_filename = filename;
                        is_uploading_ble = true;
                        ble_buffer = ""; // clear buffer
                        continue; // skip normal processing for this byte, start receiving data
                    }
                }
                ble_buffer = ""; // clear buffer if invalid format
            }
        }
        
        // Check for common connect/disconnect strings (like HM-10)
        if (ble_buffer.endsWith("OK+CONN") || ble_buffer.endsWith("CONNECTED")) {
            Serial.println("\n*** BLUETOOTH DEVICE CONNECTED ***\n");
            ble_buffer = ""; // clear so we don't trigger again
        } else if (ble_buffer.endsWith("OK+LOST") || ble_buffer.endsWith("DISCONNECTED")) {
            Serial.println("\n*** BLUETOOTH DEVICE DISCONNECTED ***\n");
            ble_buffer = "";
        }

        switch (c) {
            case '-': adjustBrightness(-6); break;
            case '+': adjustBrightness(6); break;
            case 'm':
                lbDeactivate(); // Force close any active leaderboard
                if (current_mode == MODE_GIF) {
                    current_mode = MODE_SNAKE;
                    snakeInit();
                } else if (current_mode == MODE_SNAKE) {
                    current_mode = MODE_PACMAN;
                    pacmanInit(use_sd);
                } else if (current_mode == MODE_PACMAN) {
                    current_mode = MODE_FROGGER;
                    froggerInit();
                } else if (current_mode == MODE_FROGGER) {
                    current_mode = MODE_TETRIS;
                    tetrisInit();
                } else if (current_mode == MODE_TETRIS) {
                    current_mode = MODE_VISUALIZATIONS;
                    visInit();
                } else {
                    current_mode = MODE_GIF;
                    is_first_frame = true;
                }
                break;
            case 'l':
                if (lbIsActive()) lbHandleInput(-1, 0, false);
                else if (current_mode == MODE_SNAKE) snakeSetDirection(-1, 0);
                else if (current_mode == MODE_PACMAN) pacmanSetDirection(-1, 0);
                else if (current_mode == MODE_FROGGER) froggerSetDirection(-1, 0);
                else if (current_mode == MODE_TETRIS) tetrisHandleInput(-1, 0, false);
                else if (current_mode == MODE_VISUALIZATIONS) visHandleInput(-1, 0, false);
                else if (current_mode == MODE_GIF) change_image_idx(-1);
                break;
            case 'r':
                if (lbIsActive()) lbHandleInput(1, 0, false);
                else if (current_mode == MODE_SNAKE) snakeSetDirection(1, 0);
                else if (current_mode == MODE_PACMAN) pacmanSetDirection(1, 0);
                else if (current_mode == MODE_FROGGER) froggerSetDirection(1, 0);
                else if (current_mode == MODE_TETRIS) tetrisHandleInput(1, 0, false);
                else if (current_mode == MODE_VISUALIZATIONS) visHandleInput(1, 0, false);
                else if (current_mode == MODE_GIF) change_image_idx(1);
                break;
            case 'u':
                if (lbIsActive()) lbHandleInput(0, -1, false);
                else if (current_mode == MODE_SNAKE) snakeSetDirection(0, -1);
                else if (current_mode == MODE_PACMAN) pacmanSetDirection(0, -1);
                else if (current_mode == MODE_FROGGER) froggerSetDirection(0, -1);
                else if (current_mode == MODE_TETRIS) tetrisHandleInput(0, -1, false);
                break;
            case 'd':
                if (lbIsActive()) lbHandleInput(0, 1, false);
                else if (current_mode == MODE_SNAKE) snakeSetDirection(0, 1);
                else if (current_mode == MODE_PACMAN) pacmanSetDirection(0, 1);
                else if (current_mode == MODE_FROGGER) froggerSetDirection(0, 1);
                else if (current_mode == MODE_TETRIS) tetrisHandleInput(0, 1, false);
                break;
            case 'e':
                if (lbIsActive()) lbHandleInput(0, 0, true);
                else if (current_mode == MODE_SNAKE) snakeHandleEnter();
                else if (current_mode == MODE_PACMAN) pacmanHandleEnter();
                else if (current_mode == MODE_FROGGER) froggerHandleEnter();
                else if (current_mode == MODE_TETRIS) tetrisHandleInput(0, 0, true);
                break;
            default:
                break;
        }
    }
}

void drawBitmap64(int16_t x, int16_t y, const gimp64x64bitmap* bitmap) {
  for(unsigned int i=0; i < bitmap->height; i++) {
    for(unsigned int j=0; j < bitmap->width; j++) {
      rgb24 pixel = { bitmap->pixel_data[(i*bitmap->width + j)*3 + 0],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 1],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 2] };
      backgroundLayer.drawPixel(x + j, y + i, pixel);
    }
  }
}

void displayGIFFromMemoryById(int id, unsigned long now) {
    // these variables keep track of when we're done displaying the last frame and are ready for a new frame
    static uint32_t lastFrameDisplayTime = 0;
    static unsigned int currentFrameDelay = 0;

    // Check if we should display the next frame on this cycle.
    if ((now - lastFrameDisplayTime) > currentFrameDelay) {
        if (is_first_frame) {
            int startResult = decoder.startDecoding((uint8_t *)gifsList[0], gifsSizeList[0]);
            if(startResult < 0) {
                Serial.print("GIF Memory startDecoding Error: ");
                Serial.println(startResult);
                writeDebugScreen("Bad frame", now);
                lastFrameDisplayTime = 0;
            }
        }
        // decode frame without delaying after decode
        int result = decoder.decodeFrame(false);

        lastFrameDisplayTime = now;
        currentFrameDelay = decoder.getFrameDelay_ms();

        // it's time to start decoding a new GIF if there was an error, and don't wait to decode
        if(result < 0) {
            Serial.print("GIF Memory decodeFrame Error: ");
            Serial.println(result);
            writeDebugScreen("Bad frame", now);
            lastFrameDisplayTime = 0;
            currentFrameDelay = 0;
        }
    }
}

void drawImageNoSD(unsigned long now) {
    switch(cur_image_idx) {
        case 0:
            backgroundLayer.fillScreen(COLOR_BLACK);
            backgroundLayer.swapBuffers();
            break;
        case 1:
            drawBitmap64(0, 0, &bm_brat);
            backgroundLayer.swapBuffers();
            break;
        case 2:
            drawBitmap64(0, 0, &bm_surprised_pikachu);
            backgroundLayer.swapBuffers();
            break;
        case 3:
            displayGIFFromMemoryById(0, now);
            break;
        default:
            backgroundLayer.fillScreen(COLOR_BLACK);
            backgroundLayer.swapBuffers();
    }
}

void drawImageWithSD(unsigned long now) {
    static unsigned long error_timeout = 0;
    static bool hard_sd_crash = false;
    if (error_timeout > 0) {
        if (now < error_timeout) return; // Wait until timeout finishes
        error_timeout = 0; // Timeout done, attempt to recover!
        is_first_frame = true;
        
        if (hard_sd_crash) {
            Serial.println("Attempting hard SD restart...");
            initSDCard(SD_CS, use_spi1);
            hard_sd_crash = false;
        }
    }

    // For GIFs
    // these variables keep track of when we're done displaying the last frame and are ready for a new frame
    static uint32_t lastFrameDisplayTime = 0;
    static unsigned int currentFrameDelay = 0;
    static bool start_ok = true;

    if (is_first_frame) {
        char name_buf[63];
        name_buf[0] = 0;
        if(!openGifFilenameByIndex("/gifs/", cur_image_idx, name_buf)) {
            writeDebugScreen("Fail", now);
            Serial.println("Fail");
            // If we couldn't even open the file, wait 1s before retrying
            error_timeout = now + 1000;
            hard_sd_crash = true;
            return;
        } else {
            writeDebugScreen(name_buf, now);
        }
        Serial.println(my_sd_file.name());
        
        // Reset timing so new GIF loads immediately
        lastFrameDisplayTime = 0;
        currentFrameDelay = 0;
        start_ok = true;
    }

    // Check if we should display the next frame on this cycle.
    if ((now - lastFrameDisplayTime) > currentFrameDelay) {
        if (is_first_frame || !start_ok) {
            int startResult = decoder.startDecoding();
            if(startResult < 0) {
                Serial.print("GIF SD startDecoding Error: ");
                Serial.println(startResult);
                if (my_sd_file) my_sd_file.close();
                start_ok = false;
                error_timeout = now + 1000; // Hard wait 1 second
                return;
            }
        }
        start_ok = true;
        
        // decode frame without delaying after decode
        int result = decoder.decodeFrame(false);

        lastFrameDisplayTime = now;
        currentFrameDelay = decoder.getFrameDelay_ms();

        if(result < 0) {
            Serial.print("GIF SD decodeFrame Error: ");
            Serial.println(result);
            if (my_sd_file) my_sd_file.close();
            start_ok = false;
            error_timeout = now + 1000; // Hard wait 1 second
            return;
        } else if (result == ERROR_DONE_PARSING) {
            start_ok = false; // Loop the GIF
        }
    }
}


void autoNegotiateBaudRate() {
#ifdef USE_ADAFRUIT_BLUEFRUIT
    Serial.println("Adafruit Bluefruit LE UART Friend Mode Enabled!");
    Serial.println("Defaulting to 9600 baud...");
    
    // Adafruit Bluefruit uses 9600 baud by default in UART mode
    current_ble_baud = 9600;
    Serial5.begin(9600);
    
    // Clear the error message if it was shown (though it shouldn't be here)
    indexedLayer.fillScreen(0);
    indexedLayer.swapBuffers();
    return;
#endif

    Serial.println("Waiting for HM-10 to boot...");
    delay(1500); 
    
    long possible_bauds[] = {115200, 9600, 460800, 57600, 38400, 19200, 230400, 4800};
    
    while (true) {
        Serial.println("Checking HM-10 baud rate...");
        
        for (int i = 0; i < 8; i++) {
            long test_baud = possible_bauds[i];
            Serial.print("Probing "); Serial.print(test_baud); Serial.println(" baud...");
            
            Serial5.begin(test_baud);
            Serial5.setTimeout(100);
            while(Serial5.available()) Serial5.read(); // clear noise
            delay(100);
            
            bool found = false;
            bool is_clone = false;
            
            // Test standard AT
            Serial5.print("AT");
            delay(100);
            if (Serial5.readString().indexOf("OK") >= 0) {
                found = true;
            } else {
                // Test clone AT
                Serial5.print("AT\r\n");
                delay(100);
                if (Serial5.readString().indexOf("OK") >= 0) {
                    found = true;
                    is_clone = true;
                }
            }
            
            if (found) {
                Serial.print("Success! HM-10 detected at "); Serial.println(test_baud);
                
                // Clear the error message if it was shown
                indexedLayer.fillScreen(0);
                indexedLayer.swapBuffers();
                
                if (test_baud == 115200) {
                    Serial.println("Already at target 115200 baud.");
                    current_ble_baud = 115200;
                    return;
                }
                
                Serial.println("Upgrading to 115200 baud...");
                if (is_clone) {
                    Serial5.print("AT+BAUD8\r\n");
                    delay(100); Serial.println(Serial5.readString());
                    Serial5.print("AT+RESET\r\n");
                } else {
                    Serial5.print("AT+BAUD4");
                    delay(100); Serial.println(Serial5.readString());
                    Serial5.print("AT+RESET");
                }
                
                Serial.println("Waiting 1.5s for reboot...");
                delay(1500);
                
                Serial5.begin(115200);
                while(Serial5.available()) Serial5.read();
                Serial5.print(is_clone ? "AT\r\n" : "AT");
                delay(100);
                
                if (Serial5.readString().indexOf("OK") >= 0) {
                    Serial.println("Upgrade to 115200 SUCCESSFUL!");
                    current_ble_baud = 115200;
                    return;
                } else {
                    Serial.println("Upgrade FAILED! Reverting to 9600...");
                    Serial5.begin(9600);
                    current_ble_baud = 9600;
                    return;
                }
            }
        }
        
        Serial.println("Could not auto-negotiate HM-10. Is your phone connected? Retrying in 3 seconds...");
        
        // Display an error on the LED matrix so the user knows why it's stuck!
        indexedLayer.fillScreen(0);
        indexedLayer.setFont(font3x5);
        indexedLayer.drawString(0, 15, 1, "BLE ERROR");
        indexedLayer.drawString(0, 25, 1, "Phone connected?");
        indexedLayer.drawString(0, 35, 1, "Please disconnect");
        indexedLayer.drawString(0, 45, 1, "app to setup.");
        indexedLayer.swapBuffers();
        
        delay(3000);
    }
}

// --- Text Display Logic ---
struct TextCell {
    char c;
    rgb24 color;
};

static uint8_t text_font_id = 0;
static rgb24 text_bg;
static uint8_t text_rows = 0;
static uint16_t text_cols = 0;
static TextCell text_grid[20000];
static bool text_loaded = false;
static unsigned long last_text_draw = 0;

static bool text_scroll_enabled = false;
static uint8_t text_gap = 0;
static uint8_t text_max_len = 0;
static unsigned long text_scroll_tick = 0;

void textInit(bool sd_available) {
    text_loaded = false;
    text_bg = COLOR_BLACK;
    
    if (!sd_available) return;
    
    File f = SD.open("/gifs/txt.bin", FILE_READ);
    if (!f) return;
    
    if (f.available() >= 9) {
        text_font_id = f.read();
        text_bg.red = f.read();
        text_bg.green = f.read();
        text_bg.blue = f.read();
        text_rows = f.read();
        uint8_t cols_low = f.read();
        uint8_t cols_high = f.read();
        text_cols = (cols_high << 8) | cols_low;
        
        text_scroll_enabled = f.read() == 1;
        text_gap = f.read();
        
        int total_cells = text_rows * text_cols;
        if (total_cells > 20000) total_cells = 20000; // safety
        
        for (int i = 0; i < total_cells; i++) {
            if (f.available() >= 4) {
                text_grid[i].c = (char)f.read();
                text_grid[i].color.red = f.read();
                text_grid[i].color.green = f.read();
                text_grid[i].color.blue = f.read();
            } else {
                text_grid[i].c = ' ';
                text_grid[i].color = COLOR_BLACK;
            }
        }
        
        int max_len = 0;
        for (int r = 0; r < text_rows; r++) {
            int row_len = 0;
            for (int c = text_cols - 1; c >= 0; c--) {
                if (text_grid[r * text_cols + c].c != ' ') {
                    row_len = c + 1;
                    break;
                }
            }
            if (row_len > max_len) max_len = row_len;
        }
        text_max_len = max_len;
        text_scroll_tick = 0;
        
        text_loaded = true;
    }
    f.close();
    
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    last_text_draw = 0; // force immediate draw
}

void textLoop(unsigned long now) {
    if (!text_loaded) return;
    
    if (text_scroll_enabled) {
        if (now - last_text_draw < 40) return; // scroll speed
        last_text_draw = now;
        text_scroll_tick++;
    } else {
        if (now - last_text_draw < 1000) return;
        last_text_draw = now;
    }
    
    backgroundLayer.fillScreen(text_bg);
    
    int charWidth, charHeight;
    const GFXfont* gfxFont = nullptr;
    fontChoices font;
    
    if (text_font_id < 6) {
        if (text_font_id == 0) { font = font3x5; charWidth = 4; charHeight = 6; }
        else if (text_font_id == 1) { font = font5x7; charWidth = 6; charHeight = 8; }
        else if (text_font_id == 2) { font = font6x10; charWidth = 6; charHeight = 10; }
        else if (text_font_id == 3) { font = gohufont11; charWidth = 6; charHeight = 11; }
        else if (text_font_id == 4) { font = gohufont11b; charWidth = 6; charHeight = 11; }
        else { font = font8x13; charWidth = 9; charHeight = 14; }
        backgroundLayer.setFont(font);
    } else {
        if (text_font_id == 6) { gfxFont = &FreeMono9pt7b; charWidth = 11; charHeight = 18; }
        else if (text_font_id == 7) { gfxFont = &FreeMono12pt7b; charWidth = 14; charHeight = 24; }
        else if (text_font_id == 8) { gfxFont = &FreeMono18pt7b; charWidth = 21; charHeight = 35; }
        else { gfxFont = &FreeMono24pt7b; charWidth = 28; charHeight = 47; }
        backgroundLayer.setFont(gfxFont);
    }
    
    int wrap_width_chars = text_max_len + text_gap;
    if (wrap_width_chars == 0) wrap_width_chars = 1;
    int wrap_width_pixels = wrap_width_chars * charWidth;
    
    backgroundLayer.setTextWrap(false);
    
    int scroll_offset = text_scroll_enabled ? -(text_scroll_tick % wrap_width_pixels) : 0;
    int reps = text_scroll_enabled ? (64 / wrap_width_pixels + 2) : 1;
    
    for (int rep = 0; rep < reps; rep++) {
        int current_x_offset = scroll_offset + rep * wrap_width_pixels;
        
        int idx = 0;
        for (int r = 0; r < text_rows; r++) {
            for (int c = 0; c < text_cols; c++) {
                if (!text_scroll_enabled || c < text_max_len) {
                    int x = current_x_offset + c * charWidth;
                    
                    if (x > -charWidth && x < 64) {
                        char ch[2] = { text_grid[idx].c, 0 };
                        if (text_font_id < 6) {
                            backgroundLayer.drawString(x, r * charHeight, text_grid[idx].color, ch);
                        } else {
                            uint16_t c565 = backgroundLayer.color565(text_grid[idx].color.red, text_grid[idx].color.green, text_grid[idx].color.blue);
                            backgroundLayer.setTextColor(c565);
                            int yOffset = (text_font_id == 6) ? 13 : (text_font_id == 7) ? 18 : (text_font_id == 8) ? 26 : 35;
                            backgroundLayer.setCursor(x, r * charHeight + yOffset);
                            backgroundLayer.print(ch);
                        }
                    }
                }
                idx++;
                if (idx >= 20000) break;
            }
            if (idx >= 20000) break;
        }
    }
    
    backgroundLayer.swapBuffers();
}

// --- Layer Mixer Logic ---
void layerLoop(unsigned long now) {
    // 1. Draw Background
    if (layer_bg_idx == 0) { // None
        backgroundLayer.fillScreen(COLOR_BLACK);
    } else if (layer_bg_idx == 1) { // Solid Black
        backgroundLayer.fillScreen(COLOR_BLACK);
    } else if (layer_bg_idx == 2) { // Solid Red
        backgroundLayer.fillScreen({255, 0, 0});
    } else if (layer_bg_idx == 3) { // Plasma
        visDrawBackground(3, now); // Draws directly to backgroundLayer back-buffer
    } else if (layer_bg_idx == 4) { // Starfield
        visDrawBackground(4, now);
    } else {
        backgroundLayer.fillScreen(COLOR_BLACK);
    }

    // 2. Draw Animation Layer
    if (layer_anim_idx >= 0) {
        if (layer_anim_idx < 7) {
            // Generative visualizers expect a fresh screen every frame
            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 64; x++) {
                    rgb24 tmp = {1, 1, 1};
                    layer_gif_buffer[y][x] = tmp;
                }
            }
            // Generative Visualizer
            visDrawAnimation(layer_anim_idx, now);
        } else {
            // GIF
            // Step the GIF decoder (it will write to layer_gif_buffer internally without swapping)
            if (use_sd) {
                drawImageWithSD(now);
            } else {
                displayGIFFromMemoryById(0, now);
            }
        }
        
        // Composite the gif_buffer over the background
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                rgb24 c = layer_gif_buffer[y][x];
                if (!(c.red == 1 && c.green == 1 && c.blue == 1)) { // If not transparent
                    backgroundLayer.drawPixel(x, y, c);
                }
            }
        }
    }

    // 3. Draw Foreground
    if (layer_fg_idx == 1) { // Border
        backgroundLayer.drawRectangle(0, 0, 63, 63, COLOR_WHITE);
        backgroundLayer.drawRectangle(1, 1, 62, 62, COLOR_WHITE);
    } else if (layer_fg_idx == 2) { // Vignette
        // Simple vignette: dim the corners
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                float dx = x - 31.5f;
                float dy = y - 31.5f;
                float dist = sqrt(dx*dx + dy*dy);
                if (dist > 28.0f) {
                    if (dist > 35.0f) {
                        backgroundLayer.drawPixel(x, y, COLOR_BLACK);
                    }
                }
            }
        }
    }

    // 4. Draw Text Layer
    if (layer_txt_idx == 1) { // Hello World
        backgroundLayer.setFont(font3x5);
        backgroundLayer.drawString(10, 28, COLOR_WHITE, "HELLO");
        backgroundLayer.drawString(10, 36, COLOR_WHITE, "WORLD");
    } else if (layer_txt_idx == 2) {
        backgroundLayer.setFont(font3x5);
        backgroundLayer.drawString(2, 28, COLOR_WHITE, "BONNAROO");
    }

    // Finally, swap buffers to push the composited frame to the matrix!
    backgroundLayer.swapBuffers();
}

// Setup method runs once, when the sketch starts
void setup() {
    matrix.setRotation(rotation270);
    // ----------------------------------------------
    // ---------- GIF Decorder Setup ----------------
    // ----------------------------------------------
    decoder.setScreenClearCallback(screenClearCallback);
    decoder.setUpdateScreenCallback(updateScreenCallback);
    decoder.setDrawPixelCallback(drawPixelCallback);

    decoder.setFileSeekCallback(fileSeekCallback);
    decoder.setFilePositionCallback(filePositionCallback);
    decoder.setFileReadCallback(fileReadCallback);
    decoder.setFileReadBlockCallback(fileReadBlockCallback);
    
    // NOTE: new callback function required after we moved to using the external AnimatedGIF library to decode GIFs
    decoder.setFileSizeCallback(fileSizeCallback);

    matrix.addLayer(&backgroundLayer); 
    matrix.addLayer(&indexedLayer); 
    matrix.addLayer(&scrollingLayer);

    matrix.setBrightness(brightness);
    matrix.begin();

    backgroundLayer.enableColorCorrection(true);
    indexedLayer.enableColorCorrection(true);

    // USB communication
    Serial.begin(115200);

    // BluetoothLE communication - auto upgrade to 115200
    pinMode(BLUEFRUIT_CTS_PIN, OUTPUT);
    digitalWrite(BLUEFRUIT_CTS_PIN, LOW); // Hardwire CTS to GND via pin 22
    pinMode(BLUEFRUIT_RTS_PIN, INPUT_PULLDOWN); // Use PULLDOWN (Adafruit uses push-pull 3.3V, so this safely defaults to 'ready' if unplugged)

    autoNegotiateBaudRate();
    
    // Automatically configure Bluefruit Flow Control if MOD pin is connected
    if (BLUEFRUIT_MOD_PIN >= 0) {
        Serial.println("Configuring final Bluefruit LE settings...");
        pinMode(BLUEFRUIT_MOD_PIN, OUTPUT);
        digitalWrite(BLUEFRUIT_MOD_PIN, HIGH); // Pull MOD high to enter CMD mode
        delay(500); // Wait for mode switch
        
        while(Serial5.available()) Serial5.read(); // Clear buffer
        
        Serial.println("1/4: Ensuring Hardware Flow Control is ON...");
        Serial5.println("AT+UARTFLOW=on");
        delay(250);
        while(Serial5.available()) Serial.write(Serial5.read());
        
        Serial.println("2/4: Setting Antenna Power to +4 dBm...");
        Serial5.println("AT+BLEPOWERLEVEL=4");
        delay(250);
        while(Serial5.available()) Serial.write(Serial5.read());
        
        Serial.println("3/4: Renaming device to 'shhhhhhh...'...");
        Serial5.println("AT+GAPDEVNAME=shhhhhhh...");
        delay(250);
        while(Serial5.available()) Serial.write(Serial5.read());
        
        Serial.println("4/4: Disabling +++ mode switch vulnerability...");
        Serial5.println("AT+MODESWITCHEN=off");
        delay(250);
        while(Serial5.available()) Serial.write(Serial5.read());

        Serial.println("Software rebooting Bluefruit to apply name change...");
        Serial5.println("ATZ");
        delay(1500); // Takes a second to reboot
        
        digitalWrite(BLUEFRUIT_MOD_PIN, LOW); // Pull MOD low to return to DATA mode
        delay(500); // Wait for mode switch
    }

    // give time for USB Serial to be ready
    delay(1000);

    // Clear screen
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();

    scrollingLayer.setMode(wrapForward);
    scrollingLayer.setColor({0xff, 0xff, 0xff});

    // Set large font to read
    scrollingLayer.setFont(font3x5);

    unsigned long now = millis();
    writeDebugScreen("POWER: ON", now);


    // ----------------------------------------------
    // ---------- SD Card Setup  --------------------
    // ----------------------------------------------
    if (use_sd) {
        if(!initSDCard(SD_CS, use_spi1)) {
            scrollingLayer.start("No SD card", -1);
            Serial.println("No SD card");
            while(1);
        }
    }

    // Determine how many animated GIF files exist
    num_files = wrap_enumerateGIFFiles(GIF_DIRECTORY, true);

    if(num_files < 0) {
        writeDebugScreen("No gifs directory", now);
        Serial.println("No gifs directory");
        while(1);
    }

    if(!num_files) {
        writeDebugScreen("Empty gifs directory", now);
        Serial.println("Empty gifs directory");
        while(1);
    }

    if (use_sd) {
        char buf[60];
        buf[0] = 0;
        strcat(buf, "Found ");
        strcat(buf, String(num_files).c_str());
        writeDebugScreen(buf, now);
    }

    // ----------------------------------------------
    // ---------- IR Receiver Setup  ----------------
    // ----------------------------------------------
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK); // Start the receiver
    
    lbInit(use_sd);
}


void loop() {
    unsigned long now = millis();

    maybeClearDebugScreen(now);

    HandleIRInputs(now);

    HandleBLEInputs(now);

    if (current_mode == MODE_SNAKE) {
        snakeLoop(now);
    } else if (current_mode == MODE_PACMAN) {
        pacmanLoop(now);
    } else if (current_mode == MODE_FROGGER) {
        froggerLoop(now);
    } else if (current_mode == MODE_TETRIS) {
        tetrisLoop(now);
    } else if (current_mode == MODE_VISUALIZATIONS) {
        visLoop(now);
    } else if (current_mode == MODE_TEXT) {
        textLoop(now);
    } else if (current_mode == MODE_LAYER) {
        // Pause layer rendering if uploading to prevent file access collisions
        if (!is_uploading_ble) {
            layerLoop(now);
        }
    } else {
        if (!use_sd) {
            drawImageNoSD(now);
        } else {
            // Pause GIF decoding during upload to prevent file access collisions
            if (!is_uploading_ble) {
                drawImageWithSD(now);
            }
        }
    }
    is_first_frame = false;
}

