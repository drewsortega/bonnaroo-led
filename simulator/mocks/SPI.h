#ifndef SPI_H
#define SPI_H

/**
 * SPI.h Mock for LED Grid Simulator
 * Stub implementation - SPI not needed for simulation.
 */

#include "Arduino.h"

#define SPI_HALF_SPEED 0
#define SHARED_SPI 0

class SPIClass {
public:
    void begin() {}
    void end() {}
    void beginTransaction(void*) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t data) { return data; }
};

extern SPIClass SPI;
extern SPIClass SPI1;

#endif // SPI_H
