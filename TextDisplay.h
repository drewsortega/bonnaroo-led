#ifndef TEXT_DISPLAY_H
#define TEXT_DISPLAY_H

#include <Arduino.h>
#include <SD.h>

void textInit(bool use_sd);
void textLoop(unsigned long now);
void textHandleInput(int dx, int dy, bool button);

#endif
