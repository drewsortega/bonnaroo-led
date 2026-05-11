#include "TextDisplay.h"
#include <MatrixHardware_Teensy4_ShieldV5.h>
#include <SmartMatrix.h>

extern SM_RGB backgroundLayer;

struct TextCell {
    char c;
    rgb24 color;
};

static uint8_t text_font_id = 0;
static rgb24 text_bg = {0,0,0};
static uint8_t text_rows = 0;
static uint8_t text_cols = 0;
static TextCell text_grid[200];
static bool text_loaded = false;
static unsigned long last_text_draw = 0;

void textInit(bool use_sd) {
    text_loaded = false;
    if (!use_sd) return;
    
    File f = SD.open("/gifs/txt.bin", FILE_READ);
    if (!f) return;
    
    if (f.available() >= 6) {
        text_font_id = f.read();
        text_bg.red = f.read();
        text_bg.green = f.read();
        text_bg.blue = f.read();
        text_rows = f.read();
        text_cols = f.read();
        
        int total_cells = text_rows * text_cols;
        if (total_cells > 200) total_cells = 200; // safety
        
        for (int i = 0; i < total_cells; i++) {
            if (f.available() >= 4) {
                text_grid[i].c = (char)f.read();
                text_grid[i].color.red = f.read();
                text_grid[i].color.green = f.read();
                text_grid[i].color.blue = f.read();
            } else {
                text_grid[i].c = ' ';
                text_grid[i].color = {0,0,0};
            }
        }
        text_loaded = true;
    }
    f.close();
    
    backgroundLayer.fillScreen(COLOR_BLACK);
    backgroundLayer.swapBuffers();
    last_text_draw = 0; // force immediate draw
}

void textLoop(unsigned long now) {
    if (!text_loaded) return;
    
    // Only need to redraw occasionally to maintain the frame
    if (now - last_text_draw < 1000) return;
    last_text_draw = now;
    
    backgroundLayer.fillScreen(text_bg);
    
    int charWidth, charHeight;
    const bitmap_font* font;
    if (text_font_id == 0) {
        font = &font3x5;
        charWidth = 4;
        charHeight = 6;
    } else if (text_font_id == 1) {
        font = &font5x7;
        charWidth = 6;
        charHeight = 8;
    } else {
        font = &font8x13;
        charWidth = 9;
        charHeight = 14;
    }
    
    backgroundLayer.setFont(*font);
    
    int idx = 0;
    for (int r = 0; r < text_rows; r++) {
        for (int c = 0; c < text_cols; c++) {
            char ch[2] = { text_grid[idx].c, 0 };
            rgb24 fg = text_grid[idx].color;
            backgroundLayer.drawString(c * charWidth, r * charHeight, fg, ch);
            idx++;
            if (idx >= 200) break;
        }
        if (idx >= 200) break;
    }
    
    backgroundLayer.swapBuffers();
}

void textHandleInput(int dx, int dy, bool button) {
    // Interactive text mode not currently supported, but could be!
}
