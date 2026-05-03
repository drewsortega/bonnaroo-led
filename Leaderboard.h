#pragma once
#include <stdint.h>

void lbDrawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b);
void lbDrawString(int x, int y, const char* str, uint8_t r, uint8_t g, uint8_t b);
void lbDrawNumber(int x, int y, int num, uint8_t r, uint8_t g, uint8_t b);

void lbInit(bool has_sd);
void lbStart(const char* filename, int score, void (*onComplete)());
void lbHandleText(const char* text);
void lbLoop(unsigned long now);
bool lbIsActive();
