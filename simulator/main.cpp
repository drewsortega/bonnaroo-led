/**
 * LED Grid Simulator - Main Entry Point
 * 
 * Initializes SDL2, sets up the display, and runs the Arduino
 * setup() and loop() functions from the original code.
 */

#include <SDL2/SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <string>
#include <cstdlib>

// Include stb_image implementation
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Include mock headers
#include "mocks/Arduino.h"
#include "mocks/SPI.h"
#include "mocks/SD.h"
#include "mocks/MatrixHardware_Teensy4_ShieldV5.h"
#include "mocks/IRremote.hpp"
#include "mocks/GifDecoder.h"

// Generic SmartMatrix Layer header (from real library)
#include "Layer.h"

// Global instances for mocks
SerialClass Serial;
SPIClass SPI;
SPIClass SPI1;
SDClass SD;
IRrecv IrReceiver;

// Fonts (just need to exist, not actually used for rendering in simulator)
const void* font3x5 = nullptr;
const void* font5x7 = nullptr;
const void* font6x10 = nullptr;
const void* font8x13 = nullptr;

// SDL globals
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;
static TTF_Font* g_font = nullptr;
static int g_scale = 8;
static int g_gap = 1;
static bool g_running = true;

// Matrix dimensions (must match Bonnaroo.ino)
const int g_matrix_width = 64;
const int g_matrix_height = 64;
const rotationDegrees g_rotation = rotation0; // Uses enum from MatrixCommon.h

// Set base path for SD card
void SD_setBasePath(const std::string& path) {
    SD.setBasePath(path);
}

// Forward declarations of the Arduino functions
void setup();
void loop();

/**
 * Initialize SDL2 and create the display window.
 */
bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    int windowWidth = g_matrix_width * g_scale;
    int windowHeight = g_matrix_height * g_scale;
    
    g_window = SDL_CreateWindow(
        "LED Grid Simulator - Bonnaroo",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_SHOWN
    );
    
    if (!g_window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    
    // Initialize TTF
    if (TTF_Init() < 0) {
        printf("TTF could not initialize! TTF_Error: %s\n", TTF_GetError());
    } else {
        // Try to load a system font
        g_font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 14);
        if (!g_font) {
            g_font = TTF_OpenFont("/System/Library/Fonts/SFNSMono.ttf", 14);
        }
        if (!g_font) {
            printf("[Simulator] Warning: Could not load font, text will not display\n");
        }
    }
    
    printf("[Simulator] Window created: %dx%d (scale=%d)\n", windowWidth, windowHeight, g_scale);
    return true;
}

// External reference to the matrix instance defined in Bonnaroo.cpp
extern SmartMatrixShim matrix;

// Necessary headers for Layer interaction
#include "Layer.h"
#include <algorithm>

/**
 * Update the simulator display by asking SmartMatrix layers to render
 */
