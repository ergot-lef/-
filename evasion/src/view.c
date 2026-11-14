/* Dessin d'Évasion (partagé entre la Vita et l'aperçu PC). */
#include "view.h"
#include "../../common/gfx.h"

#include <math.h>
#include <string.h>

static const uint32_t SKY = RGB(18, 22, 32), WALL = RGB(70, 66, 74), SOIL = RGB(60, 50, 40),
                      GRASS = RGB(90, 120, 70), TXT = RGB(225, 228, 235), DIM = RGB(140, 146, 160),
                      GOLD = RGB(255, 210, 90);

static int sx(const ViewState *v, float x) { return (int)(x - v->cam_x); }
static int sy(float y) { return (int)(y - (FLOOR_Y - 400)); }   /* sol à y = 400 à l'écran */

static void draw_world(const Sim *s, const ViewState *v)
{
    gfx_clear(SKY);
    /* sol */
    for (int x = 0; x < SCR_W; x++) {
        float wx = x + v->cam_x;
        float g = ground_at(s, wx);
        if (g > 9000) continue;      /* trou */
        int y = sy(g);
        gfx_rect(x, y, 1, 3, GRASS);
        gfx_rect(x, y + 3, 1, 512 - y - 3, SOIL);
    }
    /* repères tous les 100 px = 1 m */
    for (int m = (int)(v->cam_x / 100); m <= (int)((v->cam_x + SCR_W) / 100); m++) {
        int x = sx(v, m * 100.0f);
        if (m > 2 && ground_at(s, m * 100.0f) < 9000) gfx_textf(x - 8, sy(FLOOR_Y) + 10, RGB(120, 110, 90), "%d m", m - 2);
    }
    /* la cellule */
    gfx_rect(sx(v, CELL_X0 - 20), sy(CELL_TOP - 20), (int)(CELL_X1 - CELL_X0 + 54), 20, WALL);
    gfx_rect(sx(v, CELL_X0 - 20), sy(CELL_TOP - 20), 22, (int)(FLOOR_Y - CELL_TOP + 20), WALL);
    gfx_rect(sx(v, CELL_X1), sy(CELL_TOP), 14, (int)(DOOR_TOP - CELL_TOP), WALL);
    for (int k = 0; k < 6; k++)    /* barreaux de la fenêtre */
        gfx_rect(sx(v, 60 + k * 22), sy(CELL_TOP + 20), 3, 40, RGB(110, 106, 116));
    gfx_text(sx(v, 90), sy(CELL_TOP + 70), RGB(110, 106, 116), "CELLULE");
}

static void draw_body(const ViewState *v, const Body *b, uint32_t c, int thick)
{
    static const int A[] = { P_NECK, P_HIP, P_KNEE_L, P_HIP, P_KNEE_R, P_HEEL_L, P_HEEL_R };
    static const int B[] = { P_HIP, P_KNEE_L, P_FOOT_L, P_KNEE_R, P_FOOT_R, P_TOE_L, P_TOE_R };
    for (int k = 0; k < 7; k++)
        for (int t = 0; t < thick; t++)
            gfx_line(sx(v, b->x[A[k]]) + t, sy(b->y[A[k]]), sx(v, b->x[B[k]]) + t, sy(b->y[B[k]]), c);
    gfx_disc(sx(v, b->x[P_HEAD]), sy(b->y[P_HEAD]), thick > 1 ? 7 : 6, c);
}

static uint32_t act(float a)
{
    a = a > 1 ? 1 : a < -1 ? -1 : a;
    return a >= 0 ? RGB(40, 40 + 200 * a, 60) : RGB(40 - 200 * a, 40, 60);
}

