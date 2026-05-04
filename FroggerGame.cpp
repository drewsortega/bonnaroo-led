#include "FroggerGame.h"
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "Leaderboard.h"

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);
extern unsigned long millis();

// ── Layout ──────────────────────────────────────────────
// 64x64 display. Top 8px = score bar. Remaining 56px = 14 rows of 4px each.
// The world scrolls vertically. Frog stays in lower portion of screen.
//
// World row pattern (repeating every 12 rows, starting at row 1):
//   Row 0         : START safe zone
//   Rows 1-5      : Road section 0 (5 car lanes)
//   Row 6         : Safe zone
//   Rows 7-11     : River section 0 (5 log lanes)
//   Row 12        : Safe zone
//   Rows 13-17    : Road section 1 (faster)
//   Row 18        : Safe zone
//   ...and so on forever

#define ROW_H        4
#define SCORE_H      8
#define BOARD_W      64
#define VISIBLE_ROWS 14      // rows that fit on screen below score bar
#define FROG_W       4
#define MAX_OBJS     3       // objects per lane

// ── Lane types ──
#define LANE_SAFE  0
#define LANE_ROAD  1
#define LANE_RIVER 2

enum FroggerState { FROG_WAITING, FROG_INTRO, FROG_PLAYING, FROG_DYING, FROG_GAMEOVER };

struct LaneData {
    int world_row;           // -1 = uninitialized
    int type;                // LANE_SAFE, LANE_ROAD, LANE_RIVER
    int dir;                 // +1 right, -1 left
    int speed;               // ms per pixel move
    uint8_t cr, cg, cb;      // object color
    int obj_count;
    int obj_x[MAX_OBJS];     // pixel x positions
    int obj_w[MAX_OBJS];     // width of each object (per-object)
    unsigned long last_move;  // last time objects moved
};

// Simple hash for deterministic per-row randomness
static unsigned int rowHash(int row, int salt) {
    unsigned int h = (unsigned int)(row * 2654435761u + salt * 340573321u);
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
};

#define LANE_BUF 20
static LaneData lane_buf[LANE_BUF];

static int frog_world_row;   // world row the frog is on (0 = start, higher = further)
static int frog_x;           // pixel x
static int camera_row;       // world row shown at BOTTOM of visible area
static FroggerState frog_state;
static unsigned long frog_state_timer;
static unsigned long frog_dying_timer;
static int frog_score;
static int frog_best_row;    // highest world_row reached (for scoring)
static int frog_lives;

// Car colors (cycle through for variety)
static const uint8_t car_colors[][3] = {
    {255, 50, 50},    // red
    {255, 255, 50},   // yellow
    {200, 50, 200},   // purple
    {50, 200, 255},   // cyan
    {255, 150, 50},   // orange
};

// Helper: Calculate world properties for a given world_row with expanding sections.
// Sections start with 5 rows per block (road/river), and increase by 1 every 4 sections.
// Returns: section index, block size (number of road or river lanes in this section),
// and offset within the current block (0 to blockSize-1)
// Also returns whether it's a road block or river block, and if it's a safe zone.
static void getWorldPos(int world_row, int* section_out, int* block_size_out, 
                        bool* is_safe_out, bool* is_river_block_out, int* offset_out) {
    if (world_row <= 0) {
        *is_safe_out = true;
        return;
    }
    
    int current_row = 1;
    int section = 0;
    
    while (true) {
        int block_size = 5 + (section / 4); // Increases every 4 safe zones
        int section_length = block_size * 2 + 2; // Road block + safe + river block + safe
        
        if (world_row >= current_row && world_row < current_row + section_length) {
            // It's in this section
            *section_out = section;
            *block_size_out = block_size;
            
            int local_row = world_row - current_row;
            
            if (local_row == block_size || local_row == section_length - 1) {
                *is_safe_out = true;
                return;
            }
            
            *is_safe_out = false;
            if (local_row < block_size) {
                // Road block
                *is_river_block_out = false;
                *offset_out = local_row;
            } else {
                // River block
                *is_river_block_out = true;
                *offset_out = local_row - block_size - 1;
            }
            return;
        }
        
        current_row += section_length;
        section++;
    }
}

