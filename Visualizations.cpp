#include "Visualizations.h"
#include <math.h>
#include <stdint.h>

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

static int current_vis = 0;
static const int NUM_VIS = 5;

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

static void drawJulia(unsigned long now) {
    float t = now / 2000.0f;
    // Animate the Julia constant c
    float c_re = 0.7885f * cos(t);
    float c_im = 0.7885f * sin(t);
    
    int max_iter = 32;
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            // Map pixel to complex plane (-1.5 to 1.5)
            float z_re = 3.0f * (x - 32.0f) / 64.0f;
            float z_im = 3.0f * (y - 32.0f) / 64.0f;
            
            int iter = 0;
            while (z_re * z_re + z_im * z_im < 4.0f && iter < max_iter) {
                float next_re = z_re * z_re - z_im * z_im + c_re;
                float next_im = 2.0f * z_re * z_im + c_im;
                z_re = next_re;
                z_im = next_im;
                iter++;
            }
            
            if (iter == max_iter) {
                drawPixelCallback(x, y, 0, 0, 0); // Inside the set
            } else {
                // Smooth coloring based on iterations
                float hue = fmod((float)iter / max_iter * 360.0f + t * 100.0f, 360.0f);
                if (hue < 0) hue += 360.0f;
                
                // Create a glowing neon effect on a black background
                float val = 0.0f;
                if (iter > 4) {
                    val = (float)(iter - 4) / (max_iter - 4);
                    // Extremely aggressive climb to full brightness to fix the dimness
                    // without destroying the distinct separated shapes.
                    val = sqrt(sqrt(val)); 
                }
                
                uint8_t r, g, b;
                HSVtoRGB(hue, 1.0f, val, &r, &g, &b);
                drawPixelCallback(x, y, r, g, b);
            }
        }
    }
}

static uint8_t gol_grid_current[64][64];
static uint8_t gol_grid_next[64][64];
static uint8_t gol_fade[64][64];
static unsigned long gol_last_update = 0;
static unsigned long gol_last_seed = 0;
static void addRandomPocket(int cx, int cy) {
    for (int dy = -5; dy <= 5; dy++) {
        for (int dx = -5; dx <= 5; dx++) {
            if ((rand() % 100) < 30) {
                int x = (cx + dx + 64) % 64;
                int y = (cy + dy + 64) % 64;
                gol_grid_current[y][x] = 1;
            }
        }
    }
}

static unsigned long gol_stagnation_timer = 0;
static int gol_last_active_count = 0;

static void drawGameOfLife(unsigned long now) {
    if (gol_last_seed == 0) {
        // Randomly seed full board (V1 style)
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                gol_grid_current[y][x] = (rand() % 100) < 20 ? 1 : 0;
                gol_fade[y][x] = 0;
            }
        }

        gol_last_seed = now;
        gol_last_update = now;
        gol_stagnation_timer = now;
    }

    // Step every 50ms for smooth fast movement, typical for rave visuals
    if (now - gol_last_update > 50) {
        int active_cells = 0;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = (x + dx + 64) % 64;
                        int ny = (y + dy + 64) % 64;
                        neighbors += gol_grid_current[ny][nx];
                    }
                }
                
                if (gol_grid_current[y][x]) {
                    if (neighbors == 2 || neighbors == 3) {
                        gol_grid_next[y][x] = 1;
                        active_cells++;
                    } else {
                        gol_grid_next[y][x] = 0;
                    }
                } else {
                    if (neighbors == 3) {
                        gol_grid_next[y][x] = 1;
                        active_cells++;
                    } else {
                        gol_grid_next[y][x] = 0;
                    }
                }
            }
        }
        
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                gol_grid_current[y][x] = gol_grid_next[y][x];
                if (gol_grid_current[y][x]) {
                    gol_fade[y][x] = 255;
                } else {
                    // Fade out
                    if (gol_fade[y][x] > 15) {
                        gol_fade[y][x] -= 15;
                    } else {
                        gol_fade[y][x] = 0;
                    }
                }
            }
        }
        gol_last_update = now;
        
        if (active_cells != gol_last_active_count) {
            gol_last_active_count = active_cells;
            gol_stagnation_timer = now;
        }
        
        static unsigned long gol_spawn_timer = 0;
        // Inject a new pocket of life every 150ms to keep things constantly fresh and moving
        if (now - gol_spawn_timer > 150) {
            addRandomPocket(rand() % 64, rand() % 64);
            gol_spawn_timer = now;
        }
        
        // Anti-stagnation: If the board empties out or gets completely stuck
        if (active_cells < 100 || (now - gol_stagnation_timer > 2000)) {
            addRandomPocket(rand() % 64, rand() % 64);
            addRandomPocket(rand() % 64, rand() % 64);
            gol_stagnation_timer = now;
            gol_last_active_count = -1; // force reset
        }
    }
    
    // Draw
    float t = now / 1000.0f;
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            if (gol_fade[y][x] > 0) {
                // Dynamic hue based on coordinates and time
                float hue = fmod(x * 5.0f + y * 5.0f + t * 80.0f, 360.0f);
                float val = gol_fade[y][x] / 255.0f;
                // Square the value to make trails fade exponentially nicely
                val = val * val;
                uint8_t r, g, b;
                HSVtoRGB(hue, 1.0f, val, &r, &g, &b);
                drawPixelCallback(x, y, r, g, b);
            } else {
                drawPixelCallback(x, y, 0, 0, 0);
            }
        }
    }
}

