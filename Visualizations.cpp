#include "Visualizations.h"
#include <math.h>
#include <stdint.h>

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

static int current_vis = 0;
static const int NUM_VIS = 2;

// Helper: HSV to RGB
static void HSVtoRGB(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
    int i;
    float f, p, q, t;
    if (s == 0) {
        *r = *g = *b = v * 255;
        return;
    }
    h /= 60.0f;
    i = floor(h);
    f = h - i;
    p = v * (1.0f - s);
    q = v * (1.0f - s * f);
    t = v * (1.0f - s * (1.0f - f));
    float r_f, g_f, b_f;
    switch (i) {
        case 0: r_f = v; g_f = t; b_f = p; break;
        case 1: r_f = q; g_f = v; b_f = p; break;
        case 2: r_f = p; g_f = v; b_f = t; break;
        case 3: r_f = p; g_f = q; b_f = v; break;
        case 4: r_f = t; g_f = p; b_f = v; break;
        default: r_f = v; g_f = p; b_f = q; break;
    }
    *r = r_f * 255;
    *g = g_f * 255;
    *b = b_f * 255;
}

void visInit() {
    current_vis = 0;
}

void visHandleInput(int dx, int dy, bool enter) {
    if (dx > 0) {
        current_vis = (current_vis + 1) % NUM_VIS;
    } else if (dx < 0) {
        current_vis = (current_vis - 1 + NUM_VIS) % NUM_VIS;
    }
}

static void drawPlasma(unsigned long now) {
    float t = now / 1000.0f; // time in seconds
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            float v = 0;
            v += sin((x + t * 10.0f) * 0.2f);
            v += sin((y + t * 10.0f) * 0.2f);
            v += sin((x + y + t * 10.0f) * 0.2f);
            float cx = x + 32.0f * sin(t / 5.0f);
            float cy = y + 32.0f * cos(t / 3.0f);
            v += sin(sqrt(cx * cx + cy * cy) * 0.2f);
            
            // v is roughly -4 to 4, map to hue 0-360
            float hue = (v + 4.0f) / 8.0f * 360.0f;
            // animate hue over time
            hue = fmod(hue + t * 100.0f, 360.0f);
            if (hue < 0) hue += 360.0f;
            
            uint8_t r, g, b;
            HSVtoRGB(hue, 1.0f, 1.0f, &r, &g, &b);
            drawPixelCallback(x, y, r, g, b);
        }
    }
}

static void drawConcentric(unsigned long now) {
    float t = now / 500.0f; // faster time for rings
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            float dx = x - 31.5f;
            float dy = y - 31.5f;
            float dist = sqrt(dx*dx + dy*dy);
            
            // hue based on distance and time (expanding rings)
            float hue = fmod(dist * 15.0f - t * 150.0f, 360.0f);
            if (hue < 0) hue += 360.0f;
            
            // pulse brightness (value) for extra trippiness
            float val = 0.5f + 0.5f * sin(dist * 0.5f - t * 5.0f);
            
            uint8_t r, g, b;
            HSVtoRGB(hue, 1.0f, val, &r, &g, &b);
            drawPixelCallback(x, y, r, g, b);
        }
    }
}

void visLoop(unsigned long now) {
    screenClearCallback();
    
    if (current_vis == 0) {
        drawPlasma(now);
    } else if (current_vis == 1) {
        drawConcentric(now);
    }
    
    updateScreenCallback();
}
