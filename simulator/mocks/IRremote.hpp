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
private:
    bool _hasData;
    IRRawDataType _lastCode;
    
public:
    IRData decodedIRData;
    
    IRrecv() : _hasData(false), _lastCode(0) {
        decodedIRData.decodedRawData = 0;
    }
    
    void begin(int pin, bool enableLED = false) {
        (void)pin;
        (void)enableLED;
        printf("[IRremote] Receiver started (using keyboard input)\n");
    }
    
    // Check for keyboard input and convert to IR codes
    bool decode() {
        _hasData = false;
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                // Set a special quit code
                decodedIRData.decodedRawData = 0xFFFFFFFF;
                _hasData = true;
                return true;
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
                        code = 0xFFFFFFFF;  // Special quit code
                        break;
                    
                    default:
                        continue;  // Unknown key, keep polling
                }
                
                if (code != 0) {
                    decodedIRData.decodedRawData = code;
                    _hasData = true;
                    return true;
                }
            }
        }
        
        return false;
    }
    
    void resume() {
        // Nothing to do - SDL handles this automatically
    }
};

extern IRrecv IrReceiver;

#endif // IRREMOTE_HPP
