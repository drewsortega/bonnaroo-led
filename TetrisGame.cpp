#include "TetrisGame.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Leaderboard.h"

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
#include <Arduino.h>

// ── CONSTANTS & TYPES ──
#define BOARD_W 10
#define BOARD_H 20
#define BLOCK_SIZE 3

enum TetrisState { TETRIS_WAITING, TETRIS_INTRO, TETRIS_PLAYING, TETRIS_GAMEOVER };

// Shapes: 7 pieces, 4 rotations, 4x4 grid (flattened to 16 bits)
// 1 = solid, 0 = empty. Read left-to-right, top-to-bottom.
static const uint16_t SHAPES[7][4] = {
    // I (Cyan)
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },
    // J (Blue)
    { 0x08E0, 0x0644, 0x00E2, 0x0226 },
    // L (Orange)
    { 0x02E0, 0x0446, 0x00E8, 0x0C44 },
    // O (Yellow)
    { 0x0660, 0x0660, 0x0660, 0x0660 },
    // S (Green)
    { 0x06C0, 0x0462, 0x006C, 0x08C4 },
    // T (Purple)
    { 0x04E0, 0x0464, 0x00E4, 0x04C4 },
    // Z (Red)
    { 0x0C60, 0x0264, 0x00C6, 0x04C8 }
};

static const uint8_t COLORS[8][3] = {
    {0, 0, 0},         // Empty
    {0, 255, 255},     // 1: I - Cyan
    {0, 100, 255},     // 2: J - Blue (slightly brightened for visibility)
    {255, 127, 0},     // 3: L - Orange
    {255, 255, 0},     // 4: O - Yellow
    {0, 255, 0},       // 5: S - Green
    {128, 0, 128},     // 6: T - Purple
    {255, 0, 0}        // 7: Z - Red
};

// NES Drop speeds in frames (60fps)
static const int FRAMES_PER_CELL[] = {
    48, 43, 38, 33, 28, 23, 18, 13, 8, 6, 
    5, 5, 5, 
    4, 4, 4, 
    3, 3, 3, 
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1 // 29+
};

// ── STATE ──
static TetrisState t_state;
static unsigned long t_state_timer;
static uint8_t board[BOARD_H][BOARD_W]; // stores color index 1-7, 0 is empty
static int t_score;
static int t_start_level = 5;
static int t_level;
static int t_lines;

static int piece_id;
static int piece_rot;
static int piece_x;
static int piece_y;

static int next_piece_id;

static unsigned long last_fall_time;

// ── FORWARD DECLARATIONS ──
static void spawnPiece();
static bool checkCollision(int p_id, int p_rot, int p_x, int p_y);
static void lockPiece();
static void clearLines();

// ── LOGIC ──

static int getDropDelayMs() {
    int lvl = t_level;
    if (lvl > 29) lvl = 29;
    int frames = FRAMES_PER_CELL[lvl];
    // 60 fps -> 1 frame = 16.666 ms
    return frames * 16.666f;
}

static void drawBlock(int x, int y, int color_idx) {
    if (color_idx == 0) return;
    int px = x * BLOCK_SIZE + 2;  // board on the left
    int py = y * BLOCK_SIZE + 2;  // slight padding from top
    
    uint8_t r = COLORS[color_idx][0];
    uint8_t g = COLORS[color_idx][1];
    uint8_t b = COLORS[color_idx][2];
    
    // Draw 3x3 block: bright border, dark fill
    for (int dy = 0; dy < BLOCK_SIZE; dy++) {
        for (int dx = 0; dx < BLOCK_SIZE; dx++) {
            if (dx == 0 || dy == 0 || dx == BLOCK_SIZE-1 || dy == BLOCK_SIZE-1) {
                drawPixelCallback(px + dx, py + dy, r, g, b); // Border (bright)
            } else {
                drawPixelCallback(px + dx, py + dy, r/2, g/2, b/2); // Fill (dark)
            }
        }
    }
}

void tetrisInit() {
    memset(board, 0, sizeof(board));
    t_score = 0;
    t_level = t_start_level;
    t_lines = 0;
    
    // Seed PRNG based on millis
    srand(millis());
    next_piece_id = rand() % 7;
    
    t_state = TETRIS_WAITING;
    t_state_timer = 0;
}

