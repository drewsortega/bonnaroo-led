#pragma once

void visInit();
void visHandleInput(int dx, int dy, bool enter);
void visLoop(unsigned long now);
void visSetCurrent(int idx);
void visDrawBackground(int bg_idx, unsigned long now);
void visDrawAnimation(int anim_idx, unsigned long now);
