#include "Leaderboard.h"
#include <SD.h>
#include <string.h>
#include <stdlib.h>

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

static const uint8_t font3x5[16][15] = {
    {1,1,1, 1,0,1, 1,0,1, 1,0,1, 1,1,1}, // 0
    {0,1,0, 1,1,0, 0,1,0, 0,1,0, 1,1,1}, // 1
    {1,1,1, 0,0,1, 1,1,1, 1,0,0, 1,1,1}, // 2
    {1,1,1, 0,0,1, 1,1,1, 0,0,1, 1,1,1}, // 3
    {1,0,1, 1,0,1, 1,1,1, 0,0,1, 0,0,1}, // 4
    {1,1,1, 1,0,0, 1,1,1, 0,0,1, 1,1,1}, // 5
    {1,1,1, 1,0,0, 1,1,1, 1,0,1, 1,1,1}, // 6
    {1,1,1, 0,0,1, 0,1,0, 0,1,0, 0,1,0}, // 7
    {1,1,1, 1,0,1, 1,1,1, 1,0,1, 1,1,1}, // 8
    {1,1,1, 1,0,1, 1,1,1, 0,0,1, 1,1,1}, // 9
    {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0}, // 10
    {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0}, // 11
    {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0}, // 12
    {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0}, // 13
    {0,0,0, 0,0,0, 0,0,0, 0,0,0, 0,0,0}, // 14
    {0,1,0, 0,1,0, 0,1,0, 0,0,0, 0,1,0}  // 15 = !
};

static const uint8_t font_alpha[26][15] = {
    {1,1,1, 1,0,1, 1,1,1, 1,0,1, 1,0,1}, // A
    {1,1,0, 1,0,1, 1,1,0, 1,0,1, 1,1,0}, // B
    {1,1,1, 1,0,0, 1,0,0, 1,0,0, 1,1,1}, // C
    {1,1,0, 1,0,1, 1,0,1, 1,0,1, 1,1,0}, // D
    {1,1,1, 1,0,0, 1,1,1, 1,0,0, 1,1,1}, // E
    {1,1,1, 1,0,0, 1,1,1, 1,0,0, 1,0,0}, // F
    {1,1,1, 1,0,0, 1,0,1, 1,0,1, 1,1,1}, // G
    {1,0,1, 1,0,1, 1,1,1, 1,0,1, 1,0,1}, // H
    {1,1,1, 0,1,0, 0,1,0, 0,1,0, 1,1,1}, // I
    {0,0,1, 0,0,1, 0,0,1, 1,0,1, 1,1,1}, // J
    {1,0,1, 1,0,1, 1,1,0, 1,0,1, 1,0,1}, // K
    {1,0,0, 1,0,0, 1,0,0, 1,0,0, 1,1,1}, // L
    {1,1,1, 1,1,1, 1,0,1, 1,0,1, 1,0,1}, // M
    {1,1,1, 1,0,1, 1,0,1, 1,0,1, 1,0,1}, // N
    {1,1,1, 1,0,1, 1,0,1, 1,0,1, 1,1,1}, // O
    {1,1,1, 1,0,1, 1,1,1, 1,0,0, 1,0,0}, // P
    {1,1,1, 1,0,1, 1,0,1, 1,1,1, 0,0,1}, // Q
    {1,1,1, 1,0,1, 1,1,0, 1,0,1, 1,0,1}, // R
    {1,1,1, 1,0,0, 1,1,1, 0,0,1, 1,1,1}, // S
    {1,1,1, 0,1,0, 0,1,0, 0,1,0, 0,1,0}, // T
    {1,0,1, 1,0,1, 1,0,1, 1,0,1, 1,1,1}, // U
    {1,0,1, 1,0,1, 1,0,1, 1,0,1, 0,1,0}, // V
    {1,0,1, 1,0,1, 1,0,1, 1,1,1, 1,1,1}, // W
    {1,0,1, 1,0,1, 0,1,0, 1,0,1, 1,0,1}, // X
    {1,0,1, 1,0,1, 1,1,1, 0,1,0, 0,1,0}, // Y
    {1,1,1, 0,0,1, 0,1,0, 1,0,0, 1,1,1}  // Z
};

void lbDrawChar(int x, int y, char c, uint8_t r, uint8_t g, uint8_t b) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= 'A' && c <= 'Z') {
        int idx = c - 'A';
        for(int dy=0; dy<5; dy++) {
            for(int dx=0; dx<3; dx++) {
                if(font_alpha[idx][dy*3+dx]) drawPixelCallback(x+dx, y+dy, r, g, b);
            }
        }
    } else if (c >= '0' && c <= '9') {
        int idx = c - '0';
        for(int dy=0; dy<5; dy++) {
            for(int dx=0; dx<3; dx++) {
                if(font3x5[idx][dy*3+dx]) drawPixelCallback(x+dx, y+dy, r, g, b);
            }
        }
    } else if (c == '!') {
        for(int dy=0; dy<5; dy++) {
            for(int dx=0; dx<3; dx++) {
                if(font3x5[15][dy*3+dx]) drawPixelCallback(x+dx, y+dy, r, g, b);
            }
        }
    }
}

void lbDrawString(int x, int y, const char* str, uint8_t r, uint8_t g, uint8_t b) {
    while(*str) {
        if (*str != ' ') lbDrawChar(x, y, *str, r, g, b);
        x += 4;
        str++;
    }
}

