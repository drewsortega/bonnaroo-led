#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

void tetrisInit();
void tetrisLoop(unsigned long now);
void tetrisHandleInput(int dx, int dy, bool rotate);

#endif