static void draw_brain(const Sim *s, int i)
{
    const Body *b = &s->bodies[i];
    const int X = 572, Y = 8, W = 380, H = 330;
    gfx_dim(X, Y, W, H, 2);
    gfx_frame(X, Y, W, H, GOLD);
    gfx_text(X + 8, Y + 4, GOLD, "Le cerveau du meneur");
    int ix = X + 172, hx = X + 246, ox = X + 290;
    for (int j = 0; j < NB_HID; j++)
        for (int o = 0; o < NB_OUT; o++) {
            float w = s->brains[i].w2[o][j];
            uint32_t c = w >= 0 ? RGB(60, 120, 70) : RGB(130, 60, 60);
            if (fabsf(w) > 0.6f) gfx_line(hx, Y + 40 + j * 22, ox, Y + 70 + o * 60, c);
        }
    for (int k = 0; k < NB_IN; k++) {
        gfx_text(X + 8, Y + 22 + k * 20, DIM, sim_input_names[k]);
        gfx_disc(ix, Y + 30 + k * 20, 5, act(b->in[k]));
    }
    for (int j = 0; j < NB_HID; j++) gfx_disc(hx, Y + 40 + j * 22, 6, act(b->hid[j]));
    for (int o = 0; o < NB_OUT; o++) {
        gfx_disc(ox, Y + 70 + o * 60, 7, act(b->out[o]));
        gfx_text(ox + 12, Y + 62 + o * 60, DIM, sim_output_names[o]);
    }
}

void view_draw(const Sim *s, const ViewState *v)
{
    draw_world(s, v);
    int lead = sim_leader(s);
    for (int i = 0; i < POP; i++) {
        const Body *b = &s->bodies[i];
        if (!b->alive) continue;
        if (i == lead) continue;
        draw_body(v, b, i < 5 ? RGB(150, 170, 220) : RGB(95, 100, 120), 1);
    }
    if (lead >= 0) draw_body(v, &s->bodies[lead], GOLD, 2);
    if (v->show_brain && lead >= 0) draw_brain(s, lead);

    /* barre d'état */
    gfx_rect(0, 512, SCR_W, 32, RGB(14, 16, 22));
    float lead_m = lead >= 0 ? (s->bodies[lead].x[P_HIP] - 200) / 100 : 0;
    gfx_textf(8, 520, TXT, "Génération %u   en vie %2d/%d   meneur %.1f m   record %.1f m   obstacles franchis (record) %d   ×%d",
              s->generation, sim_alive(s), POP, lead_m, (s->record - 200) / 100, s->obstacles_record, v->speed);

    /* moments marquants */
    int y = 8;
    gfx_textf(10, y, s->strict ? RGB(240, 120, 100) : RGB(120, 200, 140), "Règle : %s", s->strict ? "stricte (une chute élimine)" : "libre (il peut tomber et ramper)");
    y += 18;
    if (s->first_exit_gen) { gfx_textf(10, y, GOLD, "Première évasion : génération %u", s->first_exit_gen); y += 18; }
    if (s->first_obstacle_gen) { gfx_textf(10, y, GOLD, "Premier obstacle franchi : génération %u", s->first_obstacle_gen); y += 18; }

    if (v->paused) {
        gfx_dim(0, 0, SCR_W, 512, 2);
        gfx_text(20, 20, GOLD, "EN PAUSE — meilleure distance de chaque génération");
        static float m[2000];
        for (int i = 0; i < s->ngen_best; i++) m[i] = (s->gen_best[i] - 200) / 100;
        gfx_plot(20, 44, 920, 380, m, s->ngen_best, GOLD);
        gfx_text(20, 440, DIM, "Chaque point : le bonhomme qui est allé le plus loin dans sa génération (en mètres).");
        gfx_text(20, 470, DIM, "START : reprendre   L/R : vitesse   □ : cerveau   SELECT : règle   △ maintenu : tout recommencer");
    }
    if (v->hold > 0) gfx_rect(0, 541, SCR_W * v->hold / 90, 3, RGB(220, 70, 70));
}

void view_follow(const Sim *s, ViewState *v)
{
    int lead = sim_leader(s);
    float target = lead >= 0 ? s->bodies[lead].x[P_HIP] - 300 : -40;
    if (target < -40) target = -40;
    v->cam_x += (target - v->cam_x) * 0.08f;
    if (s->step == 0) v->cam_x = -40;
}