void lbDrawNumber(int x, int y, int num, uint8_t r, uint8_t g, uint8_t b) {
    if (num == 0) {
        lbDrawChar(x, y, '0', r, g, b);
        return;
    }
    int digits[10];
    int count = 0;
    while(num > 0) {
        digits[count++] = num % 10;
        num /= 10;
    }
    for(int i=count-1; i>=0; i--) {
        lbDrawChar(x, y, '0' + digits[i], r, g, b);
        x += 4;
    }
}

struct LeaderboardEntry {
    char name[4];
    int score;
};

static LeaderboardEntry lb[5];
static bool lb_has_sd = false;
static char lb_filename[32] = "";
static int lb_current_score = 0;
static bool lb_was_in_top_5 = false;
static unsigned long lb_state_timer = 0;
static void (*lb_onComplete)() = nullptr;
static int lb_state = 0; // 0=NONE, 1=ENTER_NAME, 2=SHOW_BOARD

void lbInit(bool has_sd) {
    lb_has_sd = has_sd;
}

static void loadLeaderboard() {
    for(int i=0; i<5; i++) {
        strcpy(lb[i].name, "---");
        lb[i].score = 0;
    }
    if (!lb_has_sd || lb_filename[0] == 0) return;
    
    File f = SD.open(lb_filename, FILE_READ);
    if (f) {
        for(int i=0; i<5; i++) {
            char buf[32];
            int idx = 0;
            while(f.available()) {
                char c = f.read();
                if (c == '\n' || idx >= 31) { buf[idx] = 0; break; }
                if (c != '\r') buf[idx++] = c;
            }
            if (idx == 0) break;
            char* space = strchr(buf, ' ');
            if (space) {
                *space = 0;
                strncpy(lb[i].name, buf, 3);
                lb[i].name[3] = 0;
                lb[i].score = atoi(space+1);
            }
        }
        f.close();
    }
}

static void saveLeaderboard() {
    if (!lb_has_sd || lb_filename[0] == 0) return;
    if (SD.exists(lb_filename)) {
        SD.remove(lb_filename);
    }
    File f = SD.open(lb_filename, FILE_WRITE);
    if (f) {
        for(int i=0; i<5; i++) {
            f.print(lb[i].name);
            f.print(" ");
            f.print(lb[i].score);
            f.print("\n");
        }
        f.close();
    }
}

void lbStart(const char* filename, int score, void (*onComplete)()) {
    strncpy(lb_filename, filename, 31);
    lb_filename[31] = 0;
    lb_current_score = score;
    lb_onComplete = onComplete;
    
    loadLeaderboard();
    if (score > lb[4].score) {
        lb_was_in_top_5 = true;
        lb_state = 1; // ENTER_NAME
    } else {
        lb_was_in_top_5 = false;
        lb_state = 2; // SHOW_BOARD
        lb_state_timer = 0; // will set below
    }
}

void lbHandleText(const char* text) {
    if (lb_state == 1) { // ENTER_NAME
        char buf[4] = {0};
        int idx = 0;
        for (int i=0; text[i] && idx < 3; i++) {
            char c = text[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                buf[idx++] = c;
            }
        }
        if (idx > 0) {
            // insert
            for(int i=0; i<5; i++) {
                if (lb_current_score > lb[i].score) {
                    for(int j=4; j>i; j--) {
                        lb[j] = lb[j-1];
                    }
                    strcpy(lb[i].name, buf);
                    lb[i].score = lb_current_score;
                    break;
                }
            }
            saveLeaderboard();
            lb_state = 2; // SHOW_BOARD
            lb_state_timer = 0; // trigger immediate display
        }
    }
}

bool lbIsActive() {
    return lb_state != 0;
}

void lbLoop(unsigned long now) {
    if (lb_state == 0) return;
    
    if (lb_state == 1) {
        screenClearCallback();
        lbDrawString(10, 10, "NEW HIGH SCORE", 255, 255, 0);
        lbDrawString(10, 30, "ENTER NAME", 255, 255, 255);
        lbDrawString(10, 40, "VIA BLE", 255, 255, 255);
        lbDrawNumber(20, 50, lb_current_score, 0, 255, 255);
        updateScreenCallback();
    } else if (lb_state == 2) {
        if (lb_state_timer == 0) {
            lb_state_timer = now;
        }
        screenClearCallback();
        lbDrawString(12, 2, "TOP SCORES", 255, 255, 0);
        for(int i=0; i<5; i++) {
            int ry = 14 + i * 8;
            lbDrawString(8, ry, lb[i].name, 255, 255, 255);
            lbDrawNumber(28, ry, lb[i].score, 0, 255, 255);
        }
        if (!lb_was_in_top_5) {
            for(int i=4; i<60; i+=2) {
                drawPixelCallback(i, 54, 150, 150, 150);
            }
            lbDrawString(2, 57, "YOUR SCORE", 255, 0, 0);
            lbDrawNumber(44, 57, lb_current_score, 0, 255, 255);
        }
        updateScreenCallback();
        
        if (now - lb_state_timer > 5000) {
            lb_state = 0;
            if (lb_onComplete) {
                lb_onComplete();
            }
        }
    }
}
