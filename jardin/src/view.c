/* Dessin du Jardin (partagé entre la Vita et l'aperçu PC). */
#include "view.h"
#include "../../common/gfx.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t mix(uint32_t a, uint32_t b, float t)
{
    int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255;
    int br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255;
    return RGB(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t);
}

static void draw_ground(const Garden *g)
{
    float s = garden_season(g);
    uint32_t soil = mix(RGB(34, 30, 38), RGB(30, 26, 20), s);       /* hiver bleuté, été brun */
    uint32_t leaf = mix(RGB(70, 110, 90), RGB(70, 170, 60), s);
    for (int y = 0; y < FH; y++)
        for (int x = 0; x < FW; x++) {
            float f = g->food[y][x];
            if (f > 1) f = 1;
            gfx_rect(x * CELL, y * CELL, CELL, CELL, mix(soil, leaf, f * 0.85f));
        }
}

static void draw_creatures(const Garden *g, int selected)
{
    for (int i = 0; i < MAXC; i++) {
        const Creature *c = &g->c[i];
        if (!c->alive) continue;
        int x = (int)c->x, y = (int)c->y;
        uint32_t col = RGB(c->r, c->g, c->b);
        gfx_disc(x, y, 3, col);
        gfx_line(x, y, x + (int)(cosf(c->a) * 7), y + (int)(sinf(c->a) * 7), c->attacking ? RGB(255, 60, 50) : col);
        if (i == selected) {
            for (int k = 0; k < 24; k++) {
                float a = k * 0.2618f;
                int px = x + (int)(cosf(a) * 10), py = y + (int)(sinf(a) * 10);
                if ((unsigned)px < SCR_W && (unsigned)py < 512) gfx_px[py * SCR_W + px] = RGB(255, 230, 120);
            }
        }
    }
}

static uint32_t act_color(float a)
{
    if (a > 1) a = 1;
    if (a < -1) a = -1;
    return a >= 0 ? RGB(40, 40 + 200 * a, 60) : RGB(40 - 200 * a, 40, 60);
}

static uint32_t weight_color(float w)
{
    float t = fabsf(w) / 2.0f;
    if (t > 1) t = 1;
    return w >= 0 ? mix(RGB(26, 30, 40), RGB(110, 210, 120), t) : mix(RGB(26, 30, 40), RGB(230, 90, 90), t);
}

/* Le cerveau d'une créature : entrées -> neurones -> sorties, en direct. */
static void draw_brain(const Garden *g, int sel)
{
    const Creature *c = &g->c[sel];
    const int X = 560, Y = 8, W = 392, H = 496;
    gfx_dim(X, Y, W, H, 2);
    gfx_frame(X, Y, W, H, RGB(255, 230, 120));
    gfx_textf(X + 10, Y + 6, RGB(255, 230, 120), "Créature n°%u", c->id);
    gfx_textf(X + 10, Y + 24, RGB(200, 205, 215), "génération %u   âge %u   petits %u", c->gen, c->age, c->kids);
    gfx_textf(X + 10, Y + 40, RGB(200, 205, 215), "énergie %.0f   neurones %d", c->energy, c->brain.nh);

    int nh = c->brain.nh;
    int ix = X + 120, hx = X + 230, ox = X + 300;
    int iy0 = Y + 80, ystep = 44;
    int hy0 = Y + 80, hstep = nh > 1 ? (8 * ystep) / (nh - 1) : 0;
    if (nh == 1) hy0 = Y + 80 + 4 * ystep;
    int oy0 = Y + 80 + ystep * 2, ostep = ystep * 2;

    /* connexions */
    for (int j = 0; j < nh; j++) {
        int hy = hy0 + j * hstep;
        for (int i = 0; i < NI; i++) gfx_line(ix, iy0 + i * ystep, hx, hy, weight_color(c->brain.win[j][i]));
        for (int o = 0; o < NO; o++) gfx_line(hx, hy, ox, oy0 + o * ostep, weight_color(c->brain.wout[o][j]));
    }
    /* entrées */
    for (int i = 0; i < NI; i++) {
        int y = iy0 + i * ystep;
        gfx_text(X + 8, y - 8, RGB(190, 195, 205), garden_input_names[i]);
        gfx_disc(ix, y, 6, act_color(c->in[i]));
    }
    /* neurones cachés */
    for (int j = 0; j < nh; j++) gfx_disc(hx, hy0 + j * hstep, 7, act_color(c->h[j]));
    /* sorties */
    for (int o = 0; o < NO; o++) {
        int y = oy0 + o * ostep;
        gfx_disc(ox, y, 7, act_color(tanhf(c->out[o])));
        gfx_text(ox + 14, y - 8, RGB(190, 195, 205), garden_output_names[o]);
    }
    gfx_text(X + 10, Y + H - 40, RGB(140, 146, 160), "vert : positif   rouge : négatif");
    gfx_text(X + 10, Y + H - 22, RGB(140, 146, 160), "traits : force des connexions");
}

void view_draw(const Garden *g, const ViewState *v)
{
    draw_ground(g);
    draw_creatures(g, v->selected);
    if (v->selected >= 0 && g->c[v->selected].alive) draw_brain(g, v->selected);

    /* barre d'état */
    GardenStats s;
    garden_stats(g, &s);
    gfx_rect(0, 512, SCR_W, 32, RGB(14, 16, 22));
    gfx_textf(8, 520, RGB(225, 228, 235), "%-9s  année %llu   créatures %3.0f   neurones moy. %.1f   génération moy. %.0f   ×%d",
              garden_season_name(g), (unsigned long long)(g->tick / SEASON_TICKS) + 1, s.pop, s.brain, s.gen, v->speed);
    if (g->extinctions) gfx_textf(800, 520, RGB(230, 120, 100), "extinctions %u", g->extinctions);

    if (v->paused) {
        gfx_dim(0, 0, SCR_W, 512, 2);
        gfx_text(20, 14, RGB(255, 230, 120), "EN PAUSE — l'histoire du jardin");
        const char *names[4] = { "créatures", "neurones par cerveau (moyenne)", "chasse (attaques mortelles récentes)", "nourriture au sol" };
        uint32_t cols[4] = { RGB(120, 200, 255), RGB(255, 196, 90), RGB(240, 90, 80), RGB(110, 210, 120) };
        for (int k = 0; k < 4; k++) {
            gfx_text(20, 40 + k * 116, cols[k], names[k]);
            gfx_plot(20, 58 + k * 116, 920, 92, v->hist[k], v->hist_n, cols[k]);
        }
        gfx_text(20, 494, RGB(150, 156, 170), "START : reprendre   △ maintenu : nouveau jardin   L/R : vitesse   toucher une créature : son cerveau");
    }
    if (v->hold > 0) gfx_rect(0, 541, SCR_W * v->hold / 90, 3, RGB(220, 70, 70));
}

int view_pick(const Garden *g, int x, int y)
{
    int best = -1;
    float bd = 20 * 20;
    for (int i = 0; i < MAXC; i++) {
        if (!g->c[i].alive) continue;
        float dx = g->c[i].x - x, dy = g->c[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
