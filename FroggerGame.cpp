#include "FroggerGame.h"
#include <stdint.h>
#include <stdlib.h>
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
    int obj_w;               // width of each object
    unsigned long last_move;  // last time objects moved
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

// ── Determine lane properties from world row ──
static void getLaneInfo(int world_row, int* type, int* dir, int* speed,
                        uint8_t* cr, uint8_t* cg, uint8_t* cb,
                        int* obj_count, int* obj_w) {
    if (world_row <= 0) {
        *type = LANE_SAFE;
        return;
    }

    int offset = (world_row - 1) % 12;
    int section = (world_row - 1) / 12;  // 0, 1, 2, ... increases = harder

    if (offset == 5 || offset == 11) {
        // Safe zone between road and river
        *type = LANE_SAFE;
        return;
    }

    // Speed: starts at 180ms, decreases by 3 per section, min 50ms
    int base = 180 - section * 3;
    if (base < 50) base = 50;

    if (offset < 5) {
        // Road lane
        *type = LANE_ROAD;
        *dir = (offset % 2 == 0) ? 1 : -1;
        *speed = base - offset * 5;  // slight variation within section
        if (*speed < 50) *speed = 50;
        int ci = (section * 5 + offset) % 5;
        *cr = car_colors[ci][0];
        *cg = car_colors[ci][1];
        *cb = car_colors[ci][2];
        *obj_count = 2;
        *obj_w = 5 + (offset % 3);  // 5-7px wide cars
    } else {
        // River lane (offset 6-10)
        int river_idx = offset - 6;
        *type = LANE_RIVER;
        *dir = (river_idx % 2 == 0) ? -1 : 1;
        *speed = base - river_idx * 5;
        if (*speed < 50) *speed = 50;
        *cr = 139; *cg = 90; *cb = 43;  // brown logs
        *obj_count = 2;
        *obj_w = 14 + (river_idx % 3) * 2;  // 14-18px wide logs
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

    int type, dir, speed, obj_count, obj_w;
    uint8_t cr, cg, cb;
    getLaneInfo(world_row, &type, &dir, &speed, &cr, &cg, &cb, &obj_count, &obj_w);

    l->type = type;
    l->dir = dir;
    l->speed = speed;
    l->cr = cr; l->cg = cg; l->cb = cb;
    l->obj_count = (type == LANE_SAFE) ? 0 : obj_count;
    l->obj_w = obj_w;

    // Space objects evenly
    if (l->obj_count > 0) {
        int spacing = BOARD_W / l->obj_count;
        for (int j = 0; j < l->obj_count; j++) {
            l->obj_x[j] = j * spacing + (world_row * 7) % 13; // pseudo-random offset
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
        if (frog_screen_row > VISIBLE_ROWS - 4) {
            camera_row = frog_world_row - (VISIBLE_ROWS - 4);
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
                int type, dir, speed, obj_count, obj_w;
                uint8_t cr, cg, cb;
                getLaneInfo(frog_world_row, &type, &dir, &speed, &cr, &cg, &cb, &obj_count, &obj_w);
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
                        l->obj_x[j] = -l->obj_w;
                    }
                    if (l->dir < 0 && l->obj_x[j] + l->obj_w < 0) {
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
                if (frog_x + FROG_W > ox && frog_x < ox + cur->obj_w) {
                    killFrog(now);
                    break;
                }
            }
        } else if (cur->type == LANE_RIVER) {
            bool on_log = false;
            for (int j = 0; j < cur->obj_count; j++) {
                int ox = cur->obj_x[j];
                int fcx = frog_x + FROG_W / 2;
                if (fcx >= ox && fcx < ox + cur->obj_w) {
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
            for (int dx = 0; dx < l->obj_w; dx++) {
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
        lbDrawString(12, 30, "FROGGER", 255, 255, 0);
        lbDrawString(4, 42, "PRESS ENTER", 255, 255, 255);
    } else if (frog_state == FROG_INTRO) {
        lbDrawString(12, 34, "GET READY", 255, 255, 0);
    } else if (frog_state == FROG_GAMEOVER) {
        lbDrawString(12, 34, "GAME OVER", 255, 0, 0);
    }

    updateScreenCallback();
}
