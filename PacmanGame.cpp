#include "PacmanGame.h"
#include <stdint.h>
#include <SD.h>
#include "Leaderboard.h"

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

#define GRID_W 28
#define GRID_H 29
#define TILE_S 2
#define OFFSET_X 4
#define OFFSET_Y 6

static const char initial_grid[GRID_H][GRID_W + 1] = {
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#O####.#####.##.#####.####O#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "     #.##### ## #####.#     ",
    "     #.##          ##.#     ",
    "     #.## ###--### ##.#     ",
    "######.## #      # ##.######",
    "      .   #      #   .      ",
    "######.## #      # ##.######",
    "     #.## ######## ##.#     ",
    "     #.##          ##.#     ",
    "     #.## ######## ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#O..##.......  .......##..O#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#"
};

static char grid[GRID_H][GRID_W];

enum GhostMode { SCATTER, CHASE, FRIGHTENED, EATEN };
enum GameState { STATE_INTRO, STATE_PLAYING, STATE_GAMEOVER, STATE_ENTER_NAME, STATE_LEADERBOARD };

struct Pacman {
    int x, y, dir_x, dir_y, next_dir_x, next_dir_y;
};

struct Ghost {
    int x, y, dir_x, dir_y, color_r, color_g, color_b;
    GhostMode mode;
    int scatter_target_x, scatter_target_y, spawn_x, spawn_y, exit_timer;
};

static Pacman pac;
static Ghost ghosts[4];

static unsigned long state_timer = 0;
static GameState game_state = STATE_INTRO;
static unsigned long last_move_time = 0;
static unsigned long ghost_last_move_time = 0;
static int score = 0;
static int lives = 3;
static int total_dots = 0;
static unsigned long frightened_timer = 0;
static unsigned long mode_timer = 0;
static bool global_chase_mode = false;
static int eaten_score = 200;
static bool pac_has_sd = false;

// Fonts moved to Leaderboard.cpp
// drawChar replaced by lbDrawChar

static void drawString(int x, int y, const char* str, uint8_t r, uint8_t g, uint8_t b) {
    lbDrawString(x, y, str, r, g, b);
}

static void drawNumber(int x, int y, int num, uint8_t r, uint8_t g, uint8_t b) {
    lbDrawNumber(x, y, num, r, g, b);
}

// Leaderboard logic moved to Leaderboard.cpp
static void resetLevel(bool full_reset) {
    if (full_reset) {
        total_dots = 0;
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                grid[y][x] = initial_grid[y][x];
                if (grid[y][x] == '.' || grid[y][x] == 'O') {
                    total_dots++;
                }
            }
        }
    }
    
    pac.x = 13;
    pac.y = 22;
    pac.dir_x = -1; pac.dir_y = 0;
    pac.next_dir_x = -1; pac.next_dir_y = 0;
    
    ghosts[0].x = 13; ghosts[0].y = 10; ghosts[0].spawn_x = 13; ghosts[0].spawn_y = 13;
    ghosts[0].dir_x = -1; ghosts[0].dir_y = 0; ghosts[0].exit_timer = 0;
    ghosts[0].color_r = 255; ghosts[0].color_g = 0; ghosts[0].color_b = 0;
    ghosts[0].scatter_target_x = 26; ghosts[0].scatter_target_y = 0;
    
    ghosts[1].x = 14; ghosts[1].y = 13; ghosts[1].spawn_x = 14; ghosts[1].spawn_y = 13;
    ghosts[1].dir_x = 0; ghosts[1].dir_y = -1; ghosts[1].exit_timer = 10;
    ghosts[1].color_r = 255; ghosts[1].color_g = 184; ghosts[1].color_b = 255;
    ghosts[1].scatter_target_x = 1; ghosts[1].scatter_target_y = 0;
    
    ghosts[2].x = 12; ghosts[2].y = 13; ghosts[2].spawn_x = 12; ghosts[2].spawn_y = 13;
    ghosts[2].dir_x = 0; ghosts[2].dir_y = -1; ghosts[2].exit_timer = 30;
    ghosts[2].color_r = 0; ghosts[2].color_g = 255; ghosts[2].color_b = 255;
    ghosts[2].scatter_target_x = 26; ghosts[2].scatter_target_y = 28;
    
    ghosts[3].x = 15; ghosts[3].y = 13; ghosts[3].spawn_x = 15; ghosts[3].spawn_y = 13;
    ghosts[3].dir_x = 0; ghosts[3].dir_y = -1; ghosts[3].exit_timer = 60;
    ghosts[3].color_r = 255; ghosts[3].color_g = 184; ghosts[3].color_b = 82;
    ghosts[3].scatter_target_x = 1; ghosts[3].scatter_target_y = 28;
    
    for(int i=0; i<4; i++) ghosts[i].mode = SCATTER;
    
    global_chase_mode = false;
    mode_timer = 0;
    frightened_timer = 0;
}