void SmartMatrixShim::updateSimulator(SDL_Renderer* renderer) {
    int width = 64;
    int height = 64;
    
    // Buffer for a single row (using rgb24 as per color depth)
    rgb24 rowBuffer[64];
    
    // Clear screen first (background)
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    // Simulate vertical sync / new frame interrupt
    // This is CRITICAL: calls handleBufferSwap() internally to clear swapPending flags
    // Without this, any code calling swapBuffers() will hang indefinitely
    for (auto* layer : layers) {
        layer->frameRefreshCallback();
    }

    static bool lutTested = false;
    if (!lutTested) {
        printf("[Simulator] Testing calculate8BitBackgroundLUT...\n");
        color_chan_t testLut[256];
        calculate8BitBackgroundLUT(testLut, 255);
        printf("[Simulator] LUT[255] for brightness 255: %d (Expected ~65535)\n", testLut[255]);
        calculate8BitBackgroundLUT(testLut, 50);
        printf("[Simulator] LUT[255] for brightness 50: %d\n", testLut[255]);
        
        printf("[Simulator] Struct Sizes: rgb24=%lu, rgb48=%lu\n", sizeof(rgb24), sizeof(rgb48));
        
        if (!layers.empty()) {
            SM_Layer* l = layers[0];
            printf("[Simulator] Layer 0 Info: Width=%d, Height=%d, Rotation=%d\n", 
                   l->getLayerWidth(), l->getLayerHeight(), l->getLayerRotation());
        }
        
        lutTested = true;
    }
    
    // Iterate over all rows
    for (int y = 0; y < height; y++) {
        // Clear the row buffer to black/transparent before rendering layers
        memset(rowBuffer, 0, sizeof(rowBuffer));
        
        // Ask each layer to fill the row
        for (auto* layer : layers) {
            layer->fillRefreshRow(y, rowBuffer);
        }
        
        // Debug: Check first row for data (once per second)
        static uint32_t lastLogTime = 0;
        uint32_t now = SDL_GetTicks();
        if (now - lastLogTime > 1000) {
            
            // Check pixel data in row 32
            int nonBlack = 0;
            rgb24 sample = {0,0,0};
            // Note: Use 32 if height > 32, else 0
            int debugRow = (height > 32) ? 32 : 0;
            
            // We must read the data AFTER fillRefreshRow has run on this row.
            // But we are in the 'y' loop. We should check ONLY when y == debugRow.
            if (y == debugRow) {
                 for(int x=0; x<width; x++) {
                    if(rowBuffer[x].red > 0 || rowBuffer[x].green > 0 || rowBuffer[x].blue > 0) {
                        nonBlack++;
                        sample = rowBuffer[x];
                    }
                }
                printf("[Simulator] Layers: %lu. Row %d Info: %d non-black pixels. Sample: %d,%d,%d. Brightness: %d\n", 
                       layers.size(), debugRow, nonBlack, sample.red, sample.green, sample.blue, brightness);
                lastLogTime = now;
            }
        }
        
        // Now draw this row to SDL
        for (int x = 0; x < width; x++) {
            rgb24& pixel = rowBuffer[x];
            
            // Apply brightness (basic scaling)
            float factor = brightness / 255.0f;
            uint8_t r = (uint8_t)(pixel.red * factor);
            uint8_t g = (uint8_t)(pixel.green * factor);
            uint8_t b = (uint8_t)(pixel.blue * factor);
            
            // Simulator rotation logic (reused from before)
            int dispX = x, dispY = y;
            // ... rotation transform if needed, but for now 1:1 map
            
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
            
            int ledSize = g_scale - g_gap;
            SDL_Rect rect = {
                dispX * g_scale + g_gap / 2,
                dispY * g_scale + g_gap / 2,
                ledSize,
                ledSize
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    
    SDL_RenderPresent(renderer);
}

// Global render function called by main loop
void renderDisplay() {
    matrix.updateSimulator(g_renderer);
}

/**
 * Check for quit event.
 */
bool checkQuit() {
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0) {
        return true;
    }
    return false;
}

/**
 * Print usage information.
 */
void printUsage(const char* programName) {
    printf("LED Grid Simulator for Teensy Arduino\n\n");
    printf("Usage: %s [options]\n\n", programName);
    printf("Options:\n");
    printf("  --help           Show this help message\n");
    printf("  --scale N        Set display scale (default: 8)\n");
    printf("  --no-gap         Disable gap between LEDs\n");
    printf("\nControls:\n");
    printf("  Left/Right       Previous/Next image\n");
    printf("  -/+              Decrease/Increase brightness\n");
    printf("  Space            Play/Pause\n");
    printf("  Q                Quit\n");
}

/**
 * Main entry point.
 */
int main(int argc, char* argv[]) {
    // Disable stdout buffering to ensure logs appear immediately
    setbuf(stdout, NULL);

    // Get base path (Arduino project directory, parent of simulator/)
    std::string basePath;
    std::string exePath = argv[0];
    
    // Find the base path - go up from build/ to simulator/ to project root
    size_t pos = exePath.rfind('/');
    if (pos != std::string::npos) {
        std::string dir = exePath.substr(0, pos);  // build/
        // Go up to simulator/
        pos = dir.rfind('/');
        if (pos != std::string::npos) {
            dir = dir.substr(0, pos);  // simulator/
            // Go up to project root
            pos = dir.rfind('/');
            if (pos != std::string::npos) {
                basePath = dir.substr(0, pos);  // Bonnaroo/
            }
        }
    }
    
    // Fallback: try going up two levels from current directory
    if (basePath.empty()) {
        basePath = "../..";
    }
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--scale" && i + 1 < argc) {
            g_scale = atoi(argv[++i]);
            if (g_scale < 1) g_scale = 1;
            if (g_scale > 20) g_scale = 20;
        } else if (arg == "--no-gap") {
            g_gap = 0;
        } else if (arg == "--base-path" && i + 1 < argc) {
            basePath = argv[++i];
        }
    }
    
    printf("[Simulator] Base path: %s\n", basePath.c_str());
    
    // Set SD card base path
    SD_setBasePath(basePath);
    
    // Initialize SDL
    if (!initSDL()) {
        return 1;
    }
    
    // Start Arduino thread
    printf("[Simulator] Starting Arduino thread...\n");
    std::thread arduinoThread([]() {
        printf("[Simulator] Starting Arduino setup()...\n");
        setup();
        printf("[Simulator] Entering main loop (thread)...\n");
        while (g_running) {
            loop();
            // Yield to prevent CPU hogging in the tight loop
            std::this_thread::yield(); 
            // Small delay to simulate partial timing? 
            // Actually delay() is mocked to sleep, so loop() shouldn't spin too fast if logic implies delays.
            // But if loop() is tight, we need to breathe.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    printf("[Simulator] Entering main render loop...\n");
    
    // Main render loop
    while (g_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
             if (event.type == SDL_QUIT) {
                g_running = false;
                break;
            }
            
            if (event.type == SDL_KEYDOWN) {
                IRRawDataType code = 0;
                
                switch (event.key.keysym.sym) {
                    // Volume/brightness controls
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:
                        code = 0xFF00BF00;  // BUT_VOL_DOWN
                        break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:
                        code = 0xFD02BF00;  // BUT_VOL_UP
                        break;
                    
                    // Navigation
                    case SDLK_LEFT:
                        code = 0xF708BF00;  // BUT_LEFT
                        break;
                    case SDLK_RIGHT:
                        code = 0xF50ABF00;  // BUT_RIGHT
                        break;
                    case SDLK_UP:
                        code = 0xFA05BF00;  // BUT_UP
                        break;
                    case SDLK_DOWN:
                        code = 0xF20DBF00;  // BUT_DOWN
                        break;
                    
                    // Action buttons
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        code = 0xF609BF00;  // BUT_ENTER
                        break;
                    case SDLK_SPACE:
                        code = 0xFE01BF00;  // BUT_PLAY
                        break;
                    case SDLK_BACKSPACE:
                    case SDLK_ESCAPE:
                        code = 0xF10EBF00;  // BUT_BACK
                        break;
                    case SDLK_s:
                        code = 0xF906BF00;  // BUT_STOP
                        break;
                    
                    // Number keys
                    case SDLK_0:
                    case SDLK_KP_0:
                        code = 0xF30CBF00;  // BUT_0
                        break;
                    case SDLK_1:
                    case SDLK_KP_1:
                        code = 0xEF10BF00;  // BUT_1
                        break;
                    case SDLK_2:
                    case SDLK_KP_2:
                        code = 0xEE11BF00;  // BUT_2
                        break;
                    case SDLK_3:
                    case SDLK_KP_3:
                        code = 0xED12BF00;  // BUT_3
                        break;
                    case SDLK_4:
                    case SDLK_KP_4:
                        code = 0xEB14BF00;  // BUT_4
                        break;
                    case SDLK_5:
                    case SDLK_KP_5:
                        code = 0xEA15BF00;  // BUT_5
                        break;
                    case SDLK_6:
                    case SDLK_KP_6:
                        code = 0xE916BF00;  // BUT_6
                        break;
                    case SDLK_7:
                    case SDLK_KP_7:
                        code = 0xE718BF00;  // BUT_7
                        break;
                    case SDLK_8:
                    case SDLK_KP_8:
                        code = 0xE619BF00;  // BUT_8
                        break;
                    case SDLK_9:
                    case SDLK_KP_9:
                        code = 0xE51ABF00;  // BUT_9
                        break;
                    
                    // Quit on Q
                    case SDLK_q:
                        g_running = false;
                        code = 0xFFFFFFFF;  // Special quit code
                        break;
                }
                
                if (code != 0) {
                    IrReceiver.injectCode(code);
                }
            }
        }
        
        // Render the display
        // This calls updateSimulator which calls frameRefreshCallback
        // This clears swapPending flags, allowing the Arduino thread to proceed past swapBuffers()
        renderDisplay();
        
        // Simulate 60FPS refresh rate (approx 16ms)
        // This controls how often we clear the swap buffers
        SDL_Delay(16);
    }
    
    // Join thread before exiting
    if (arduinoThread.joinable()) {
        arduinoThread.join();
    }
    
    // Cleanup
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
    
    printf("[Simulator] Goodbye!\n");
    return 0;
}