// ── Determine lane type/direction/speed/color from world row ──
static void getLaneInfo(int world_row, int* type, int* dir, int* speed,
                        uint8_t* cr, uint8_t* cg, uint8_t* cb) {
    bool is_safe, is_river;
    int section, block_size, offset;
    
    getWorldPos(world_row, &section, &block_size, &is_safe, &is_river, &offset);

    if (world_row <= 0 || is_safe) {
        *type = LANE_SAFE;
        return;
    }

    // Base speed per section: starts at 135ms, decreases continually
    // Use an asymptotic decay so it never reaches 0 but keeps getting harder
    int section_base = 40 + (int)(95.0f * powf(0.85f, section)); 

    // Per-lane random speed: varies wildly to create extreme speed differences
    // Varies by +/- 80ms at the start, shrinking slightly as base speed increases
    // so cars don't go backwards or break playability bounds.
    int spread = 60 + (section_base / 2);
    int lane_variation = (int)(rowHash(world_row, 99) % (spread * 2)) - spread;

    if (!is_river) {
        *type = LANE_ROAD;
        *dir = (offset % 2 == 0) ? 1 : -1;
        *speed = section_base + lane_variation;
        // Hard limits to keep playability
        if (*speed < 20) *speed = 20; // very fast
        if (*speed > 300) *speed = 300; // very slow
        int ci = (section * 5 + offset) % 5;
        *cr = car_colors[ci][0];
        *cg = car_colors[ci][1];
        *cb = car_colors[ci][2];
    } else {
        *type = LANE_RIVER;
        *dir = (offset % 2 == 0) ? -1 : 1; // Alternating direction
        *speed = section_base + lane_variation;
        // Hard limits to keep playability
        if (*speed < 20) *speed = 20; // very fast
        if (*speed > 300) *speed = 300; // very slow
        *cr = 139; *cg = 90; *cb = 43;
    }
}