void pacmanInit(bool has_sd) {
    pac_has_sd = has_sd;
    score = 0;
    lives = 3;
    resetLevel(true);
    game_state = STATE_INTRO;
    state_timer = 0;
    last_move_time = 0;
    ghost_last_move_time = 0;
}

void pacmanSetDirection(int dx, int dy) {
    if (game_state != STATE_PLAYING) return;
    pac.next_dir_x = dx;
    pac.next_dir_y = dy;
}

void pacmanHandleText(const char* text) {
    // Handled by lbHandleText
}

static bool isWall(int x, int y, bool isGhost, bool isEaten) {
    if (x < 0 || x >= GRID_W) return false; // Tunnel
    if (y < 0 || y >= GRID_H) return true;
    if (grid[y][x] == '#') return true;
    if (grid[y][x] == '-') {
        if (!isGhost) return true; // Pacman can't pass door
        if (!isEaten && y == 11) return true; // Ghosts can't enter house unless eaten
    }
    return false;
}

static int distanceSq(int x1, int y1, int x2, int y2) {
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

// (unused was_in_top_5 removed)

void pacmanLoop(unsigned long now) {
    if (game_state == STATE_GAMEOVER) {
        if (state_timer > 0 && now - state_timer > 3000) {
            lbStart("pac_lb.txt", score, [](){
                score = 0;
                lives = 3;
                resetLevel(true);
                game_state = STATE_INTRO;
                state_timer = millis();
                last_move_time = 0;
                ghost_last_move_time = 0;
            });
            game_state = STATE_LEADERBOARD; // Actually just handled by lbLoop
        }
        return;
    } else if (game_state == STATE_ENTER_NAME || game_state == STATE_LEADERBOARD) {
        lbLoop(now);
        return;
    } else if (game_state == STATE_INTRO) {
        if (state_timer == 0) state_timer = now;
        if (now - state_timer > 2500) {
            game_state = STATE_PLAYING;
            last_move_time = now;
            ghost_last_move_time = now;
            mode_timer = now;
        }
    } else if (game_state == STATE_PLAYING) {
        if (total_dots == 0) {
            resetLevel(true);
            game_state = STATE_INTRO;
            state_timer = now;
        } else {
            // Mode transitions
            if (frightened_timer > 0) {
                if (now - frightened_timer > 8000) { // 8 seconds of fright
                    frightened_timer = 0;
                    for(int i=0; i<4; i++) {
                        if (ghosts[i].mode == FRIGHTENED) ghosts[i].mode = global_chase_mode ? CHASE : SCATTER;
                    }
                }
            } else {
                if (now - mode_timer > 20000) { // switch every 20s
                    global_chase_mode = !global_chase_mode;
                    mode_timer = now;
                    for(int i=0; i<4; i++) {
                        if (ghosts[i].mode != EATEN) {
                            ghosts[i].mode = global_chase_mode ? CHASE : SCATTER;
                            ghosts[i].dir_x = -ghosts[i].dir_x;
                            ghosts[i].dir_y = -ghosts[i].dir_y;
                        }
                    }
                }
            }

            // PACMAN MOVEMENT
            if (now - last_move_time > 100) {
                last_move_time = now;
                
                int try_x = pac.x + pac.next_dir_x;
                int try_y = pac.y + pac.next_dir_y;
                if (try_x < 0) try_x = GRID_W - 1;
                if (try_x >= GRID_W) try_x = 0;
                
                if (!isWall(try_x, try_y, false, false)) {
                    pac.dir_x = pac.next_dir_x;
                    pac.dir_y = pac.next_dir_y;
                }
                
                pac.x += pac.dir_x;
                pac.y += pac.dir_y;
                if (pac.x < 0) pac.x = GRID_W - 1;
                if (pac.x >= GRID_W) pac.x = 0;
                
                if (isWall(pac.x, pac.y, false, false)) {
                    pac.x -= pac.dir_x;
                    pac.y -= pac.dir_y;
                }
                
                if (grid[pac.y][pac.x] == '.') {
                    grid[pac.y][pac.x] = ' ';
                    score += 10;
                    total_dots--;
                } else if (grid[pac.y][pac.x] == 'O') {
                    grid[pac.y][pac.x] = ' ';
                    score += 50;
                    total_dots--;
                    frightened_timer = now;
                    eaten_score = 200;
                    for(int i=0; i<4; i++) {
                        if (ghosts[i].mode != EATEN) {
                            ghosts[i].mode = FRIGHTENED;
                            ghosts[i].dir_x = -ghosts[i].dir_x;
                            ghosts[i].dir_y = -ghosts[i].dir_y;
                        }
                    }
                }
            }

            // GHOST MOVEMENT
            unsigned long ghost_speed = 120;
            if (frightened_timer > 0) ghost_speed = 160;
            
            if (now - ghost_last_move_time > ghost_speed) {
                ghost_last_move_time = now;
                
                for (int i = 0; i < 4; i++) {
                    if (ghosts[i].exit_timer > 0) {
                        ghosts[i].exit_timer--;
                        if (ghosts[i].exit_timer == 0) {
                            ghosts[i].x = 13; ghosts[i].y = 10;
                        }
                        continue;
                    }
                    
                    int target_x = pac.x;
                    int target_y = pac.y;
                    
                    if (ghosts[i].mode == EATEN) {
                        target_x = 13;
                        target_y = 10;
                        if (ghosts[i].x == target_x && ghosts[i].y == target_y) {
                            ghosts[i].mode = global_chase_mode ? CHASE : SCATTER;
                            ghosts[i].x = ghosts[i].spawn_x;
                            ghosts[i].y = ghosts[i].spawn_y;
                            ghosts[i].exit_timer = 10;
                            continue;
                        }
                    } else if (ghosts[i].mode == SCATTER) {
                        target_x = ghosts[i].scatter_target_x;
                        target_y = ghosts[i].scatter_target_y;
                    } else if (ghosts[i].mode == CHASE) {
                        if (i == 1) { // Pinky
                            target_x = pac.x + pac.dir_x * 4;
                            target_y = pac.y + pac.dir_y * 4;
                        } else if (i == 2) { // Inky
                            int tx = pac.x + pac.dir_x * 2;
                            int ty = pac.y + pac.dir_y * 2;
                            target_x = tx + (tx - ghosts[0].x);
                            target_y = ty + (ty - ghosts[0].y);
                        } else if (i == 3) { // Clyde
                            if (distanceSq(ghosts[i].x, ghosts[i].y, pac.x, pac.y) < 64) {
                                target_x = ghosts[i].scatter_target_x;
                                target_y = ghosts[i].scatter_target_y;
                            }
                        }
                    }
                    
                    int dirs[4][2] = {{0,-1}, {-1,0}, {0,1}, {1,0}}; // UP, LEFT, DOWN, RIGHT
                    int best_d = -1;
                    int min_dist = 999999;
                    int valid_dirs[4];
                    int valid_count = 0;
                    
                    for (int d = 0; d < 4; d++) {
                        int dx = dirs[d][0];
                        int dy = dirs[d][1];
                        if (dx == -ghosts[i].dir_x && dy == -ghosts[i].dir_y && (ghosts[i].dir_x!=0 || ghosts[i].dir_y!=0)) continue;
                        
                        int nx = ghosts[i].x + dx;
                        int ny = ghosts[i].y + dy;
                        if (nx < 0) nx = GRID_W - 1;
                        if (nx >= GRID_W) nx = 0;
                        
                        if (!isWall(nx, ny, true, ghosts[i].mode == EATEN)) {
                            valid_dirs[valid_count++] = d;
                            int dist = distanceSq(nx, ny, target_x, target_y);
                            if (dist < min_dist) {
                                min_dist = dist;
                                best_d = d;
                            }
                        }
                    }
                    
                    if (valid_count > 0) {
                        if (ghosts[i].mode == FRIGHTENED) {
                            int r = rand() % valid_count;
                            best_d = valid_dirs[r];
                        }
                        ghosts[i].dir_x = dirs[best_d][0];
                        ghosts[i].dir_y = dirs[best_d][1];
                    } else {
                        ghosts[i].dir_x = -ghosts[i].dir_x;
                        ghosts[i].dir_y = -ghosts[i].dir_y;
                    }
                    
                    ghosts[i].x += ghosts[i].dir_x;
                    ghosts[i].y += ghosts[i].dir_y;
                    if (ghosts[i].x < 0) ghosts[i].x = GRID_W - 1;
                    if (ghosts[i].x >= GRID_W) ghosts[i].x = 0;
                }
            }

            // Collision detection
            for (int i = 0; i < 4; i++) {
                if (ghosts[i].x == pac.x && ghosts[i].y == pac.y) {
                    if (ghosts[i].mode == FRIGHTENED) {
                        ghosts[i].mode = EATEN;
                        score += eaten_score;
                        eaten_score *= 2;
                    } else if (ghosts[i].mode != EATEN) {
                        lives--;
                        if (lives > 0) {
                            resetLevel(false);
                            game_state = STATE_INTRO;
                            state_timer = now;
                        } else {
                            game_state = STATE_GAMEOVER;
                            state_timer = now;
                        }
                    }
                }
            }
        }
    }

    // DRAWING
    screenClearCallback();
    
    if (game_state == STATE_ENTER_NAME || game_state == STATE_LEADERBOARD) {
        // Handled by lbLoop
        return;
    } else {
        // Draw Score
        drawNumber(4, 0, score, 255, 255, 255);
        
        // Draw Lives
        for (int l = 0; l < lives - 1; l++) {
            int lx = 64 - 4 - (l * 6);
            int ly = 1;
            drawPixelCallback(lx, ly, 255, 255, 0);
            drawPixelCallback(lx+1, ly, 255, 255, 0);
            drawPixelCallback(lx, ly+1, 255, 255, 0);
            drawPixelCallback(lx+1, ly+1, 255, 255, 0);
        }
        
        for (int y = 0; y < GRID_H; y++) {
            for (int x = 0; x < GRID_W; x++) {
                char c = grid[y][x];
                int px = OFFSET_X + x * TILE_S;
                int py = OFFSET_Y + y * TILE_S;
                
                if (c == '#') {
                    drawPixelCallback(px, py, 0, 0, 150);
                    drawPixelCallback(px+1, py, 0, 0, 150);
                    drawPixelCallback(px, py+1, 0, 0, 150);
                    drawPixelCallback(px+1, py+1, 0, 0, 150);
                } else if (c == '-') {
                    drawPixelCallback(px, py+1, 255, 184, 174);
                    drawPixelCallback(px+1, py+1, 255, 184, 174);
                } else if (c == '.') {
                    drawPixelCallback(px, py, 255, 184, 174);
                } else if (c == 'O') {
                    drawPixelCallback(px, py, 255, 184, 174);
                    drawPixelCallback(px+1, py, 255, 184, 174);
                    drawPixelCallback(px, py+1, 255, 184, 174);
                    drawPixelCallback(px+1, py+1, 255, 184, 174);
                }
            }
        }
        
        // Draw pacman
        int px = OFFSET_X + pac.x * TILE_S;
        int py = OFFSET_Y + pac.y * TILE_S;
        drawPixelCallback(px, py, 255, 255, 0);
        drawPixelCallback(px+1, py, 255, 255, 0);
        drawPixelCallback(px, py+1, 255, 255, 0);
        drawPixelCallback(px+1, py+1, 255, 255, 0);
        
        // Draw ghosts
        for (int i = 0; i < 4; i++) {
            int gx = OFFSET_X + ghosts[i].x * TILE_S;
            int gy = OFFSET_Y + ghosts[i].y * TILE_S;
            
            if (ghosts[i].mode == FRIGHTENED) {
                drawPixelCallback(gx, gy, 0, 0, 255);
                drawPixelCallback(gx+1, gy, 0, 0, 255);
                drawPixelCallback(gx, gy+1, 0, 0, 255);
                drawPixelCallback(gx+1, gy+1, 0, 0, 255);
            } else if (ghosts[i].mode == EATEN) {
                drawPixelCallback(gx, gy, 255, 255, 255);
                drawPixelCallback(gx+1, gy, 255, 255, 255);
            } else {
                drawPixelCallback(gx, gy, ghosts[i].color_r, ghosts[i].color_g, ghosts[i].color_b);
                drawPixelCallback(gx+1, gy, ghosts[i].color_r, ghosts[i].color_g, ghosts[i].color_b);
                drawPixelCallback(gx, gy+1, ghosts[i].color_r, ghosts[i].color_g, ghosts[i].color_b);
                drawPixelCallback(gx+1, gy+1, ghosts[i].color_r, ghosts[i].color_g, ghosts[i].color_b);
            }
        }
        
        // Draw "GET READY" or Game Over
        if (game_state == STATE_INTRO) {
            int rx = OFFSET_X + 14;
            int ry = OFFSET_Y + 16 * TILE_S;
            drawString(rx, ry, "GET READY", 255, 255, 0);
        } else if (game_state == STATE_GAMEOVER) {
            int rx = OFFSET_X + 14;
            int ry = OFFSET_Y + 16 * TILE_S;
            drawString(rx, ry, "GAME OVER", 255, 0, 0);
            
            drawPixelCallback(px, py, 255, 0, 0);
            drawPixelCallback(px+1, py+1, 255, 0, 0);
            drawPixelCallback(px+1, py, 255, 0, 0);
            drawPixelCallback(px, py+1, 255, 0, 0);
        }
    }
    
    updateScreenCallback();
}
