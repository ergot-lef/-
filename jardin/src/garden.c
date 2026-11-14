#include "garden.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const char *const garden_input_names[NI] = {
    "nourriture ←", "nourriture ↑", "nourriture →",
    "voisin ←", "voisin ↑", "voisin →",
    "énergie", "hasard", "constante"
};
const char *const garden_output_names[NO] = { "tourner", "avancer", "attaquer" };

/* ------------------------------------------------------------------ */

static uint64_t sm(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static float fr(Garden *g) { return (float)((sm(&g->rng) >> 40) * (1.0 / 16777216.0)); }
static float gs(Garden *g)
{
    float u = fr(g) + 1e-7f, v = fr(g);
    return sqrtf(-2.0f * logf(u)) * cosf(6.2831853f * v);
}

static inline float wrapx(float x) { return x < 0 ? x + WORLD_W : x >= WORLD_W ? x - WORLD_W : x; }
static inline float wrapy(float y) { return y < 0 ? y + WORLD_H : y >= WORLD_H ? y - WORLD_H : y; }
static inline int cx(float x) { int i = (int)(x / CELL); return i < 0 ? 0 : i >= FW ? FW - 1 : i; }
static inline int cy(float y) { int i = (int)(y / CELL); return i < 0 ? 0 : i >= FH ? FH - 1 : i; }

/* ------------------------------------------------------------------ */

float garden_season(const Garden *g)
{
    float s = 0.5f + 0.5f * sinf(6.2831853f * (float)(g->tick % SEASON_TICKS) / SEASON_TICKS);
    return 0.12f + 0.88f * s;
}

const char *garden_season_name(const Garden *g)
{
    int q = (int)((g->tick % SEASON_TICKS) * 4 / SEASON_TICKS);
    static const char *n[4] = { "printemps", "été", "automne", "hiver" };
    return n[q];
}

static void random_brain(Garden *g, Brain *b)
{
    memset(b, 0, sizeof *b);
    b->nh = 2 + (int)(fr(g) * 3);
    for (int j = 0; j < NH_MAX; j++) {
        for (int i = 0; i < NI; i++) b->win[j][i] = gs(g);
        for (int k = 0; k < NH_MAX; k++) b->wh[j][k] = gs(g) * 0.3f;
        b->bh[j] = gs(g) * 0.5f;
    }
    for (int o = 0; o < NO; o++) {
        for (int j = 0; j < NH_MAX; j++) b->wout[o][j] = gs(g);
        b->bo[o] = gs(g) * 0.5f;
    }
}

static Creature *spawn(Garden *g)
{
    if (g->count >= MAXC) return NULL;
    for (int i = 0; i < MAXC; i++)
        if (!g->c[i].alive) {
            Creature *c = &g->c[i];
            memset(c, 0, sizeof *c);
            c->alive = 1;
            c->id = ++g->next_id;
            g->count++;
            return c;
        }
    return NULL;
}

static void spawn_random(Garden *g, int n)
{
    for (int k = 0; k < n; k++) {
        Creature *c = spawn(g);
        if (!c) return;
        c->x = fr(g) * WORLD_W; c->y = fr(g) * WORLD_H; c->a = fr(g) * 6.2831853f;
        c->energy = 60;
        c->r = (uint8_t)(80 + fr(g) * 175); c->g = (uint8_t)(80 + fr(g) * 175); c->b = (uint8_t)(80 + fr(g) * 175);
        random_brain(g, &c->brain);
    }
}

void garden_new(Garden *g, uint64_t seed)
{
    memset(g, 0, sizeof *g);
    g->rng = seed ? seed : 1;
    /* sol : quelques grandes ondulations donnent des terres riches et des déserts */
    float kx[4], ky[4], ph[4];
    for (int k = 0; k < 4; k++) { kx[k] = (fr(g) * 2 - 1) * 0.25f; ky[k] = (fr(g) * 2 - 1) * 0.25f; ph[k] = fr(g) * 6.28f; }
    for (int y = 0; y < FH; y++)
        for (int x = 0; x < FW; x++) {
            float v = 0;
            for (int k = 0; k < 4; k++) v += cosf(kx[k] * x + ky[k] * y + ph[k]);
            v = (v + 4) / 8;            /* 0..1 */
            v = v * v * 1.6f;
            g->fert[y][x] = v > 1 ? 1 : v < 0.03f ? 0.03f : v;
            g->food[y][x] = g->fert[y][x] * 0.6f;
        }
    spawn_random(g, 150);
}

static void mutate(Garden *g, Brain *b)
{
    for (int j = 0; j < NH_MAX; j++) {
        for (int i = 0; i < NI; i++) if (fr(g) < 0.08f) b->win[j][i] += gs(g) * 0.3f;
        for (int k = 0; k < NH_MAX; k++) if (fr(g) < 0.04f) b->wh[j][k] += gs(g) * 0.2f;
        if (fr(g) < 0.08f) b->bh[j] += gs(g) * 0.2f;
    }
    for (int o = 0; o < NO; o++) {
        for (int j = 0; j < NH_MAX; j++) if (fr(g) < 0.08f) b->wout[o][j] += gs(g) * 0.3f;
        if (fr(g) < 0.08f) b->bo[o] += gs(g) * 0.2f;
    }
    /* parfois un neurone de plus, ou de moins */
    float r = fr(g);
    if (r < 0.04f && b->nh < NH_MAX) {
        int j = b->nh++;
        for (int i = 0; i < NI; i++) b->win[j][i] = gs(g) * 0.5f;
        for (int k = 0; k < NH_MAX; k++) { b->wh[j][k] = 0; b->wh[k][j] = 0; }
        b->bh[j] = 0;
        for (int o = 0; o < NO; o++) b->wout[o][j] = gs(g) * 0.1f;
    } else if (r < 0.08f && b->nh > 1) {
        b->nh--;
    }
}

/* ------------------------------------------------------------------ */
/* Voisinage : grille de cases pour trouver vite les créatures proches */
/* ------------------------------------------------------------------ */

static int head[FH][FW], nxt[MAXC];

static void build_grid(Garden *g)
{
    for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) head[y][x] = -1;
    for (int i = 0; i < MAXC; i++) {
        if (!g->c[i].alive) continue;
        int x = cx(g->c[i].x), y = cy(g->c[i].y);
        nxt[i] = head[y][x];
        head[y][x] = i;
    }
}

