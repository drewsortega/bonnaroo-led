#ifndef GIFDECODER_H
#define GIFDECODER_H

/**
 * GifDecoder.h Mock for LED Grid Simulator
 * 
 * Provides the GifDecoder API using stb_image for GIF decoding.
 */

#include "Arduino.h"
#include <cstdint>
#include <cstring>
#include <functional>

// Define the callbacks as function pointer types
typedef void (*ScreenClearCallback)();
typedef void (*UpdateScreenCallback)();
typedef void (*DrawPixelCallback)(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b);
typedef bool (*FileSeekCallback)(unsigned long position);
typedef unsigned long (*FilePositionCallback)();
typedef int (*FileReadCallback)();
typedef int (*FileReadBlockCallback)(void* buffer, int numberOfBytes);
typedef int (*FileSizeCallback)();

// Include stb_image implementation (defined in main.cpp)
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_DECLARATION
#endif

// Forward declare stb functions
extern "C" {
    unsigned char* stbi_load_gif_from_memory(unsigned char const *buffer, int len, 
                                              int **delays, int *x, int *y, int *z, 
                                              int *comp, int req_comp);
    void stbi_image_free(void *retval_from_stbi_load);
}

template<int maxWidth, int maxHeight, int lzwMaxBits>
class GifDecoder {
private:
    // Callbacks
    ScreenClearCallback screenClearCallback;
    UpdateScreenCallback updateScreenCallback;
    DrawPixelCallback drawPixelCallback;
    FileSeekCallback fileSeekCallback;
    FilePositionCallback filePositionCallback;
    FileReadCallback fileReadCallback;
    FileReadBlockCallback fileReadBlockCallback;
    FileSizeCallback fileSizeCallback;
    
    // GIF data
    unsigned char* gifData;
    int gifDataSize;
    unsigned char* frames;
    int* frameDelays;
    int frameWidth;
    int frameHeight;
    int frameCount;
    int currentFrame;
    int currentDelay;
    
    bool fromMemory;
    const uint8_t* memoryBuffer;
    int memorySize;
    
public:
    GifDecoder() : 
        screenClearCallback(nullptr),
        updateScreenCallback(nullptr),
        drawPixelCallback(nullptr),
        fileSeekCallback(nullptr),
        filePositionCallback(nullptr),
        fileReadCallback(nullptr),
        fileReadBlockCallback(nullptr),
        fileSizeCallback(nullptr),
        gifData(nullptr),
        gifDataSize(0),
        frames(nullptr),
        frameDelays(nullptr),
        frameWidth(0),
        frameHeight(0),
        frameCount(0),
        currentFrame(0),
        currentDelay(100),
        fromMemory(false),
        memoryBuffer(nullptr),
        memorySize(0)
    {}
    
    ~GifDecoder() {
        cleanup();
    }
    
    void cleanup() {
        if (frames) {
            stbi_image_free(frames);
            frames = nullptr;
        }
        if (frameDelays) {
            stbi_image_free(frameDelays);
            frameDelays = nullptr;
        }
        if (gifData) {
            delete[] gifData;
            gifData = nullptr;
        }
    }
    
    // Callback setters
    void setScreenClearCallback(ScreenClearCallback cb) { screenClearCallback = cb; }
    void setUpdateScreenCallback(UpdateScreenCallback cb) { updateScreenCallback = cb; }
    void setDrawPixelCallback(DrawPixelCallback cb) { drawPixelCallback = cb; }
    void setFileSeekCallback(FileSeekCallback cb) { fileSeekCallback = cb; }
    void setFilePositionCallback(FilePositionCallback cb) { filePositionCallback = cb; }
    void setFileReadCallback(FileReadCallback cb) { fileReadCallback = cb; }
    void setFileReadBlockCallback(FileReadBlockCallback cb) { fileReadBlockCallback = cb; }
    void setFileSizeCallback(FileSizeCallback cb) { fileSizeCallback = cb; }
    
