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
#include "mocks/SmartMatrix.h"
#include "mocks/IRremote.hpp"
#include "mocks/GifDecoder.h"

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

/**
 * Render the LED grid to the SDL window.
 */
void renderDisplay() {
    // Clear with dark gray (simulates LED panel background)
    SDL_SetRenderDrawColor(g_renderer, 20, 20, 20, 255);
    SDL_RenderClear(g_renderer);
    
    int ledSize = g_scale - g_gap;
    float brightnessFactor = g_brightness / 255.0f;
    
    // Apply rotation
    int width = g_matrix_width;
    int height = g_matrix_height;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Apply rotation when reading from buffer
            int srcX = x, srcY = y;
            switch (g_rotation) {
                case rotation90:
                    srcX = height - 1 - y;
                    srcY = x;
                    break;
                case rotation180:
                    srcX = width - 1 - x;
                    srcY = height - 1 - y;
                    break;
                case rotation270:
                    srcX = y;
                    srcY = width - 1 - x;
                    break;
                default:
                    break;
            }
            
            int idx = (srcY * width + srcX) * 3;
            
            // Apply brightness
            uint8_t r = (uint8_t)(g_display_buffer[idx + 0] * brightnessFactor);
            uint8_t g = (uint8_t)(g_display_buffer[idx + 1] * brightnessFactor);
            uint8_t b = (uint8_t)(g_display_buffer[idx + 2] * brightnessFactor);
            
            SDL_SetRenderDrawColor(g_renderer, r, g, b, 255);
            
            SDL_Rect rect = {
                x * g_scale + g_gap / 2,
                y * g_scale + g_gap / 2,
                ledSize,
                ledSize
            };
            
            SDL_RenderFillRect(g_renderer, &rect);
        }
    }
    
    // Render scrolling text overlay if active
    if (scrollingLayer.isActive() && g_font) {
        unsigned long elapsed = millis() - scrollingLayer.getStartTime();
        
        const char* text = scrollingLayer.getText();
        if (text && text[0] != '\0') {
            // Render text to surface
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* textSurface = TTF_RenderText_Solid(g_font, text, white);
            
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(g_renderer, textSurface);
                
                if (textTexture) {
                    // Draw black background bar
                    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
                    SDL_Rect bgRect = {0, 0, g_matrix_width * g_scale, 20};
                    SDL_RenderFillRect(g_renderer, &bgRect);
                    
                    // Calculate scroll position
                    int scrollOffset = (int)(elapsed * 0.08);  // Scroll speed
                    int textX = g_matrix_width * g_scale - scrollOffset;
                    
                    // Draw text at scrolling position
                    SDL_Rect textRect = {textX, 2, textSurface->w, textSurface->h};
                    SDL_RenderCopy(g_renderer, textTexture, NULL, &textRect);
                    
                    SDL_DestroyTexture(textTexture);
                }
                SDL_FreeSurface(textSurface);
            }
        }
        
        // Hide text after 3 seconds
        if (elapsed > 3000) {
            scrollingLayer.stop();
        }
    } else if (scrollingLayer.isActive()) {
        // No font available, just time out
        if (millis() - scrollingLayer.getStartTime() > 3000) {
            scrollingLayer.stop();
        }
    }
    
    SDL_RenderPresent(g_renderer);
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
    
    printf("[Simulator] Starting Arduino setup()...\n");
    
    // Call Arduino setup
    setup();
    
    printf("[Simulator] Entering main loop...\n");
    
    // Main loop
    while (g_running) {
        // Check for quit via Q key or window close
        SDL_PumpEvents();
        if (checkQuit()) {
            g_running = false;
            break;
        }
        
        // Call Arduino loop
        loop();
        
        // Render the display
        renderDisplay();
        
        // Check for special quit code from IR receiver
        if (IrReceiver.decodedIRData.decodedRawData == 0xFFFFFFFF) {
            g_running = false;
        }
        
        // Small delay to prevent CPU spinning
        SDL_Delay(1);
    }
    
    // Cleanup
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();
    
    printf("[Simulator] Goodbye!\n");
    return 0;
}