// ── Find or create lane data for a given world row ──
static LaneData* getLane(int world_row) {
    // Check if already in buffer
    for (int i = 0; i < LANE_BUF; i++) {
        if (lane_buf[i].world_row == world_row) return &lane_buf[i];
    }

    // Find an unused slot or the one furthest from camera
    int best = 0;
    int best_dist = 0;
    for (int i = 0; i < LANE_BUF; i++) {
        if (lane_buf[i].world_row == -1) {
            best = i;
            break;
        }
        int dist = abs(lane_buf[i].world_row - frog_world_row);
        if (dist > best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    // Initialize the lane
    LaneData* l = &lane_buf[best];
    l->world_row = world_row;
    l->last_move = 0;

    int type, dir, speed;
    uint8_t cr, cg, cb;
    getLaneInfo(world_row, &type, &dir, &speed, &cr, &cg, &cb);

    l->type = type;
    l->dir = dir;
    l->speed = speed;
    l->cr = cr; l->cg = cg; l->cb = cb;

    if (type == LANE_SAFE) {
        l->obj_count = 0;
    } else {
        bool is_safe, is_river;
        int section, block_size, offset;
        getWorldPos(world_row, &section, &block_size, &is_safe, &is_river, &offset);
        
        // Variability increases with section
        int var = section;  // 0 at start, grows
        if (var > 8) var = 8;

        if (type == LANE_ROAD) {
            // Cars: 2-4, more likely to have more at higher sections (increased base frequency)
            l->obj_count = 2 + (int)(rowHash(world_row, 1) % (3 + var)) / (2 + var/2);
            if (l->obj_count > MAX_OBJS) l->obj_count = MAX_OBJS;
            // Car widths: base 4-8, variability grows, but shrinks if speed is very high
            for (int j = 0; j < l->obj_count; j++) {
                int base_w = 4 + (int)(rowHash(world_row, 10 + j) % 5);
                int extra = (int)(rowHash(world_row, 50 + j) % (1 + var));
                
                // If this lane is moving very fast (low ms/pixel), shrink the cars
                // so the game remains possible.
                int speed_shrink = 0;
                if (l->speed < 60) speed_shrink = 2;
                else if (l->speed < 80) speed_shrink = 1;
                
                l->obj_w[j] = base_w + extra - speed_shrink;
                if (l->obj_w[j] < 3) l->obj_w[j] = 3;  // absolute min car size
                if (l->obj_w[j] > 14) l->obj_w[j] = 14;
            }
        } else {
            // Logs: 1-3 (decreased frequency by ~20%)
            // We use 1 + a random number 0-2 (skewed slightly higher to not make it impossible)
            int r = (int)(rowHash(world_row, 2) % 10);
            l->obj_count = (r < 2) ? 1 : ((r < 7) ? 2 : 3);
            if (l->obj_count > MAX_OBJS) l->obj_count = MAX_OBJS;
            for (int j = 0; j < l->obj_count; j++) {
                // Logs base width 6-13 (much smaller average)
                int base_w = 6 + (int)(rowHash(world_row, 20 + j) % 8);
                // In higher sections, chance for logs to be extremely small,
                // but some can remain average size for variety.
                int shrink = (int)(rowHash(world_row, 60 + j) % (1 + var)); 
                l->obj_w[j] = base_w - shrink;
                
                // Allow very small boats, especially when fast
                if (l->obj_w[j] < 2) l->obj_w[j] = 2; 
                // Cap max size so they don't get too big
                if (l->obj_w[j] > 14) l->obj_w[j] = 14; 
            }
        }

        // Randomized spacing with variable gaps
        int total_obj_width = 0;
        for (int j = 0; j < l->obj_count; j++) total_obj_width += l->obj_w[j];
        int total_gap = BOARD_W - total_obj_width;
        if (total_gap < l->obj_count * 4) total_gap = l->obj_count * 4;

        int cursor = (int)(rowHash(world_row, 42) % 20);  // random start offset
        for (int j = 0; j < l->obj_count; j++) {
            l->obj_x[j] = cursor;
            // Variable gap: base even gap +/- random spread
            int base_gap = total_gap / l->obj_count;
            int spread = 1 + var * 2;  // more spread at higher sections
            int gap = base_gap + (int)(rowHash(world_row, 30 + j) % spread) - spread / 2;
            if (gap < 4) gap = 4;
            cursor += l->obj_w[j] + gap;
        }
        // Wrap positions into valid range
        for (int j = 0; j < l->obj_count; j++) {
            l->obj_x[j] = l->obj_x[j] % (BOARD_W + 10);
        }
    }

    return l;
}

static void resetFrog() {
    frog_world_row = 0;
    frog_x = 30;
    camera_row = 0;
    frog_best_row = 0;
}

void froggerInit() {
    frog_score = 0;
    frog_lives = 3;
    for (int i = 0; i < LANE_BUF; i++) lane_buf[i].world_row = -1;
    resetFrog();
    frog_state = FROG_WAITING;
    frog_state_timer = 0;
}

void froggerSetDirection(int dx, int dy) {
    if (frog_state != FROG_PLAYING) return;

    if (dy < 0) { // UP = advance forward
        frog_world_row++;
        if (frog_world_row > frog_best_row) {
            frog_best_row = frog_world_row;
            frog_score += 10;
        }
        // Scroll camera if frog is too high on screen
        int frog_screen_row = frog_world_row - camera_row;
        if (frog_screen_row > 6) { // Start following much earlier
            camera_row = frog_world_row - 6;
        }
    } else if (dy > 0) { // DOWN = go back
        if (frog_world_row > camera_row) {
            frog_world_row--;
        }
    } else if (dx < 0) { // LEFT
        frog_x -= 4;
        if (frog_x < 0) frog_x = 0;
    } else if (dx > 0) { // RIGHT
        frog_x += 4;
        if (frog_x > BOARD_W - FROG_W) frog_x = BOARD_W - FROG_W;
    }
}

void froggerHandleEnter() {
    if (frog_state == FROG_WAITING) {
        frog_state = FROG_INTRO;
        frog_state_timer = 0;
    }
}

static void killFrog(unsigned long now) {
    frog_lives--;
    if (frog_lives <= 0) {
        frog_state = FROG_GAMEOVER;
        frog_state_timer = now;
    } else {
        frog_state = FROG_DYING;
        frog_dying_timer = now;
    }
}

void froggerLoop(unsigned long now) {
    if (lbIsActive()) {
        lbLoop(now);
        return;
    }

    // ── State Machine ──
    if (frog_state == FROG_GAMEOVER) {
        if (frog_state_timer == 0) frog_state_timer = now;
        if (now - frog_state_timer > 3000) {
            lbStart("frg_lb.txt", frog_score, [](){ froggerInit(); });
            return;
        }
    } else if (frog_state == FROG_DYING) {
        if (now - frog_dying_timer > 1000) {
            // Respawn at nearest safe row at or below current position
            // Find the nearest safe row <= frog_world_row
            while (frog_world_row > 0) {
                int type, dir, speed;
                uint8_t cr, cg, cb;
                getLaneInfo(frog_world_row, &type, &dir, &speed, &cr, &cg, &cb);
                if (type == LANE_SAFE) break;
                frog_world_row--;
            }
            frog_x = 30;
            frog_state = FROG_PLAYING;
        }
    } else if (frog_state == FROG_WAITING) {
        // wait for enter
    } else if (frog_state == FROG_INTRO) {
        if (frog_state_timer == 0) frog_state_timer = now;
        if (now - frog_state_timer > 2000) {
            frog_state = FROG_PLAYING;
        }
    } else if (frog_state == FROG_PLAYING) {
        // ── Update lane objects ──
        for (int r = camera_row; r < camera_row + VISIBLE_ROWS + 2; r++) {
            LaneData* l = getLane(r);
            if (l->type == LANE_SAFE || l->obj_count == 0) continue;

            if (l->last_move == 0) l->last_move = now;
            if (now - l->last_move >= (unsigned long)l->speed) {
                l->last_move = now;
                for (int j = 0; j < l->obj_count; j++) {
                    l->obj_x[j] += l->dir;
                    // Wrap
                    if (l->dir > 0 && l->obj_x[j] > BOARD_W) {
                        l->obj_x[j] = -l->obj_w[j];
                    }
                    if (l->dir < 0 && l->obj_x[j] + l->obj_w[j] < 0) {
                        l->obj_x[j] = BOARD_W;
                    }
                }
                // Carry frog on river
                if (l->type == LANE_RIVER && frog_world_row == l->world_row) {
                    frog_x += l->dir;
                }
            }
        }

        // ── Collision ──
        LaneData* cur = getLane(frog_world_row);
        if (cur->type == LANE_ROAD) {
            for (int j = 0; j < cur->obj_count; j++) {
                int ox = cur->obj_x[j];
                if (frog_x + FROG_W > ox && frog_x < ox + cur->obj_w[j]) {
                    killFrog(now);
                    break;
                }
            }
        } else if (cur->type == LANE_RIVER) {
            bool on_log = false;
            for (int j = 0; j < cur->obj_count; j++) {
                int ox = cur->obj_x[j];
                int fcx = frog_x + FROG_W / 2;
                if (fcx >= ox && fcx < ox + cur->obj_w[j]) {
                    on_log = true;
                    break;
                }
            }
            if (!on_log) killFrog(now);
        }

        // Off screen from log carry
        if (frog_state == FROG_PLAYING) {
            if (frog_x < -FROG_W || frog_x > BOARD_W) {
                killFrog(now);
            }
        }
    }

    // ── DRAWING ──
    screenClearCallback();

    // Score bar
    lbDrawString(2, 1, "SCORE", 150, 150, 150);
    lbDrawNumber(24, 1, frog_score, 255, 255, 255);
    // Lives
    for (int l = 0; l < frog_lives - 1; l++) {
        int lx = 60 - l * 5;
        drawPixelCallback(lx, 2, 0, 255, 0);
        drawPixelCallback(lx+1, 2, 0, 255, 0);
        drawPixelCallback(lx, 3, 0, 255, 0);
        drawPixelCallback(lx+1, 3, 0, 255, 0);
    }

    // Separator
    for (int x = 0; x < 64; x += 2) {
        drawPixelCallback(x, 7, 100, 100, 100);
    }

    // Draw visible rows (bottom-up: camera_row at bottom, higher rows above)
    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int world_r = camera_row + i;
        int screen_y = SCORE_H + (VISIBLE_ROWS - 1 - i) * ROW_H;  // row 0 of visible = bottom

        LaneData* l = getLane(world_r);

        // Background
        if (l->type == LANE_SAFE) {
            for (int x = 0; x < BOARD_W; x++)
                for (int dy = 0; dy < ROW_H; dy++)
                    drawPixelCallback(x, screen_y + dy, 0, 70, 0);
        } else if (l->type == LANE_ROAD) {
            for (int x = 0; x < BOARD_W; x++)
                for (int dy = 0; dy < ROW_H; dy++)
                    drawPixelCallback(x, screen_y + dy, 40, 40, 40);
        } else { // LANE_RIVER
            for (int x = 0; x < BOARD_W; x++)
                for (int dy = 0; dy < ROW_H; dy++)
                    drawPixelCallback(x, screen_y + dy, 0, 0, 80);
        }

        // Objects
        for (int j = 0; j < l->obj_count; j++) {
            int ox = l->obj_x[j];
            for (int dx = 0; dx < l->obj_w[j]; dx++) {
                int px = ox + dx;
                if (px >= 0 && px < BOARD_W) {
                    for (int dy = 0; dy < ROW_H; dy++) {
                        drawPixelCallback(px, screen_y + dy, l->cr, l->cg, l->cb);
                    }
                }
            }
        }
    }

    // Draw frog
    int frog_screen_i = frog_world_row - camera_row;  // index from bottom
    int frog_sy = SCORE_H + (VISIBLE_ROWS - 1 - frog_screen_i) * ROW_H;

    if (frog_state == FROG_DYING) {
        bool flash = ((now - frog_dying_timer) / 150) % 2 == 0;
        if (flash) {
            drawPixelCallback(frog_x, frog_sy, 255, 0, 0);
            drawPixelCallback(frog_x+3, frog_sy, 255, 0, 0);
            drawPixelCallback(frog_x+1, frog_sy+1, 255, 0, 0);
            drawPixelCallback(frog_x+2, frog_sy+1, 255, 0, 0);
            drawPixelCallback(frog_x+1, frog_sy+2, 255, 0, 0);
            drawPixelCallback(frog_x+2, frog_sy+2, 255, 0, 0);
            drawPixelCallback(frog_x, frog_sy+3, 255, 0, 0);
            drawPixelCallback(frog_x+3, frog_sy+3, 255, 0, 0);
        }
    } else if (frog_state != FROG_GAMEOVER || ((now / 300) % 2 == 0)) {
        // Eyes
        drawPixelCallback(frog_x, frog_sy, 255, 255, 255);
        drawPixelCallback(frog_x+3, frog_sy, 255, 255, 255);
        drawPixelCallback(frog_x+1, frog_sy, 0, 255, 0);
        drawPixelCallback(frog_x+2, frog_sy, 0, 255, 0);
        // Body
        drawPixelCallback(frog_x, frog_sy+1, 0, 200, 0);
        drawPixelCallback(frog_x+1, frog_sy+1, 0, 180, 0);
        drawPixelCallback(frog_x+2, frog_sy+1, 0, 180, 0);
        drawPixelCallback(frog_x+3, frog_sy+1, 0, 200, 0);
        drawPixelCallback(frog_x, frog_sy+2, 0, 200, 0);
        drawPixelCallback(frog_x+1, frog_sy+2, 0, 180, 0);
        drawPixelCallback(frog_x+2, frog_sy+2, 0, 180, 0);
        drawPixelCallback(frog_x+3, frog_sy+2, 0, 200, 0);
        // Legs
        drawPixelCallback(frog_x, frog_sy+3, 0, 200, 0);
        drawPixelCallback(frog_x+3, frog_sy+3, 0, 200, 0);
    }

    // Overlay text
    if (frog_state == FROG_WAITING) {
        lbDrawBox(10, 28, 31, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(12, 30, "FROGGER", 255, 255, 0);
        lbDrawBox(2, 40, 47, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(4, 42, "PRESS ENTER", 255, 255, 255);
    } else if (frog_state == FROG_INTRO) {
        lbDrawBox(10, 32, 39, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(12, 34, "GET READY", 255, 255, 0);
    } else if (frog_state == FROG_GAMEOVER) {
        lbDrawBox(10, 32, 39, 9, 0, 0, 0, true, 255, 255, 255);
        lbDrawString(12, 34, "GAME OVER", 255, 0, 0);
    }

    updateScreenCallback();
}