static void spawnPiece() {
    piece_id = next_piece_id;
    next_piece_id = rand() % 7;
    piece_rot = 0;
    piece_x = 3;
    piece_y = 0;
    
    if (checkCollision(piece_id, piece_rot, piece_x, piece_y)) {
        t_state = TETRIS_GAMEOVER;
        t_state_timer = millis();
    }
}

static bool checkCollision(int p_id, int p_rot, int p_x, int p_y) {
    uint16_t shape = SHAPES[p_id][p_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bit = (shape >> (15 - (r * 4 + c))) & 1;
            if (bit) {
                int bx = p_x + c;
                int by = p_y + r;
                
                if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
                if (by >= 0 && board[by][bx] != 0) return true;
            }
        }
    }
    return false;
}

static void lockPiece() {
    uint16_t shape = SHAPES[piece_id][piece_rot];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bit = (shape >> (15 - (r * 4 + c))) & 1;
            if (bit) {
                int bx = piece_x + c;
                int by = piece_y + r;
                if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W) {
                    board[by][bx] = piece_id + 1;
                }
            }
        }
    }
    clearLines();
    spawnPiece();
}

static void clearLines() {
    int lines_cleared = 0;
    for (int y = BOARD_H - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_W; x++) {
            if (board[y][x] == 0) {
                full = false;
                break;
            }
        }
        
        if (full) {
            lines_cleared++;
            // Shift everything down
            for (int sy = y; sy > 0; sy--) {
                for (int sx = 0; sx < BOARD_W; sx++) {
                    board[sy][sx] = board[sy-1][sx];
                }
            }
            // Clear top row
            for (int sx = 0; sx < BOARD_W; sx++) {
                board[0][sx] = 0;
            }
            y++; // Re-check this row index since we shifted down
        }
    }
    
    if (lines_cleared > 0) {
        // NES Scoring
        int line_scores[] = {0, 40, 100, 300, 1200};
        t_score += line_scores[lines_cleared] * (t_level + 1);
        
        t_lines += lines_cleared;
        if (t_lines >= (t_level + 1) * 10) {
            t_level++;
        }
    }
}

void tetrisHandleInput(int dx, int dy, bool rotate) {
    if (t_state == TETRIS_WAITING) {
        if (dx != 0) {
            t_start_level += dx;
            if (t_start_level < 0) t_start_level = 0;
            if (t_start_level > 19) t_start_level = 19;
            t_level = t_start_level;
        } else if (rotate) {
            t_state = TETRIS_INTRO;
            t_state_timer = millis();
        }
        return;
    }
    
    if (t_state != TETRIS_PLAYING) return;
    
    if (dx != 0) {
        if (!checkCollision(piece_id, piece_rot, piece_x + dx, piece_y)) {
            piece_x += dx;
        }
    }
    
    if (rotate) {
        int next_rot = (piece_rot + 1) % 4;
        if (!checkCollision(piece_id, next_rot, piece_x, piece_y)) {
            piece_rot = next_rot;
        } else {
            // Simple wall kick (1 pixel left or right)
            if (!checkCollision(piece_id, next_rot, piece_x - 1, piece_y)) {
                piece_x -= 1;
                piece_rot = next_rot;
            } else if (!checkCollision(piece_id, next_rot, piece_x + 1, piece_y)) {
                piece_x += 1;
                piece_rot = next_rot;
            }
        }
    }
    
    if (dy > 0) { // Soft drop
        if (!checkCollision(piece_id, piece_rot, piece_x, piece_y + 1)) {
            piece_y += 1;
            t_score += 1; // 1 point per soft drop
            last_fall_time = millis(); // Reset fall timer
        } else {
            lockPiece();
        }
    }
}

