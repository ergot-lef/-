#include "sim.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const char *const sim_input_names[NB_IN] = {
    "hanche G", "genou G", "hanche D", "genou D", "penche du buste", "hauteur bassin",
    "vitesse →", "vitesse ↓", "pied G au sol", "pied D au sol", "rythme sin", "rythme cos",
    "obstacle : distance", "obstacle : hauteur", "obstacle : largeur"
};
const char *const sim_output_names[NB_OUT] = { "hanche G", "genou G", "hanche D", "genou D" };

#define GRAV     0.30f
#define ITER     6
#define PI       3.14159265f

/* os ; les pieds (cheville, talon, orteils) sont rigides et la cheville garde un angle fixe */
static const float bone_len[] = { 14, 34, 26, 26, 26, 26, 48,
                                   12.37f, 12.37f, 5.83f, 5.83f, 17, 17, 31.38f, 31.38f, 29.43f, 29.43f };
static const int   bone_a[]   = { P_HEAD, P_NECK, P_HIP, P_KNEE_L, P_HIP, P_KNEE_R, P_HEAD,
                                   P_FOOT_L, P_FOOT_R, P_FOOT_L, P_FOOT_R, P_HEEL_L, P_HEEL_R, P_KNEE_L, P_KNEE_R, P_KNEE_L, P_KNEE_R };
static const int   bone_b[]   = { P_NECK, P_HIP, P_KNEE_L, P_FOOT_L, P_KNEE_R, P_FOOT_R, P_HIP,
                                   P_TOE_L, P_TOE_R, P_HEEL_L, P_HEEL_R, P_TOE_L, P_TOE_R, P_TOE_L, P_TOE_R, P_HEEL_L, P_HEEL_R };
#define NBONES 17

/* ------------------------------------------------------------------ */