    // Start decoding from file callbacks
    int startDecoding() {
        cleanup();
        
        if (!fileSizeCallback || !fileSeekCallback || !fileReadBlockCallback) {
            printf("[GifDecoder] Error: File callbacks not set\n");
            return -1;
        }
        
        // Get file size
        gifDataSize = fileSizeCallback();
        printf("[GifDecoder] File size: %d bytes\n", gifDataSize);
        if (gifDataSize <= 0) {
            printf("[GifDecoder] Error: Invalid file size\n");
            return -1;
        }
        
        // Read entire file into memory
        gifData = new unsigned char[gifDataSize];
        fileSeekCallback(0);
        int bytesRead = fileReadBlockCallback(gifData, gifDataSize);
        printf("[GifDecoder] Read %d bytes\n", bytesRead);
        if (bytesRead != gifDataSize) {
            printf("[GifDecoder] Error: Could only read %d of %d bytes\n", bytesRead, gifDataSize);
            delete[] gifData;
            gifData = nullptr;
            return -1;
        }
        
        // Decode GIF
        int comp;
        frames = stbi_load_gif_from_memory(gifData, gifDataSize, &frameDelays, 
                                            &frameWidth, &frameHeight, &frameCount, 
                                            &comp, 4);  // Request RGBA
        
        if (!frames) {
            printf("[GifDecoder] Error: Failed to decode GIF (stb_image error)\n");
            delete[] gifData;
            gifData = nullptr;
            return -1;
        }
        
        currentFrame = 0;
        printf("[GifDecoder] Loaded GIF: %dx%d, %d frames\n", frameWidth, frameHeight, frameCount);
        
        return 0;
    }
    
    // Start decoding from memory buffer
    int startDecoding(uint8_t* buffer, int bufferSize) {
        cleanup();
        
        memoryBuffer = buffer;
        memorySize = bufferSize;
        fromMemory = true;
        
        // Decode GIF from memory
        int comp;
        frames = stbi_load_gif_from_memory(buffer, bufferSize, &frameDelays,
                                            &frameWidth, &frameHeight, &frameCount,
                                            &comp, 4);
        
        if (!frames) {
            printf("[GifDecoder] Error: Failed to decode GIF from memory\n");
            return -1;
        }
        
        currentFrame = 0;
        printf("[GifDecoder] Loaded GIF from memory: %dx%d, %d frames\n", 
               frameWidth, frameHeight, frameCount);
        
        return 0;
    }
    
    // Decode and display current frame
    int decodeFrame(bool delayAfterDecode = false) {
        if (!frames || frameCount == 0) {
            return -1;
        }
        
        // Clear screen
        if (screenClearCallback) {
            screenClearCallback();
        }
        
        // Calculate scale to fit in maxWidth x maxHeight
        float scaleX = (float)maxWidth / frameWidth;
        float scaleY = (float)maxHeight / frameHeight;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        if (scale > 1.0f) scale = 1.0f;  // Don't scale up
        
        int scaledWidth = (int)(frameWidth * scale);
        int scaledHeight = (int)(frameHeight * scale);
        int offsetX = (maxWidth - scaledWidth) / 2;
        int offsetY = (maxHeight - scaledHeight) / 2;
        
        // Get current frame data
        unsigned char* frameData = frames + (currentFrame * frameWidth * frameHeight * 4);
        
        // Draw pixels
        if (drawPixelCallback) {
            for (int y = 0; y < maxHeight; y++) {
                for (int x = 0; x < maxWidth; x++) {
                    int srcX = (int)((x - offsetX) / scale);
                    int srcY = (int)((y - offsetY) / scale);
                    
                    if (srcX >= 0 && srcX < frameWidth && srcY >= 0 && srcY < frameHeight) {
                        int idx = (srcY * frameWidth + srcX) * 4;
                        uint8_t r = frameData[idx + 0];
                        uint8_t g = frameData[idx + 1];
                        uint8_t b = frameData[idx + 2];
                        uint8_t a = frameData[idx + 3];
                        
                        // Only draw non-transparent pixels
                        if (a > 128) {
                            drawPixelCallback(x, y, r, g, b);
                        }
                    }
                }
            }
        }
        
        // Update screen
        if (updateScreenCallback) {
            updateScreenCallback();
        }
        
        // Get delay for this frame (stb_image returns delay in milliseconds)
        if (frameDelays && currentFrame < frameCount) {
            currentDelay = frameDelays[currentFrame];
            if (currentDelay < 10) currentDelay = 100;  // Default if not specified
        }
        
        // Advance to next frame
        currentFrame++;
        if (currentFrame >= frameCount) {
            currentFrame = 0;
            return -1;  // Signal animation loop complete
        }
        
        if (delayAfterDecode) {
            delay(currentDelay);
        }
        
        return 0;
    }
    
    int getFrameDelay_ms() const {
        return currentDelay;
    }
    
    int getWidth() const { return frameWidth; }
    int getHeight() const { return frameHeight; }
    int getFrameCount() const { return frameCount; }
};

// GIFIMAGE structure (not used in this simulator but needed for compilation)
struct GIFIMAGE {
    int x, y;
};

#endif // GIFDECODER_H
