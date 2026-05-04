#include "SnakeGame.h"
#include <stdint.h>
#include <stdlib.h>

#include "Leaderboard.h"

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

#define SNAKE_MAX_LEN 1024
#define BOARD_WIDTH 32
#define BOARD_HEIGHT 27
#define OFFSET_Y 10

struct Position {
    int x, y;
};

static Position snake[SNAKE_MAX_LEN];
static int snake_len = 0;
static Position apple;
static int dir_x = 1;
static int dir_y = 0;
enum SnakeGameState { SNAKE_STATE_WAITING, SNAKE_STATE_INTRO, SNAKE_STATE_PLAYING, SNAKE_STATE_GAMEOVER };

static int next_dir_x = 1;
static int next_dir_y = 0;
static unsigned long last_move_time = 0;
static SnakeGameState snake_state = SNAKE_STATE_INTRO;
static unsigned long snake_state_timer = 0;
static int score = 0;

static void placeApple() {
    bool ok = false;
    while (!ok) {
        apple.x = 1 + rand() % (BOARD_WIDTH - 2);
        apple.y = 1 + rand() % (BOARD_HEIGHT - 2);
        ok = true;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == apple.x && snake[i].y == apple.y) {
                ok = false;
                break;
            }
        }
    }
}

void snakeInit() {
    snake_len = 4;
    snake[0] = {BOARD_WIDTH/2, BOARD_HEIGHT/2};
    snake[1] = {BOARD_WIDTH/2 - 1, BOARD_HEIGHT/2};
    snake[2] = {BOARD_WIDTH/2 - 2, BOARD_HEIGHT/2};
    snake[3] = {BOARD_WIDTH/2 - 3, BOARD_HEIGHT/2};
    
    dir_x = 1;
    dir_y = 0;
    next_dir_x = 1;
    next_dir_y = 0;
    snake_state = SNAKE_STATE_WAITING;
    snake_state_timer = 0;
    score = 0;
    placeApple();
    last_move_time = 0; // will update immediately
}

void snakeSetDirection(int dx, int dy) {
    if (snake_state != SNAKE_STATE_PLAYING) return;
    // prevent 180 degree turns
    if (dir_x == -dx && dx != 0) return;
    if (dir_y == -dy && dy != 0) return;
    
    next_dir_x = dx;
    next_dir_y = dy;
}

void snakeHandleEnter() {
    if (snake_state == SNAKE_STATE_WAITING) {
        snake_state = SNAKE_STATE_INTRO;
        snake_state_timer = 0;
    }
}

void snakeLoop(unsigned long now) {
    if (lbIsActive()) {
        lbLoop(now);
        return;
    }

    if (snake_state == SNAKE_STATE_GAMEOVER) {
        if (snake_state_timer == 0) snake_state_timer = now;
        if (now - snake_state_timer > 3000) {
            lbStart("snk_lb.txt", score, [](){ snakeInit(); });
            return;
        }
    } else if (snake_state == SNAKE_STATE_WAITING) {
        // Wait for enter press
    } else if (snake_state == SNAKE_STATE_INTRO) {
        if (snake_state_timer == 0) snake_state_timer = now;
        if (now - snake_state_timer > 2000) {
            snake_state = SNAKE_STATE_PLAYING;
            last_move_time = now;
        }
    } else if (snake_state == SNAKE_STATE_PLAYING) {
        if (last_move_time == 0 || now - last_move_time >= 115) {
            last_move_time = now;
            
            dir_x = next_dir_x;
            dir_y = next_dir_y;
            
            Position next_head = {snake[0].x + dir_x, snake[0].y + dir_y};
            
            // Check wall collision
            if (next_head.x < 0 || next_head.x >= BOARD_WIDTH || 
                next_head.y < 0 || next_head.y >= BOARD_HEIGHT) {
                snake_state = SNAKE_STATE_GAMEOVER;
                snake_state_timer = now;
            }
            
            // Check self collision
            if (snake_state == SNAKE_STATE_PLAYING) {
                for (int i = 0; i < snake_len; i++) {
                    if (i == snake_len - 1 && (next_head.x != apple.x || next_head.y != apple.y)) {
                        continue;
                    }
                    if (snake[i].x == next_head.x && snake[i].y == next_head.y) {
                        snake_state = SNAKE_STATE_GAMEOVER;
                        snake_state_timer = now;
                        break;
                    }
                }
            }
            
            if (snake_state == SNAKE_STATE_PLAYING) {
                bool ate_apple = (next_head.x == apple.x && next_head.y == apple.y);
                
                if (ate_apple) {
                    score += 10;
                    if (snake_len < SNAKE_MAX_LEN) {
                        snake_len++;
                    }
                }
                
                for (int i = snake_len - 1; i > 0; i--) {
                    snake[i] = snake[i-1];
                }
                snake[0] = next_head;
                
                if (ate_apple) {
                    placeApple();
                }
            }
        }
    }
    
    // Draw
    screenClearCallback();
    
    // Draw Score
    lbDrawNumber(4, 2, score, 255, 255, 255);
    
    // Draw Separator
    for(int x=0; x<64; x+=2) {
        drawPixelCallback(x, 9, 150, 150, 150);
    }
    
    // Apple
    int ax = apple.x * 2;
    int ay = apple.y * 2 + OFFSET_Y;
    drawPixelCallback(ax, ay, 255, 0, 0);
    drawPixelCallback(ax+1, ay, 255, 0, 0);
    drawPixelCallback(ax, ay+1, 255, 0, 0);
    drawPixelCallback(ax+1, ay+1, 255, 0, 0);
    
    // Snake
    for (int i = 0; i < snake_len; i++) {
        int sx = snake[i].x * 2;
        int sy = snake[i].y * 2 + OFFSET_Y;
        if (i == 0) {
            drawPixelCallback(sx, sy, 0, 255, 0);
            drawPixelCallback(sx+1, sy, 0, 255, 0);
            drawPixelCallback(sx, sy+1, 0, 255, 0);
            drawPixelCallback(sx+1, sy+1, 0, 255, 0);
        } else {
            drawPixelCallback(sx, sy, 0, 150, 0);
            drawPixelCallback(sx+1, sy, 0, 150, 0);
            drawPixelCallback(sx, sy+1, 0, 150, 0);
            drawPixelCallback(sx+1, sy+1, 0, 150, 0);
        }
    }
    
    if (snake_state == SNAKE_STATE_WAITING) {
        lbDrawBox(14, 22, 23, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(16, 24, "SNAKE", 255, 255, 0);
        lbDrawBox(2, 36, 47, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(4, 38, "PRESS ENTER", 255, 255, 255);
    } else if (snake_state == SNAKE_STATE_INTRO) {
        lbDrawBox(10, 28, 39, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(12, 30, "GET READY", 255, 255, 0);
    } else if (snake_state == SNAKE_STATE_GAMEOVER) {
        lbDrawBox(10, 28, 39, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(12, 30, "GAME OVER", 255, 0, 0);
        
        // Red X over the head
        int hx = snake[0].x * 2;
        int hy = snake[0].y * 2 + OFFSET_Y;
        drawPixelCallback(hx, hy, 255, 0, 0);
        drawPixelCallback(hx+1, hy+1, 255, 0, 0);
        drawPixelCallback(hx+1, hy, 255, 0, 0);
        drawPixelCallback(hx, hy+1, 255, 0, 0);
    }
    
    updateScreenCallback();
}