/* créatures (autres que self) à moins de r pixels du point (px,py) ; renvoie la plus proche */
static int near(Garden *g, float px, float py, float r, int self, int *count)
{
    int best = -1, n = 0;
    float bd = r * r;
    int x0 = cx(px), y0 = cy(py);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int xx = (x0 + dx + FW) % FW, yy = (y0 + dy + FH) % FH;
            for (int i = head[yy][xx]; i >= 0; i = nxt[i]) {
                if (i == self) continue;
                float ddx = g->c[i].x - px, ddy = g->c[i].y - py;
                if (ddx > WORLD_W / 2) ddx -= WORLD_W; else if (ddx < -WORLD_W / 2) ddx += WORLD_W;
                if (ddy > WORLD_H / 2) ddy -= WORLD_H; else if (ddy < -WORLD_H / 2) ddy += WORLD_H;
                float d = ddx * ddx + ddy * ddy;
                if (d < r * r) { n++; if (d < bd) { bd = d; best = i; } }
            }
        }
    if (count) *count = n;
    return best;
}

static void die(Garden *g, Creature *c)
{
    float *f = &g->food[cy(c->y)][cx(c->x)];
    *f += 0.6f;
    if (*f > 1.5f) *f = 1.5f;
    c->alive = 0;
    g->count--;
    g->deaths++;
}

/* ------------------------------------------------------------------ */

static void think(Garden *g, Creature *c)
{
    static const float off[3] = { -0.6f, 0.0f, 0.6f };
    for (int k = 0; k < 3; k++) {
        float px = wrapx(c->x + cosf(c->a + off[k]) * 26), py = wrapy(c->y + sinf(c->a + off[k]) * 26);
        float f = g->food[cy(py)][cx(px)];
        c->in[k] = f > 1 ? 1 : f;
        int n = 0;
        near(g, px, py, 12, (int)(c - g->c), &n);
        c->in[3 + k] = n > 3 ? 1.0f : n / 3.0f;
    }
    c->in[6] = c->energy / 150.0f;
    c->in[7] = fr(g) * 2 - 1;
    c->in[8] = 1;

    Brain *b = &c->brain;
    float h[NH_MAX];
    for (int j = 0; j < b->nh; j++) {
        float a = b->bh[j];
        for (int i = 0; i < NI; i++) a += b->win[j][i] * c->in[i];
        for (int k = 0; k < b->nh; k++) a += b->wh[j][k] * c->h[k];
        h[j] = tanhf(a);
    }
    for (int j = 0; j < b->nh; j++) c->h[j] = h[j];
    for (int j = b->nh; j < NH_MAX; j++) c->h[j] = 0;
    for (int o = 0; o < NO; o++) {
        float a = b->bo[o];
        for (int j = 0; j < b->nh; j++) a += b->wout[o][j] * c->h[j];
        c->out[o] = a;
    }
}

