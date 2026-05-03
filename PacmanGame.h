#pragma once

void pacmanInit(bool has_sd);
void pacmanSetDirection(int dx, int dy);
void pacmanHandleText(const char* text);
void pacmanLoop(unsigned long now);
