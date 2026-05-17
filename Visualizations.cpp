#include "Visualizations.h"
#include <math.h>
#include <stdint.h>

extern void screenClearCallback(void);
extern void updateScreenCallback(void);
extern void drawPixelCallback(int16_t x, int16_t y, uint8_t red, uint8_t green, uint8_t blue);

static int current_vis = 0;
static const int NUM_VIS = 7;

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

void visSetCurrent(int idx) {
    if (idx >= 0 && idx < NUM_VIS) {
        current_vis = idx;
    }
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

// -----------------------------------------------------------
// VISUALIZATION 6: INFINITE CUBIC MATRIX (3D Raymarching)
// -----------------------------------------------------------

static float boxSDF(float x, float y, float z, float s) {
    float dx = fabsf(x) - s;
    float dy = fabsf(y) - s;
    float dz = fabsf(z) - s;
    float max_d = fmaxf(dx, fmaxf(dy, dz));
    if (max_d <= 0.0f) return max_d; // Inside the box
    
    float bx = dx > 0.0f ? dx : 0.0f;
    float by = dy > 0.0f ? dy : 0.0f;
    float bz = dz > 0.0f ? dz : 0.0f;
    return sqrtf(bx*bx + by*by + bz*bz);
}

static float mapMenger(float x, float y, float z) {
    float spacing = 3.0f; 
    float inv_spacing = 0.3333333f;
    
    float nx = x + 30000.0f;
    float ny = y + 30000.0f;
    float nz = z + 30000.0f;
    
    // Fast hardware modulo using integer truncation (10x faster than fmodf)
    float cell_x = nx - (int)(nx * inv_spacing) * spacing - 1.5f;
    float cell_y = ny - (int)(ny * inv_spacing) * spacing - 1.5f;
    float cell_z = nz - (int)(nz * inv_spacing) * spacing - 1.5f;
    
    // Base cube size decreased to 0.8 so they don't fill the entire screen
    float d = boxSDF(cell_x, cell_y, cell_z, 0.8f);
    
    return d;
}

static void drawCubicMatrix(unsigned long now) {
    float t = now / 1000.0f;
    
    // Camera setup: endlessly flying forward through the infinite grid
    float ro_x = 0.0f;
    float ro_y = 0.0f;
    float ro_z = t * 3.0f; 
    
    // Reduced wobble so we stay cleanly inside the hallway gaps and don't crash into walls
    ro_x += 0.3f * sinf(t * 0.5f);
    ro_y += 0.3f * cosf(t * 0.3f);
    
    int max_steps = 22; // Balanced for 1x1 fidelity
    float max_dist = 14.0f; // Increased fog to hide lower step count
    
    // Precalculate camera roll and pitch
    float cam_s = sinf(t * 0.4f);
    float cam_c = cosf(t * 0.4f);
    float pitch_s = sinf(t * 0.2f);
    float pitch_c = cosf(t * 0.2f);
    
    // Pure 1x1 Block Rendering (No temporal blurring)
    for (int py = 0; py < 64; py++) {
        for (int px = 0; px < 64; px++) {
            // Ray direction (FOV setup)
            float rd_x = (px - 31.5f) / 32.0f;
            float rd_y = (py - 31.5f) / 32.0f;
            float rd_z = 1.0f; 
            
            // Normalize
            float len = sqrtf(rd_x*rd_x + rd_y*rd_y + rd_z*rd_z);
            rd_x /= len; rd_y /= len; rd_z /= len;
            
            // Apply Camera Roll
            float trx = rd_x * cam_c - rd_y * cam_s;
            float try_ = rd_x * cam_s + rd_y * cam_c;
            rd_x = trx; rd_y = try_;
            
            // Apply Camera Pitch/Yaw to organically look around while flying
            float trz = rd_z * pitch_c - rd_y * pitch_s;
            try_ = rd_z * pitch_s + rd_y * pitch_c;
            rd_z = trz; rd_y = try_;
            
            float dist = 0.0f;
            int steps = 0;
            
            // 3D Raymarching Engine
            for (steps = 0; steps < max_steps; steps++) {
                float p_x = ro_x + rd_x * dist;
                float p_y = ro_y + rd_y * dist;
                float p_z = ro_z + rd_z * dist;
                
                // Add minor domain warping to make the cubes melt slightly
                p_x += 0.1f * sinf(p_z * 1.5f + t);
                p_y += 0.1f * cosf(p_z * 1.5f - t);
                
                // Scale space slightly to fit more cubes in view
                float scale = 1.3f;
                float d = mapMenger(p_x * scale, p_y * scale, p_z * scale) / scale;
                
                if (d < 0.01f) break; // We hit the surface of a cube!
                dist += d;
                if (dist > max_dist) break; // Lost in the fog
            }
            
            if (dist < max_dist) {
                // Color mapping
                float hue = fmodf(dist * 12.0f + t * 60.0f, 360.0f);
                if (hue < 0) hue += 360.0f;
                
                // Ambient Occlusion: crevices (high steps) glow darker, edges are bright
                float val = 1.0f - ((float)steps / max_steps);
                
                // Fog: fade smoothly to black in the distance to hide pop-in
                float fog = 1.0f - (dist / max_dist);
                val *= fog;
                
                // Neon glow pop
                val = sqrtf(val);
                
                uint8_t r_col, g_col, b_col;
                HSVtoRGB(hue, 1.0f, val, &r_col, &g_col, &b_col);
                drawPixelCallback(px, py, r_col, g_col, b_col);
            } else {
                drawPixelCallback(px, py, 0, 0, 0); // Background void
            }
        }
    }
}

// -----------------------------------------------------------
// VISUALIZATION 9: DVD SCREENSAVER (Corner Hit!)
// -----------------------------------------------------------

// Authentic 31x14 DVD Logo Bitmap (Zero Whitespace, GCD=1 Stretched)
static const uint32_t dvd_logo[14] = {
    0b0011111111111100000111111111110,
    0b0000001111111100000111000011111,
    0b0011100011111110001111110000111,
    0b0111000011101110011101110000111,
    0b0111000011101111111001110000111,
    0b0111001111000111110001110011110,
    0b0111111110000111100011111111000,
    0b0111111110000111100011111111000,
    0b0000000000000110000000000000000,
    0b0000000000000010000000000000000,
    0b0000001111111111111111110000000,
    0b1111111111111111111111111111100,
    0b1111111111100000011111111111100,
    0b0001111111111111111111111100000
};

static void drawDVD(unsigned long now) {
    static unsigned long last_time = 0;
    static float x = 0.0f;
    static float y = 10.0f;
    // Both velocities MUST be perfectly identical to preserve the smooth 
    // 45-degree diagonal pixel-stepping illusion without jitter.
    // Increased by 20% from 10.5 to 12.6
    static float vx = 12.6f; 
    static float vy = 12.6f;
    static float corner_flash = 0.0f;
    
    if (last_time == 0) {
        last_time = now;
        return;
    }
    
    float dt = (now - last_time) / 1000.0f;
    last_time = now;
    
    if (dt > 0.1f) dt = 0.016f; // Pause safety
    
    x += vx * dt;
    y += vy * dt;
    
    // Box dimensions chosen as 31 and 14. 
    // Grid movement bounds are 33 and 50 (GCD is 1!)
    // This mathematically guarantees a perfect corner hit!
    int box_w = 31;
    int box_h = 14;
    float max_x = 64.0f - box_w;
    float max_y = 64.0f - box_h;
    
    bool bounce_x = false;
    bool bounce_y = false;
    
    // Perfect mathematical reflection
    if (x <= 0.0f) { 
        x = -x; vx = fabsf(vx); bounce_x = true; 
    } else if (x >= max_x) { 
        x = max_x - (x - max_x); vx = -fabsf(vx); bounce_x = true; 
    }
    
    if (y <= 0.0f) { 
        y = -y; vy = fabsf(vy); bounce_y = true; 
    } else if (y >= max_y) { 
        y = max_y - (y - max_y); vy = -fabsf(vy); bounce_y = true; 
    }
    
    if (bounce_x && bounce_y) {
        static int corner_attempts = 0;
        corner_attempts++;
        
        if (corner_attempts < 3) {
            // THE ULTIMATE TROLL:
            // It perfectly hit the corner. But we refuse to flash the screen.
            // Instead, we bump the X coordinate 1 pixel AHEAD of its bounce.
            // Visually, it looks like it awkwardly skipped/missed the exact pixel.
            // Mathematically, this specific 1-pixel X offset forces the logo to 
            // travel another 1550 units (2 minutes 27 seconds) before it hits again!
            if (x < max_x / 2.0f) {
                x += 1.0f; // Bounced at left, push it ahead to 1
            } else {
                x -= 1.0f; // Bounced at right, push it ahead to max_x - 1
            }
        } else {
            // 3rd actual mathematical hit! We finally reward them.
            corner_flash = 1.0f;
            corner_attempts = 0;
        }
    }
    
    if (corner_flash > 0.0f) {
        corner_flash -= dt * 1.5f; 
        if (corner_flash < 0.0f) corner_flash = 0.0f;
    }
    
    int ix = (int)roundf(x);
    int iy = (int)roundf(y);
    
    // Very slow transition through the rainbow
    float solid_hue = fmodf(now / 100.0f, 360.0f);
    
    for (int py = 0; py < 64; py++) {
        for (int px = 0; px < 64; px++) {
            float r_f = 0.0f, g_f = 0.0f, b_f = 0.0f;
            
            if (px >= ix && px < ix + box_w && py >= iy && py < iy + box_h) {
                int lx = px - ix;
                int ly = py - iy;
                
                // Read directly from the highly accurate 31x14 bitmap
                bool is_pixel = (dvd_logo[ly] & ((uint32_t)1 << (30 - lx))) != 0;
                
                if (is_pixel) {
                    uint8_t br, bg, bb;
                    HSVtoRGB(solid_hue, 1.0f, 1.0f, &br, &bg, &bb);
                    r_f = br / 255.0f; 
                    g_f = bg / 255.0f; 
                    b_f = bb / 255.0f;
                }
            }
            
            // Apply mega flash over the whole screen
            if (corner_flash > 0.0f) {
                r_f += corner_flash;
                g_f += corner_flash;
                b_f += corner_flash;
            }
            
            if (r_f > 1.0f) r_f = 1.0f;
            if (g_f > 1.0f) g_f = 1.0f;
            if (b_f > 1.0f) b_f = 1.0f;
            
            drawPixelCallback(px, py, (uint8_t)(r_f * 255), (uint8_t)(g_f * 255), (uint8_t)(b_f * 255));
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
    } else if (current_vis == 5) {
        drawCubicMatrix(now);
    } else if (current_vis == 6) {
        drawDVD(now);
    }
    
    updateScreenCallback();
}

void visDrawBackground(int bg_idx, unsigned long now) {
    if (bg_idx == 3) {
        drawPlasma(now);
    } else if (bg_idx == 4) {
        drawCubicMatrix(now); // Starfield doesn't exist, use Cubic Matrix
    }
}

void visDrawAnimation(int anim_idx, unsigned long now) {
    if (anim_idx < 0 || anim_idx >= NUM_VIS) return;
    
    switch (anim_idx) {
        case 0: drawPlasma(now); break;
        case 1: drawConcentric(now); break;
        case 2: drawJulia(now); break;
        case 3: drawGameOfLife(now); break;
        case 4: drawMandelbrot(now); break;
        case 5: drawCubicMatrix(now); break;
        case 6: drawDVD(now); break;
        default: break;
    }
}
