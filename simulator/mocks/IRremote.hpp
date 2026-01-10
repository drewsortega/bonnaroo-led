#ifndef IRREMOTE_HPP
#define IRREMOTE_HPP

/**
 * IRremote.hpp Mock for LED Grid Simulator
 * 
 * Translates SDL keyboard events into IR button codes.
 */

#include "Arduino.h"
#include <SDL2/SDL.h>

// IR data type matching the original
typedef uint32_t IRRawDataType;

// Decoded IR data structure
struct IRData {
    IRRawDataType decodedRawData;
};

// IR Receiver class
class IRrecv {
    // Thread-safe queue for input codes
    std::vector<IRRawDataType> _inputQueue;
    std::mutex _queueMutex;
    
public:
    IRData decodedIRData;
    
    IRrecv() {
        decodedIRData.decodedRawData = 0;
    }
    
    void begin(int pin, bool enableLED = false) {
        (void)pin;
        (void)enableLED;
        printf("[IRremote] Receiver started (using keyboard input)\n");
    }
    
    // Inject a key code (called from main thread)
    void injectCode(IRRawDataType code) {
        std::lock_guard<std::mutex> lock(_queueMutex);
        _inputQueue.push_back(code);
    }
    
    // Check for accumulated inputs
    bool decode() {
        std::lock_guard<std::mutex> lock(_queueMutex);
        if (!_inputQueue.empty()) {
            decodedIRData.decodedRawData = _inputQueue.front();
            _inputQueue.erase(_inputQueue.begin());
            return true;
        }
        
        return false;
    }
    
    void resume() {
        // Nothing to do
    }
};

extern IRrecv IrReceiver;

#endif // IRREMOTE_HPP