void garden_step(Garden *g)
{
    /* le sol repousse, selon sa fertilité et la saison */
    float abund = garden_season(g);
    for (int y = 0; y < FH; y++)
        for (int x = 0; x < FW; x++) {
            float *f = &g->food[y][x];
            if (*f < 1) *f += 0.0016f * g->fert[y][x] * abund * (1.0f - *f) + 0.00002f;
        }

    build_grid(g);
    for (int i = 0; i < MAXC; i++) {
        Creature *c = &g->c[i];
        if (!c->alive) continue;
        think(g, c);

        float turn = tanhf(c->out[0]) * 0.25f;
        float speed = 1.6f / (1.0f + expf(-c->out[1]));
        c->attacking = 1.0f / (1.0f + expf(-c->out[2])) > 0.6f;
        c->speed = speed;
        c->a += turn;
        c->x = wrapx(c->x + cosf(c->a) * speed);
        c->y = wrapy(c->y + sinf(c->a) * speed);
        c->age++;

        /* manger ce qui pousse sous ses pieds */
        float *f = &g->food[cy(c->y)][cx(c->x)];
        float eat = *f < 0.08f ? *f : 0.08f;
        *f -= eat;
        c->energy += eat * 15;
        if (c->energy > 200) c->energy = 200;

        /* vivre coûte : bouger, penser, attaquer */
        c->energy -= 0.02f + 0.03f * speed + 0.0015f * c->brain.nh + (c->attacking ? 0.05f : 0);

        /* attaquer la créature juste devant */
        if (c->attacking) {
            float px = wrapx(c->x + cosf(c->a) * 6), py = wrapy(c->y + sinf(c->a) * 6);
            int v = near(g, px, py, 7, i, NULL);
            if (v >= 0 && g->c[v].alive) {
                g->c[v].energy -= 5;
                c->energy += 2.5f;
                if (g->c[v].energy <= 0) { die(g, &g->c[v]); g->kills++; g->kill_rate += 1; }
            }
        }

        /* avoir un petit */
        if (c->energy > 140 && c->age > 100) {
            Creature *k = spawn(g);
            if (k) {
                Creature parent = *c;   /* spawn peut avoir réutilisé la mémoire : on copie */
                k->x = wrapx(parent.x + gs(g) * 6); k->y = wrapy(parent.y + gs(g) * 6);
                k->a = fr(g) * 6.2831853f;
                k->energy = 60;
                k->gen = parent.gen + 1;
                k->parent = parent.id;
                k->brain = parent.brain;
                mutate(g, &k->brain);
                int dr = (int)(gs(g) * 8), dg = (int)(gs(g) * 8), db = (int)(gs(g) * 8);
                k->r = (uint8_t)(parent.r + dr < 60 ? 60 : parent.r + dr > 255 ? 255 : parent.r + dr);
                k->g = (uint8_t)(parent.g + dg < 60 ? 60 : parent.g + dg > 255 ? 255 : parent.g + dg);
                k->b = (uint8_t)(parent.b + db < 60 ? 60 : parent.b + db > 255 ? 255 : parent.b + db);
                c->energy -= 70;
                c->kids++;
                g->births++;
            }
        }

        if (c->energy <= 0 || c->age > 6000 + (c->id % 2000)) die(g, c);
    }
    g->kill_rate *= 0.999f;

    /* si tout le monde est mort, de nouveaux venus arrivent au hasard */
    if (g->count < 4) { spawn_random(g, 40); g->extinctions++; }
    g->tick++;
}

void garden_stats(const Garden *g, GardenStats *s)
{
    double nh = 0, gen = 0, food = 0;
    int n = 0;
    for (int i = 0; i < MAXC; i++)
        if (g->c[i].alive) { nh += g->c[i].brain.nh; gen += g->c[i].gen; n++; }
    for (int y = 0; y < FH; y++) for (int x = 0; x < FW; x++) food += g->food[y][x];
    s->pop = (float)n;
    s->brain = n ? (float)(nh / n) : 0;
    s->gen = n ? (float)(gen / n) : 0;
    s->kills = g->kill_rate;
    s->food = (float)(food / (FW * FH));
}

static const char MAGIC[4] = { 'J', 'A', 'R', '1' };

int garden_save(const Garden *g, const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(MAGIC, 4, 1, f) == 1 && fwrite(g, sizeof *g, 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    remove(path);
    return rename(tmp, path);
}

int garden_load(Garden *g, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char m[4];
    int ok = fread(m, 4, 1, f) == 1 && !memcmp(m, MAGIC, 4) && fread(g, sizeof *g, 1, f) == 1;
    fclose(f);
    return ok ? 0 : -1;
}