void tetrisLoop(unsigned long now) {
    if (lbIsActive()) {
        lbLoop(now);
        return;
    }
    
    if (t_state == TETRIS_WAITING) {
        // waiting for input
    } else if (t_state == TETRIS_INTRO) {
        if (t_state_timer == 0) t_state_timer = now;
        if (now - t_state_timer > 2000) {
            spawnPiece();
            last_fall_time = now;
            t_state = TETRIS_PLAYING;
        }
    } else if (t_state == TETRIS_GAMEOVER) {
        if (t_state_timer == 0) t_state_timer = now;
        if (now - t_state_timer > 3000) {
            lbStart("tet_lb.txt", t_score, [](){ tetrisInit(); });
            return;
        }
    } else if (t_state == TETRIS_PLAYING) {
        int drop_delay = getDropDelayMs();
        if (now - last_fall_time >= (unsigned long)drop_delay) {
            last_fall_time = now;
            if (!checkCollision(piece_id, piece_rot, piece_x, piece_y + 1)) {
                piece_y++;
            } else {
                lockPiece();
            }
        }
    }
    
    // ── DRAWING ──
    screenClearCallback();
    
    // Draw board borders
    int px_start = 2 - 1;
    int px_end = 2 + BOARD_W * BLOCK_SIZE;
    int py_start = 2 - 1;
    int py_end = 2 + BOARD_H * BLOCK_SIZE;
    
    for (int y = py_start; y <= py_end; y++) {
        drawPixelCallback(px_start, y, 50, 50, 50);
        drawPixelCallback(px_end, y, 50, 50, 50);
    }
    for (int x = px_start; x <= px_end; x++) {
        drawPixelCallback(x, py_end, 50, 50, 50);
    }
    
    // Draw locked blocks
    for (int r = 0; r < BOARD_H; r++) {
        for (int c = 0; c < BOARD_W; c++) {
            if (board[r][c] != 0) {
                drawBlock(c, r, board[r][c]);
            }
        }
    }
    
    // Draw current piece
    if (t_state == TETRIS_PLAYING) {
        uint16_t shape = SHAPES[piece_id][piece_rot];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                int bit = (shape >> (15 - (r * 4 + c))) & 1;
                if (bit) {
                    drawBlock(piece_x + c, piece_y + r, piece_id + 1);
                }
            }
        }
    }
    
    // Right Side UI Elements
    if (t_state != TETRIS_WAITING) {
        // Score
        lbDrawString(35, 2, "SCORE", 150, 150, 150);
        lbDrawNumber(35, 8, t_score, 255, 255, 255);
        
        // Level
        lbDrawString(35, 16, "LEVEL", 150, 150, 150);
        lbDrawNumber(35, 22, t_level, 255, 255, 255);
        
        // Lines
        lbDrawString(35, 30, "LINES", 150, 150, 150);
        lbDrawNumber(35, 36, t_lines, 255, 255, 255);
        
        // Next Piece Preview
        if (t_state == TETRIS_PLAYING || t_state == TETRIS_GAMEOVER) {
            lbDrawString(35, 44, "NEXT", 100, 100, 100);
            uint16_t next_shape = SHAPES[next_piece_id][0];
            for (int r = 0; r < 4; r++) {
                for (int c = 0; c < 4; c++) {
                    int bit = (next_shape >> (15 - (r * 4 + c))) & 1;
                    if (bit) {
                        // Draw mini block
                        int nx = 35 + c * 3;
                        int ny = 51 + r * 3;
                        uint8_t cr = COLORS[next_piece_id+1][0];
                        uint8_t cg = COLORS[next_piece_id+1][1];
                        uint8_t cb = COLORS[next_piece_id+1][2];
                        for (int dy=0; dy<2; dy++) {
                            for (int dx=0; dx<2; dx++) {
                                drawPixelCallback(nx+dx, ny+dy, cr, cg, cb);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Middle Overlay UI Elements
    if (t_state == TETRIS_WAITING) {
        lbDrawBox(33, 13, 27, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(35, 15, "TETRIS", 255, 255, 0);
        
        lbDrawBox(33, 28, 31, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(35, 30, "LVL: ", 200, 200, 200);
        lbDrawNumber(55, 30, t_start_level, 0, 255, 255);
        
        lbDrawBox(33, 43, 23, 16, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(35, 45, "PRESS", 200, 200, 200);
        lbDrawString(35, 52, "START", 200, 200, 200);
    } else if (t_state == TETRIS_INTRO) {
        lbDrawBox(5, 22, 24, 17, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(11, 24, "GET", 255, 0, 0);
        lbDrawString(7, 32, "READY", 255, 0, 0);
    } else if (t_state == TETRIS_GAMEOVER) {
        lbDrawBox(9, 22, 19, 17, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(11, 24, "GAME", 255, 0, 0);
        lbDrawString(11, 32, "OVER", 255, 0, 0);
    }
    
    updateScreenCallback();
}