static void drawMandelbrot(unsigned long now) {
    // "Endless Fractal Tunnel" using Log-Polar mapping and Modulo logic!
    // This converts the Cartesian screen into a tunnel, allowing us to use
    // fmod() to mathematically loop the zoom forever without hitting precision limits.
    float t = now / 1000.0f;
    
    int max_iter = 16; // Lower iteration is fine since the shapes are dense
    
    // Animate the Julia set constant so the tunnel walls constantly mutate
    float julia_c_re = -0.8f + 0.2f * sinf(t * 0.5f);
    float julia_c_im = 0.156f + 0.2f * cosf(t * 0.3f);
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            // Map pixel to center (-1.0 to 1.0)
            float px = (x - 31.5f) / 32.0f;
            float py = (y - 31.5f) / 32.0f;
            
            // Convert to polar coordinates for tunnel mapping
            float r = sqrtf(px * px + py * py);
            if (r < 0.001f) r = 0.001f; // Prevent log(0)
            float theta = atan2f(py, px);
            
            // Log-Polar Magic: log(r) makes zooming linear!
            float u = logf(r) * 2.5f - t * 2.0f; // The forward zoom
            
            // Map angle to a linear value and add spin
            float v = (theta + M_PI) / (2.0f * M_PI);
            v = v * 5.0f + t * 0.3f; // 5 spiral arms, slowly spinning
            
            // LIQUID GRID DISTORTION!
            // We warp the coordinate space before wrapping it. This causes the "seams"
            // and the areas between each fractal to stretch, wobble, and squirm dynamically!
            u += 0.3f * sinf(theta * 4.0f + t * 2.5f);
            v += 0.3f * cosf(logf(r) * 5.0f - t * 1.8f);
            
            // MODULO LOGIC: This wraps the tunnel endlessly!
            float u_wrap = fmodf(u + 10000.0f, 1.0f);
            float v_wrap = fmodf(v + 10000.0f, 1.0f);
            
            // Map the wrapped cell back to a fractal coordinate space
            float z_re = (u_wrap - 0.5f) * 4.0f;
            float z_im = (v_wrap - 0.5f) * 4.0f;
            
            // CONTINUOUS SPATIAL MUTATION!
            // We perturb the Julia constant based on the physical angle (theta)
            // and the physical depth (log(r)). This guarantees every single section 
            // of the tunnel morphs into a completely unique organic shape!
            float local_c_re = julia_c_re + 0.15f * sinf(logf(r) * 4.0f + t * 2.0f);
            float local_c_im = julia_c_im + 0.15f * cosf(theta * 3.0f - t * 1.5f);
            
            int iter = 0;
            
            // Run the fractal math on the tunnel walls
            while (z_re * z_re + z_im * z_im < 4.0f && iter < max_iter) {
                float next_re = z_re * z_re - z_im * z_im + local_c_re;
                float next_im = 2.0f * z_re * z_im + local_c_im;
                z_re = next_re;
                z_im = next_im;
                iter++;
            }
            
            if (iter == max_iter) {
                // Core of the fractal pattern
                drawPixelCallback(x, y, 0, 0, 0); 
            } else {
                // "Purple Chrome Blazing" color mapping
                float hue = 290.0f + 50.0f * sinf((float)iter * 0.5f - t * 2.0f);
                if (hue < 0.0f) hue += 360.0f;
                if (hue >= 360.0f) hue -= 360.0f;
                
                // Chrome shiny effect
                float val = 0.5f + 0.5f * cosf((float)iter * 0.8f);
                val = sqrtf(val); 
                
                // Fade to pure black in the deep center of the tunnel
                float depth = r * 2.0f;
                if (depth > 1.0f) depth = 1.0f;
                val *= depth; // Applies the darkness to the distance
                
                uint8_t r_col, g_col, b_col;
                HSVtoRGB(hue, 1.0f, val, &r_col, &g_col, &b_col);
                drawPixelCallback(x, y, r_col, g_col, b_col);
            }
        }
    }
}

void visLoop(unsigned long now) {
    screenClearCallback();
    
    if (current_vis == 0) {
        drawPlasma(now);
    } else if (current_vis == 1) {
        drawConcentric(now);
    } else if (current_vis == 2) {
        drawJulia(now);
    } else if (current_vis == 3) {
        drawGameOfLife(now);
    } else if (current_vis == 4) {
        drawMandelbrot(now);
    }
    
    updateScreenCallback();
}
