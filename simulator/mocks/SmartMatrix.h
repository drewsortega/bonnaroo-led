#ifndef SMARTMATRIX_H
#define SMARTMATRIX_H

/**
 * SmartMatrix.h Mock for LED Grid Simulator
 * 
 * Provides the SmartMatrix API that redirects drawing calls
 * to an SDL2 display window.
 */

#include "Arduino.h"
#include <cstdint>
#include <cstring>
#include <vector>

// Forward declarations
class SMLayerBackground;
class SMLayerScrolling;
class SMLayerIndexed;

// Color type matching SmartMatrix rgb24
struct rgb24 {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    
    rgb24() : red(0), green(0), blue(0) {}
    rgb24(uint8_t r, uint8_t g, uint8_t b) : red(r), green(g), blue(b) {}
};

// Panel type constants (not used in simulator, but needed for compilation)
#define SM_PANELTYPE_HUB75_32ROW_MOD16SCAN 0

// Options constants
#define SMARTMATRIX_OPTIONS_C_SHAPE_STACKING 0
#define SM_BACKGROUND_OPTIONS_NONE 0
#define SM_SCROLLING_OPTIONS_NONE 0
#define SM_INDEXED_OPTIONS_NONE 0

// Rotation constants
enum rotationDegrees {
    rotation0 = 0,
    rotation90 = 90,
    rotation180 = 180,
    rotation270 = 270
};

// Scrolling modes
enum ScrollMode {
    wrapForward,
    wrapForwardFromLeft,
    stopped,
    off,
    bounceForward,
    bounceReverse
};

// Fonts (simplified - just need the type)
extern const void* font3x5;
extern const void* font5x7;
extern const void* font6x10;
extern const void* font8x13;

// Global display buffers (defined in sdl_display.cpp)
extern uint8_t g_display_buffer[];
extern uint8_t g_back_buffer[];
extern int g_matrix_width;
extern int g_matrix_height;
extern int g_brightness;
extern rotationDegrees g_rotation;

// Background layer for drawing images
class SMLayerBackground {
public:
    int width, height;
    
    SMLayerBackground() : width(64), height(64) {}
    
    void drawPixel(int16_t x, int16_t y, const rgb24& color) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            int idx = (y * width + x) * 3;
            g_back_buffer[idx + 0] = color.red;
            g_back_buffer[idx + 1] = color.green;
            g_back_buffer[idx + 2] = color.blue;
        }
    }
    
    void fillScreen(const rgb24& color) {
        for (int i = 0; i < width * height; i++) {
            g_back_buffer[i * 3 + 0] = color.red;
            g_back_buffer[i * 3 + 1] = color.green;
            g_back_buffer[i * 3 + 2] = color.blue;
        }
    }
    
    void swapBuffers(bool copy = true) {
        memcpy(g_display_buffer, g_back_buffer, width * height * 3);
    }
    
    void enableColorCorrection(bool enable) {
        (void)enable;
    }
};

// Scrolling layer for text
class SMLayerScrolling {
public:
    char text[256];
    rgb24 color;
    ScrollMode mode;
    const void* font;
    int scrollCount;
    bool active;
    unsigned long startTime;
    
    SMLayerScrolling() : color(255, 255, 255), mode(wrapForward), font(nullptr), 
                         scrollCount(-1), active(false), startTime(0) {
        text[0] = '\0';
    }
    
    void setColor(const rgb24& c) {
        color = c;
    }
    
    void setMode(ScrollMode m) {
        mode = m;
    }
    
    void setFont(const void* f) {
        font = f;
    }
    
    void start(const char* t, int count) {
        strncpy(text, t, sizeof(text) - 1);
        text[sizeof(text) - 1] = '\0';
        scrollCount = count;
        active = (strlen(text) > 0);
        startTime = millis();
        
        // Print debug text to console
        if (active) {
            printf("[ScrollingLayer] %s\n", text);
        }
    }
    
    void stop() {
        active = false;
        text[0] = '\0';
    }
    
    bool isActive() const {
        return active;
    }
    
    const char* getText() const {
        return text;
    }
    
    unsigned long getStartTime() const {
        return startTime;
    }
};

// Indexed layer for overlays
class SMLayerIndexed {
public:
    int width, height;
    rgb24 colors[256];
    uint8_t* indexBuffer;
    uint8_t* backBuffer;
    
    SMLayerIndexed() : width(64), height(64) {
        indexBuffer = new uint8_t[width * height]();
        backBuffer = new uint8_t[width * height]();
    }
    
    ~SMLayerIndexed() {
        delete[] indexBuffer;
        delete[] backBuffer;
    }
    
    void setIndexedColor(uint8_t index, const rgb24& color) {
        colors[index] = color;
    }
    
    void drawPixel(int16_t x, int16_t y, uint8_t index) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            backBuffer[y * width + x] = index;
        }
    }
    
    void fillScreen(uint8_t index) {
        memset(backBuffer, index, width * height);
    }
    
    void swapBuffers() {
        memcpy(indexBuffer, backBuffer, width * height);
    }
};

// Main matrix class
class SmartMatrixHub75 {
public:
    SMLayerBackground* bgLayer;
    SMLayerScrolling* scrollLayer;
    SMLayerIndexed* idxLayer;
    int brightness;
    rotationDegrees rotation;
    
    SmartMatrixHub75() : bgLayer(nullptr), scrollLayer(nullptr), idxLayer(nullptr),
                         brightness(255), rotation(rotation0) {}
    
    void addLayer(SMLayerBackground* layer) {
        bgLayer = layer;
        layer->width = g_matrix_width;
        layer->height = g_matrix_height;
    }
    
    void addLayer(SMLayerScrolling* layer) {
        scrollLayer = layer;
    }
    
    void addLayer(SMLayerIndexed* layer) {
        idxLayer = layer;
        layer->width = g_matrix_width;
        layer->height = g_matrix_height;
    }
    
    void begin() {
        printf("[SmartMatrix] Display initialized (%dx%d)\n", g_matrix_width, g_matrix_height);
    }
    
    void setBrightness(uint8_t b) {
        brightness = b;
        g_brightness = b;
    }
    
    void setRotation(rotationDegrees r) {
        rotation = r;
        g_rotation = r;
    }
    
    void setRefreshRate(uint16_t rate) {
        (void)rate; // Not used in simulator
    }
    
    uint16_t getRefreshRate() {
        return 120;
    }
    
    uint16_t countFPS() {
        return 60;
    }
};

// Declare global instances (defined in sdl_display.cpp)
extern SmartMatrixHub75 matrix;
extern SMLayerBackground backgroundLayer;
extern SMLayerScrolling scrollingLayer;
extern SMLayerIndexed indexedLayer;

// Macros to allocate layers (in simulator, these just declare globals)
#define SMARTMATRIX_ALLOCATE_BUFFERS(name, width, height, depth, rows, type, options) \
    uint8_t g_display_buffer[(width) * (height) * 3]; \
    uint8_t g_back_buffer[(width) * (height) * 3]; \
    int g_matrix_width = (width); \
    int g_matrix_height = (height); \
    int g_brightness = 255; \
    rotationDegrees g_rotation = rotation0; \
    SmartMatrixHub75 name;

#define SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(name, width, height, depth, options) \
    SMLayerBackground name;

#define SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(name, width, height, depth, options) \
    SMLayerScrolling name;

#define SMARTMATRIX_ALLOCATE_INDEXED_LAYER(name, width, height, depth, options) \
    SMLayerIndexed name;

#endif // SMARTMATRIX_H