static uint64_t sm(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static float fr(Sim *s) { return (float)((sm(&s->rng) >> 40) * (1.0 / 16777216.0)); }
static float gs(Sim *s)
{
    float u = fr(s) + 1e-7f, v = fr(s);
    return sqrtf(-2.0f * logf(u)) * cosf(2 * PI * v);
}
static inline float wrap(float a)
{
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

/* ------------------------------------------------------------------ */
/* Le terrain                                                          */
/* ------------------------------------------------------------------ */

float ground_at(const Sim *s, float x)
{
    int lo = 0, hi = s->nobs - 1, k = -1;
    while (lo <= hi) {                      /* dernier obstacle qui commence avant x */
        int mid = (lo + hi) / 2;
        if (s->obs[mid].x0 <= x) { k = mid; lo = mid + 1; } else hi = mid - 1;
    }
    if (k >= 0 && x < s->obs[k].x1) {
        float h = s->obs[k].h;
        return h > 0 ? FLOOR_Y - h : 99999.0f;
    }
    return FLOOR_Y;
}

static const Obstacle *next_obstacle(const Sim *s, float x)
{
    for (int i = 0; i < s->nobs; i++)
        if (s->obs[i].x1 > x) return &s->obs[i];
    return NULL;
}

int obstacles_passed(const Sim *s, float x)
{
    int n = 0;
    for (int i = 0; i < s->nobs; i++) if (s->obs[i].x1 < x) n++;
    return n;
}

static void make_course(Sim *s)
{
    float x = CELL_X1 + 200;
    s->nobs = 0;
    while (s->nobs < MAX_OBS && x < COURSE_LEN) {
        float d = (float)s->nobs / MAX_OBS;          /* difficulté croissante */
        Obstacle *o = &s->obs[s->nobs++];
        if (fr(s) < 0.6f) {
            float w = 40 + fr(s) * 60;
            o->x0 = x; o->x1 = x + w;
            o->h = 6 + d * 34 + fr(s) * 6;
        } else {
            float w = 14 + d * 46 + fr(s) * 8;
            o->x0 = x; o->x1 = x + w;
            o->h = -1;
        }
        x = o->x1 + 150 + fr(s) * 120;
    }
}

/* ------------------------------------------------------------------ */
/* Le corps                                                            */
/* ------------------------------------------------------------------ */

static void body_reset(Body *b)
{
    static const float ox[NPTS] = { 0, 0, 0, -1, -1, 1, 1, 11, 13, -6, -4 };
    static const float oy[NPTS] = { -103, -89, -55, -29, -3, -29, -3, 0, 0, 0, 0 };
    memset(b, 0, sizeof *b);
    for (int i = 0; i < NPTS; i++) {
        b->x[i] = b->px[i] = 70 + ox[i];
        b->y[i] = b->py[i] = FLOOR_Y + oy[i];
    }
    b->alive = 1;
    b->best_x = b->x[P_HIP];
}

/* Un muscle change la posture sans créer d'élan par magie : on tourne la position
 * actuelle ET la précédente du point. Pour avancer, il faudra pousser contre le sol. */
static void rotate(Body *b, int p, int pivot, float a)
{
    float c = cosf(a), s = sinf(a);
    float dx = b->x[p] - b->x[pivot], dy = b->y[p] - b->y[pivot];
    float vx = b->x[p] - b->px[p], vy = b->y[p] - b->py[p];
    b->x[p] = b->x[pivot] + dx * c - dy * s;
    b->y[p] = b->y[pivot] + dx * s + dy * c;
    b->px[p] = b->x[p] - vx;
    b->py[p] = b->y[p] - vy;
}

static float seg_angle(const Body *b, int from, int to)
{
    return atan2f(b->y[to] - b->y[from], b->x[to] - b->x[from]);
}

static float hip_angle(const Body *b, int knee)
{
    return wrap(seg_angle(b, P_HIP, knee) - seg_angle(b, P_NECK, P_HIP));
}

static float knee_angle(const Body *b, int knee, int foot)
{
    return wrap(seg_angle(b, knee, foot) - seg_angle(b, P_HIP, knee));
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void muscles(Body *b)
{
    const int knees[2] = { P_KNEE_L, P_KNEE_R }, feet[2] = { P_FOOT_L, P_FOOT_R };
    for (int l = 0; l < 2; l++) {
        /* hanche : la jambe tourne d'un côté, le buste de l'autre */
        float th = b->out[l * 2] * 1.3f;
        float e = clampf(wrap(th - hip_angle(b, knees[l])) * 0.35f, -0.15f, 0.15f);
        rotate(b, knees[l], P_HIP, e * 0.5f);
        rotate(b, feet[l], P_HIP, e * 0.5f);
        rotate(b, feet[l] == P_FOOT_L ? P_TOE_L : P_TOE_R, P_HIP, e * 0.5f);
        rotate(b, feet[l] == P_FOOT_L ? P_HEEL_L : P_HEEL_R, P_HIP, e * 0.5f);
        rotate(b, P_NECK, P_HIP, -e * 0.25f);
        rotate(b, P_HEAD, P_HIP, -e * 0.25f);
        /* genou : ne plie que dans un sens */
        float tk = (b->out[l * 2 + 1] * 0.5f + 0.5f) * 2.2f;
        e = clampf(wrap(tk - knee_angle(b, knees[l], feet[l])) * 0.35f, -0.15f, 0.15f);
        rotate(b, feet[l], knees[l], e * 0.5f);
        rotate(b, feet[l] == P_FOOT_L ? P_TOE_L : P_TOE_R, knees[l], e * 0.5f);
        rotate(b, feet[l] == P_FOOT_L ? P_HEEL_L : P_HEEL_R, knees[l], e * 0.5f);
        rotate(b, P_HIP, knees[l], -e * 0.25f);
    }
}

static void collide(const Sim *s, Body *b, int i)
{
    float *x = &b->x[i], *y = &b->y[i];
    /* la cellule */
    if (*x < CELL_X0 + 2) *x = CELL_X0 + 2;
    if (*x < CELL_X1 && *y < CELL_TOP) *y = CELL_TOP;
    if (*x > CELL_X1 - 2 && *x < CELL_X1 + 14 && *y < DOOR_TOP) {
        if (b->px[i] < CELL_X1) *x = CELL_X1 - 2; else *x = CELL_X1 + 14;
    }
    /* le sol, les marches, les trous */
    float g = ground_at(s, *x);
    if (*y > g) {
        float gold = ground_at(s, b->px[i]);
        if (g < gold - 4 && *y > g + 4) *x = b->px[i];      /* face d'un bloc ou bord d'un trou */
        else *y = g;
    }
}

static void think(const Sim *s, const Brain *br, Body *b)
{
    float g = ground_at(s, b->x[P_HIP]);
    if (g > 9000) g = FLOOR_Y;
    const Obstacle *o = next_obstacle(s, b->x[P_HIP]);
    float ph = b->steps * (2 * PI / 50);
    float *in = b->in;
    in[0] = hip_angle(b, P_KNEE_L) / PI;
    in[1] = knee_angle(b, P_KNEE_L, P_FOOT_L) / PI;
    in[2] = hip_angle(b, P_KNEE_R) / PI;
    in[3] = knee_angle(b, P_KNEE_R, P_FOOT_R) / PI;
    in[4] = wrap(seg_angle(b, P_HIP, P_NECK) + PI / 2) / PI;
    in[5] = (g - b->y[P_HIP]) / 100;
    in[6] = (b->x[P_HIP] - b->px[P_HIP]) / 5;
    in[7] = (b->y[P_HIP] - b->py[P_HIP]) / 5;
    in[8] = (float)b->contact[0];
    in[9] = (float)b->contact[1];
    in[10] = sinf(ph);
    in[11] = cosf(ph);
    in[12] = o ? clampf((o->x0 - b->x[P_HIP]) / 300, -1, 1) : 1;
    in[13] = o ? (o->h > 0 ? o->h / 40 : -1) : 0;
    in[14] = o ? (o->x1 - o->x0) / 100 : 0;
    for (int j = 0; j < NB_HID; j++) {
        float a = br->w1[j][NB_IN];
        for (int i = 0; i < NB_IN; i++) a += br->w1[j][i] * in[i];
        b->hid[j] = tanhf(a);
    }
    for (int k = 0; k < NB_OUT; k++) {
        float a = br->w2[k][NB_HID];
        for (int j = 0; j < NB_HID; j++) a += br->w2[k][j] * b->hid[j];
        b->out[k] = tanhf(a);
    }
}

static void body_step(Sim *s, int idx)
{
    Body *b = &s->bodies[idx];
    if (!b->alive) return;
    think(s, &s->brains[idx], b);
    muscles(b);

    for (int i = 0; i < NPTS; i++) {
        float vx = (b->x[i] - b->px[i]) * 0.995f, vy = (b->y[i] - b->py[i]) * 0.995f + GRAV;
        float v2 = vx * vx + vy * vy;
        if (v2 > 36) { float k = 6 / sqrtf(v2); vx *= k; vy *= k; }   /* vitesse max d'un point */
        b->px[i] = b->x[i]; b->py[i] = b->y[i];
        b->x[i] += vx; b->y[i] += vy;
    }
    for (int it = 0; it < ITER; it++) {
        for (int k = 0; k < NBONES; k++) {
            int a = bone_a[k], c = bone_b[k];
            float dx = b->x[c] - b->x[a], dy = b->y[c] - b->y[a];
            float d = sqrtf(dx * dx + dy * dy) + 1e-6f;
            float diff = (d - bone_len[k]) / d * 0.5f;
            b->x[a] += dx * diff; b->y[a] += dy * diff;
            b->x[c] -= dx * diff; b->y[c] -= dy * diff;
        }
        for (int i = 0; i < NPTS; i++) collide(s, b, i);
    }
    /* frottement au sol */
    b->contact[0] = b->contact[1] = 0;
    for (int i = 0; i < NPTS; i++) {
        float g = ground_at(s, b->x[i]);
        if (b->y[i] >= g - 0.5f) {
            int foot = i == P_FOOT_L || i == P_FOOT_R || i == P_TOE_L || i == P_TOE_R || i == P_HEEL_L || i == P_HEEL_R;
            float f = foot ? 0.8f : 0.5f;
            b->px[i] += (b->x[i] - b->px[i]) * f;
            if (i == P_TOE_L || i == P_HEEL_L) b->contact[0] = 1;
            if (i == P_TOE_R || i == P_HEEL_R) b->contact[1] = 1;
        }
    }

    b->steps++;
    float hx = b->x[P_HIP];
    if (hx > b->best_x + 1) { b->best_x = hx; b->stall = 0; } else b->stall++;

    /* fin de l'essai */
    int head_down = b->y[P_HEAD] >= ground_at(s, b->x[P_HEAD]) - 3 || b->y[P_NECK] >= ground_at(s, b->x[P_NECK]) - 3;
    if ((s->strict && head_down) || b->y[P_HIP] > FLOOR_Y + 60 || b->stall > 400 || b->steps > 3600) b->alive = 0;
}

/* ------------------------------------------------------------------ */
/* L'évolution                                                         */
/* ------------------------------------------------------------------ */

static void random_brain(Sim *s, Brain *b)
{
    for (int j = 0; j < NB_HID; j++) for (int i = 0; i <= NB_IN; i++) b->w1[j][i] = gs(s) * 0.6f;
    for (int k = 0; k < NB_OUT; k++) for (int j = 0; j <= NB_HID; j++) b->w2[k][j] = gs(s) * 0.6f;
}

static void mutate(Sim *s, Brain *b)
{
    float *w = &b->w1[0][0];
    int n = (int)(sizeof(Brain) / sizeof(float));
    for (int i = 0; i < n; i++) {
        float r = fr(s);
        if (r < 0.01f) w[i] = gs(s) * 0.6f;
        else if (r < 0.15f) w[i] += gs(s) * 0.25f;
    }
}

static void next_generation(Sim *s)
{
    int order[POP];
    /* aller loin compte le plus ; tenir debout longtemps rapporte un peu (c'est le premier pas) */
    for (int i = 0; i < POP; i++) { order[i] = i; s->fitness[i] = s->bodies[i].best_x + 0.05f * (s->bodies[i].steps < 600 ? s->bodies[i].steps : 600); }
    for (int i = 1; i < POP; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && s->fitness[order[j]] < s->fitness[k]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    float best = 0;
    for (int i = 0; i < POP; i++) if (s->bodies[i].best_x > best) best = s->bodies[i].best_x;
    if (best > s->record) s->record = best;
    if (s->ngen_best < 2000) s->gen_best[s->ngen_best++] = best;
    else { memmove(s->gen_best, s->gen_best + 1, sizeof(float) * 1999); s->gen_best[1999] = best; }
    if (!s->first_exit_gen && best > CELL_X1 + 20) s->first_exit_gen = s->generation;
    int passed = obstacles_passed(s, best);
    if (passed > s->obstacles_record) s->obstacles_record = passed;
    if (!s->first_obstacle_gen && passed > 0) s->first_obstacle_gen = s->generation;

    static Brain next[POP];
    const int ELITE = 5, PARENTS = 12;
    for (int i = 0; i < POP; i++) {
        if (i < ELITE) { next[i] = s->brains[order[i]]; continue; }
        const Brain *a = &s->brains[order[(int)(fr(s) * fr(s) * PARENTS)]];
        const Brain *b2 = &s->brains[order[(int)(fr(s) * fr(s) * PARENTS)]];
        float *dst = &next[i].w1[0][0];
        const float *pa = &a->w1[0][0], *pb = &b2->w1[0][0];
        int n = (int)(sizeof(Brain) / sizeof(float));
        int cut = (int)(fr(s) * n);
        for (int k = 0; k < n; k++) dst[k] = k < cut ? pa[k] : pb[k];
        mutate(s, &next[i]);
    }
    memcpy(s->brains, next, sizeof next);
    for (int i = 0; i < POP; i++) body_reset(&s->bodies[i]);
    s->generation++;
    s->step = 0;
}

/* ------------------------------------------------------------------ */

void sim_new(Sim *s, uint64_t seed)
{
    memset(s, 0, sizeof *s);
    s->rng = seed ? seed : 1;
    make_course(s);
    for (int i = 0; i < POP; i++) { random_brain(s, &s->brains[i]); body_reset(&s->bodies[i]); }
    s->generation = 1;
}

void sim_step(Sim *s)
{
    for (int i = 0; i < POP; i++) body_step(s, i);
    s->step++;
    if (!sim_alive(s)) next_generation(s);
}

int sim_alive(const Sim *s)
{
    int n = 0;
    for (int i = 0; i < POP; i++) n += s->bodies[i].alive;
    return n;
}

int sim_leader(const Sim *s)
{
    int best = -1;
    for (int i = 0; i < POP; i++)
        if (s->bodies[i].alive && (best < 0 || s->bodies[i].x[P_HIP] > s->bodies[best].x[P_HIP])) best = i;
    return best;
}

static const char MAGIC[4] = { 'E', 'V', 'A', '1' };

int sim_save(const Sim *s, const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(MAGIC, 4, 1, f) == 1 && fwrite(s, sizeof *s, 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    remove(path);
    return rename(tmp, path);
}

int sim_load(Sim *s, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char m[4];
    int ok = fread(m, 4, 1, f) == 1 && !memcmp(m, MAGIC, 4) && fread(s, sizeof *s, 1, f) == 1;
    fclose(f);
    return ok ? 0 : -1;
}
