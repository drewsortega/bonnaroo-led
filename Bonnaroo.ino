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

#include <GifDecoder.h>
#include <IRremote.hpp>
#include <MatrixHardware_Teensy4_ShieldV5.h>        // SmartLED Shield for Teensy 4 (V5)
#include <SD.h>
#include <SPI.h>
#include <SmartMatrix.h>

#include "FilenameFunctions.h"

#define DISPLAY_TIME_SECONDS 10
#define NUMBER_FULL_CYCLES   100

// Teensy 4.0 using CS0.
// If use_sd == false, SD is not read and colors stand in for images.
#define SD_CS 0
#define SD_SCK 27
#define SD_MOSI 26
#define SD_MISO 1

const bool use_sd = false;
// Teensy 4.1 with builtin
// #define SD_CS BUILTIN_SDCARD

// Teensy SD Library requires a trailing slash in the directory name
#define GIF_DIRECTORY "/gifs/"

// Data pin the IR receiver is hooked up to.
#define IR_RECEIVE_PIN 16

// range 0-255 technically, but battery drives less than that.
const int max_brightness = 180;
static int brightness = max_brightness;

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

Sd2Card card;
SdVolume volume;
SdFile root;


void screenClearCallback(void) {
  backgroundLayer.fillScreen({0,0,0});
}

void updateScreenCallback(void) {
  backgroundLayer.swapBuffers();
}

void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue) {
    backgroundLayer.drawPixel(x, y, {red, green, blue});
}

int wrap_initFileSystem(int chipSelectPin) {
    if (use_sd) {
        return initFileSystem(chipSelectPin);
    }
    return 0;
}

int wrap_enumerateGIFFiles(const char *directoryName, bool displayFilenames) {
    if (use_sd) {
        return wrap_enumerateGIFFiles(directoryName, displayFilenames);
    }
    return 5;
}

// Remote Layout:
//
// BUT_VOL_DOWN     BUT_PLAY        BUT_VOL_UP
// BUT_SETUP        BUT_UP          BUT_STOP
// BUT_LEFT         BUT_ENTER       BUT_RIGHT
// BUT_0            BUT_DOWN        BUT_BACK
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
void writeDebugScreen(char text[], unsigned long now, bool allow_clear = true) {
    allow_debug_clear = allow_clear;
    indexedLayer.fillScreen(0);
    indexedLayer.setIndexedColor(1, COLOR_BLACK);
    for(int row=0; row<12; row++) {
        for(int col=0; col<(strlen(text) * 6) + 2; col++) {
            indexedLayer.drawPixel(col,row,1);
        }
    }
    indexedLayer.swapBuffers();
    scrollingLayer.start(text, -1);

    last_debug_write_time = now;
}

bool validatePressAndGetName(IRRawDataType button, char* buf) {
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
        brightness = max(0, next_brightness);
    } else {
        brightness = min(max_brightness, next_brightness);
    }
    matrix.setBrightness(brightness);
}

int num_files = 0;
static int cur_image_idx = 0;
static int next_image_idx = 0;
void requestChangeImageIdx(int amount) {
    next_image_idx = cur_image_idx + amount;

    // Wrap around images on overflow.
    if (next_image_idx < 0) {
        next_image_idx = num_files - 1;
    } else if (next_image_idx >= num_files) {
        next_image_idx = 0;
    }
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

    IRRawDataType received_data = IrReceiver.decodedIRData.decodedRawData;
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


    switch(received_data) {
        case BUT_VOL_DOWN:
            adjustBrightness(-26);
            strcat(debug_buf, "BRT: ");
            strcat(debug_buf, String(brightness).c_str());
            break;
        case BUT_VOL_UP:
            adjustBrightness(26);
            strcat(debug_buf, "BRT: ");
            strcat(debug_buf, String(brightness).c_str());
            break;
        case BUT_LEFT:
            requestChangeImageIdx(-1);
            strcat(debug_buf, "IMG: ");
            strcat(debug_buf, String(next_image_idx).c_str());
            break;
        case BUT_RIGHT:
            requestChangeImageIdx(1);
            strcat(debug_buf, "IMG: ");
            strcat(debug_buf, String(next_image_idx).c_str());
            break;
        default:
            // Unhandled buttons just display name.
            
            strcat(debug_buf, button_name);
            break;
    }

    writeDebugScreen(debug_buf, now);
    IrReceiver.resume(); // Receive the next value
}

