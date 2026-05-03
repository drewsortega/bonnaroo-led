#include "SnakeGame.h"
#include <stdint.h>
#include <stdlib.h>

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

#define SNAKE_MAX_LEN 1024
#define BOARD_WIDTH 64
#define BOARD_HEIGHT 64

struct Position {
    int x, y;
};

static Position snake[SNAKE_MAX_LEN];
static int snake_len = 0;
static Position apple;
static int dir_x = 1;
static int dir_y = 0;
static int next_dir_x = 1;
static int next_dir_y = 0;
static unsigned long last_move_time = 0;
static bool game_over = false;

static void placeApple() {
    bool ok = false;
    while (!ok) {
        apple.x = rand() % BOARD_WIDTH;
        apple.y = rand() % BOARD_HEIGHT;
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
    game_over = false;
    placeApple();
    last_move_time = 0; // will update immediately
}

void snakeSetDirection(int dx, int dy) {
    if (game_over) return;
    // prevent 180 degree turns
    if (dir_x == -dx && dx != 0) return;
    if (dir_y == -dy && dy != 0) return;
    
    next_dir_x = dx;
    next_dir_y = dy;
}

void snakeLoop(unsigned long now) {
    if (game_over) {
        // restart after 1 sec
        if (last_move_time > 0 && now - last_move_time > 1000) { 
            snakeInit();
        }
        return;
    }

    if (last_move_time > 0 && now - last_move_time < 100) {
        // move every 100ms (10 fps)
        return;
    }
    last_move_time = now;
    
    dir_x = next_dir_x;
    dir_y = next_dir_y;
    
    Position next_head = {snake[0].x + dir_x, snake[0].y + dir_y};
    
    // Check wall collision
    if (next_head.x < 0 || next_head.x >= BOARD_WIDTH || 
        next_head.y < 0 || next_head.y >= BOARD_HEIGHT) {
        game_over = true;
        return;
    }
    
    // Check self collision
    for (int i = 0; i < snake_len; i++) {
        // If we didn't eat an apple, the tail will move, but to be safe we can just check all except last piece
        if (i == snake_len - 1 && (next_head.x != apple.x || next_head.y != apple.y)) {
            continue; // tail will move
        }
        if (snake[i].x == next_head.x && snake[i].y == next_head.y) {
            game_over = true;
            return;
        }
    }
    
    // Move snake
    bool ate_apple = (next_head.x == apple.x && next_head.y == apple.y);
    
    if (ate_apple) {
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
    
    // Draw
    screenClearCallback();
    
    // Apple
    drawPixelCallback(apple.x, apple.y, 255, 0, 0); // Red apple
    
    // Snake
    for (int i = 0; i < snake_len; i++) {
        if (i == 0) {
            drawPixelCallback(snake[i].x, snake[i].y, 0, 255, 0); // Bright green head
        } else {
            drawPixelCallback(snake[i].x, snake[i].y, 0, 150, 0); // Darker green body
        }
    }
    
    updateScreenCallback();
}
