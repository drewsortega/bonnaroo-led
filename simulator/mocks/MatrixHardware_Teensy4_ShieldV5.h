#/*
 * SmartMatrix Library - Hardware-Specific Header File (Shim for Simulator)
 */

#ifndef MATRIX_HARDWARE_H
#define MATRIX_HARDWARE_H

#pragma message "MatrixHardware: Simulator Shim"

// Constants needed by Bonnaroo.ino
#define SMARTMATRIX_OPTIONS_NONE 0
#define SMARTMATRIX_OPTIONS_C_SHAPE_STACKING 0
#define SM_PANELTYPE_HUB75_32ROW_MOD16SCAN 0
#define SM_BACKGROUND_OPTIONS_NONE 0
#define SM_SCROLLING_OPTIONS_NONE 0
#define SM_INDEXED_OPTIONS_NONE 0

// Hardware constants (copied from Teesy4 V5 header)
#define FLEXPWM_PIN_OE_TEENSY_PIN       2
#define FLEXPWM_PIN_LATCH_TEENSY_PIN    3
#define FLEXIO_PIN_CLK_TEENSY_PIN       7
#define FLEXIO_PIN_B0_TEENSY_PIN        10
#define FLEXIO_PIN_R0_TEENSY_PIN        6
#define FLEXIO_PIN_R1_TEENSY_PIN        12
#define FLEXIO_PIN_G0_TEENSY_PIN        9
#define FLEXIO_PIN_G1_TEENSY_PIN        11
#define FLEXIO_PIN_B1_TEENSY_PIN        13

// Simulator dependencies
#include <vector>
#include <SDL2/SDL.h>
#include "Arduino.h"

// Pre-include our mocked Layer.h to satisfy dependencies and inject assertions
// This prevents the real libraries/SmartMatrix/src/Layer.h from being included
#include "Layer.h"

// Forward define generic Layer class if not available yet (it will be from SmartMatrix.h)
// But we can use void* or basic type for the shim interface
// class SM_Layer; // No longer needed as we included Layer.h

// Shim class that replaces the real SmartMatrix class
class SmartMatrixShim {
private:
    std::vector<SM_Layer*> layers;
    uint8_t brightness = 255;
    
public:
    SmartMatrixShim(int bufferRows, void* data) {
        (void)bufferRows; (void)data;
    }
    
    void addLayer(SM_Layer* layer) {
        layers.push_back(layer);
    }
    
    void begin() {
        for (auto* layer : layers) {
            layer->begin();
        }
    }
    
    void setBrightness(uint8_t newBrightness) {
        brightness = newBrightness;
    }
    
    void setRefreshRate(uint8_t rate) {
        (void)rate;
    }
    
    // Simulator specific update function
    void updateSimulator(SDL_Renderer* renderer);
    
    // Rotation support
    rotationDegrees rotation = rotation0;
    void setRotation(rotationDegrees newRotation) {
        rotation = newRotation;
    }
};

// Define allocation macros to use REAL layers but our Shim matrix
// These mimic the ones in SmartMatrix.h but are tailored for the simulator

// Main Buffer Allocation - Instantiates our Shim
#define SMARTMATRIX_ALLOCATE_BUFFERS(matrix_name, width, height, pwm_depth, buffer_rows, panel_type, option_flags) \
    SmartMatrixShim matrix_name(buffer_rows, nullptr);

// Background Layer - Uses real SMLayerBackground
#define SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(layer_name, width, height, storage_depth, background_options) \
    typedef RGB_TYPE(storage_depth) SM_RGB; \
    static RGB_TYPE(storage_depth) layer_name##Bitmap[2*width*height]; \
    static color_chan_t layer_name##colorCorrectionLUT[sizeof(SM_RGB) <= 3 ? 256 : 4096]; \
    static SMLayerBackground<RGB_TYPE(storage_depth), background_options> layer_name(layer_name##Bitmap, width, height, layer_name##colorCorrectionLUT)

// Scrolling Layer - Uses real SMLayerScrolling
#define SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(layer_name, width, height, storage_depth, scrolling_options) \
    typedef RGB_TYPE(storage_depth) SM_RGB; \
    static uint8_t layer_name##Bitmap[width * (height / 8)]; \
    static SMLayerScrolling<RGB_TYPE(storage_depth), scrolling_options> layer_name(layer_name##Bitmap, width, height)

// Indexed Layer - Uses real SMLayerIndexed
#define SMARTMATRIX_ALLOCATE_INDEXED_LAYER(layer_name, width, height, storage_depth, indexed_options) \
    typedef RGB_TYPE(storage_depth) SM_RGB; \
    static uint8_t layer_name##Bitmap[2 * width * (height / 8)]; \
    static SMLayerIndexed<RGB_TYPE(storage_depth), indexed_options> layer_name(layer_name##Bitmap, width, height)

#endif // MATRIX_HARDWARE_H