void changeImageNoSD(int next_image_idx) {
    switch(next_image_idx) {
        case 0:
            backgroundLayer.fillScreen(COLOR_BLACK);
            break;
        case 1:
            backgroundLayer.fillScreen(COLOR_RED);
            break;
        case 2:
            backgroundLayer.fillScreen(COLOR_BLUE);
            break;
        case 3:
            backgroundLayer.fillScreen(COLOR_GREEN);
            break;
        case 4:
            backgroundLayer.fillScreen(COLOR_WHITE);
            break;
        default:
            backgroundLayer.fillScreen(COLOR_BLACK);
    }
    backgroundLayer.swapBuffers();
    cur_image_idx = next_image_idx;
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

    Serial.begin(115200);

    // give time for USB Serial to be ready
    delay(1000);

    Serial.println("Starting AnimatedGIFs Sketch");

    matrix.addLayer(&backgroundLayer); 
    matrix.addLayer(&indexedLayer); 
    matrix.addLayer(&scrollingLayer);

    matrix.setBrightness(brightness);



    // for large panels, may want to set the refresh rate lower to leave more CPU time to decoding GIFs (needed if GIFs are playing back slowly)
    //matrix.setRefreshRate(90);
    matrix.begin();


    // Clear screen
    // TODO(drewsortega): Change this back to black.
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    scrollingLayer.setMode(stopped);
    scrollingLayer.setColor({0xff, 0xff, 0xff});

    // Set large font to read
    scrollingLayer.setFont(font6x10);

    writeDebugScreen("POWER: ON", millis());


    // ----------------------------------------------
    // ---------- SD Card Setup  --------------------
    // ----------------------------------------------
    if (use_sd) {
        SPI1.setMOSI(SD_MOSI);
        SPI1.setMISO(SD_MISO);
        SPI1.setSCK(SD_SCK);
        if(!card.init(SPI_HALF_SPEED, SD_CS)) {
            scrollingLayer.start("No SD card", -1);
            Serial.println("No SD card");
            while(1);
        }
        scrollingLayer.start("SD Card found!", -1);
        while(1);
    }

    // Determine how many animated GIF files exist
    num_files = wrap_enumerateGIFFiles(GIF_DIRECTORY, true);

    if(num_files < 0) {
        scrollingLayer.start("No gifs directory", -1);
        Serial.println("No gifs directory");
        while(1);
    }

    if(!num_files) {
        scrollingLayer.start("Empty gifs directory", -1);
        Serial.println("Empty gifs directory");
        while(1);
    }

    // ----------------------------------------------
    // ---------- IR Receiver Setup  ----------------
    // ----------------------------------------------
    IrReceiver.begin(IR_RECEIVE_PIN, DISABLE_LED_FEEDBACK); // Start the receiver
}


void loop() {
    unsigned long now = millis();

    maybeClearDebugScreen(now);

    HandleIRInputs(now);

    if (cur_image_idx != next_image_idx) {
        if (!use_sd) {
            changeImageNoSD(next_image_idx);
        } else {
            // TODO(drewortega): SD code
        }
    }
    // else if(nextGIF)  {
    //     nextGIF = 0;

    //     if (use_sd) {
    //         if (openGifFilenameByIndex(GIF_DIRECTORY, index) >= 0) {
    //             // Can clear screen for new animation here, but this might cause flicker with short animations
    //             // matrix.fillScreen(COLOR_BLACK);
    //             // matrix.swapBuffers();

    //             // start decoding, skipping to the next GIF if there's an error
    //             if(decoder.startDecoding() < 0) {
    //                 nextGIF = 1;
    //                 return;
    //             }

    //         }
    //     } else {
    //         switch(index) {
    //             case 0:
    //                 strcat(str_buf, "Zero");
    //                 break;
    //             case 1:
    //                 strcat(str_buf, "One");
    //                 break;
    //             case 2:
    //                 strcat(str_buf, "Two");
    //                 break;
    //             default:
    //                 strcat(str_buf, "Oops!");
    //         }

    //         scrollingLayer.start(str_buf, -1);
    //         // Calculate time in the future to terminate animation
    //         displayStartTime_millis = now;
    //     }

    //     // get the index for the next pass through
    //     if (++index >= num_files) {
    //         index = 0;
    //     }

    // }

    // if(use_sd && decoder.decodeFrame() < 0) {
    //     // There's an error with this GIF, go to the next one
    //     nextGIF = 1;
    // }
}
