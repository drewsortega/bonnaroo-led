#pragma once
#include <stdint.h>

void lbDrawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b);
void lbDrawString(int x, int y, const char* str, uint8_t r, uint8_t g, uint8_t b);
void lbDrawNumber(int x, int y, int num, uint8_t r, uint8_t g, uint8_t b);

void lbInit(bool has_sd);
void lbStart(const char* filename, int score, void (*onComplete)());
void lbHandleInput(int dx, int dy, bool enter);
void lbLoop(unsigned long now);
bool lbIsActive();
int lbGetHighScore(const char* filename);
void lbDrawBox(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool outline, uint8_t or_r=0, uint8_t or_g=0, uint8_t or_b=0);
