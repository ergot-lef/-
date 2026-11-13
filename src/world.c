#include "world.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Hasard : xoshiro256**, initialise a partir de la graine            */
/* ------------------------------------------------------------------ */

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

uint64_t world_rand(World *w)
{
    uint64_t *s = w->rng;
    uint64_t r = rotl(s[1] * 5, 7) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t; s[3] = rotl(s[3], 45);
    return r;
}

static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static inline uint32_t rnd(World *w, uint32_t n) { return (uint32_t)((world_rand(w) >> 32) * n >> 32); }

/* ------------------------------------------------------------------ */
/* Naissance du monde                                                 */
/* ------------------------------------------------------------------ */

void world_new(World *w, const uint8_t seed[32])
{
    memset(w, 0, sizeof *w);
    memcpy(w->seed, seed, 32);
    for (int i = 0; i < 4; i++) {
        uint64_t x;
        memcpy(&x, seed + i * 8, 8);
        w->rng[i] = splitmix(&x);
    }
    if (!(w->rng[0] | w->rng[1] | w->rng[2] | w->rng[3])) w->rng[0] = 1;

    /* Tout le monde est du bruit. */
    for (size_t i = 0; i < WORLD_BYTES; i += 8) {
        uint64_t r = world_rand(w);
        memcpy(w->mem + i, &r, 8);
    }

    /* Physique de depart : presque tous les octets ne font rien (valeurs 10..15),
     * dix valeurs tirees au hasard deviennent les dix instructions de base.
     * Cette table vit dans le monde : elle peut etre reecrite. */
    uint8_t *table = w->mem + OFF_TABLE;
    for (int i = 0; i < 256; i++)
        table[i] = (uint8_t)((world_rand(w) & 0xF0) | (10 + rnd(w, 6)));
    for (int op = 0; op < 10; op++) {
        int v;
        do v = rnd(w, 256); while ((table[v] & 15) < 10);
        table[v] = (uint8_t)((world_rand(w) & 0xF0) | op);
    }

    memset(w->mem + OFF_INPUT, 0, TAPE);
}

void world_set_input(World *w, const uint8_t in[TAPE])
{
    memcpy(w->mem + OFF_INPUT, in, TAPE);
}

/* ------------------------------------------------------------------ */
/* Une rencontre : deux bandes collees bout a bout, puis executees     */
/* 0 < 1 > deplacent la tete A, 2 { 3 } la tete B                      */
/* 4 - 5 + modifient l'octet sous A                                     */
/* 6 . copie A -> B, 7 , copie B -> A                                   */
/* 8 [ 9 ] boucles, 10..15 : rien                                       */
/* ------------------------------------------------------------------ */

#define N2 (2 * TAPE)

static uint32_t run(uint8_t *t, const uint8_t *table)
{
    int ip = 0, a = 0, b = 0;
    uint32_t steps = 0;
    while (ip >= 0 && ip < N2 && steps < MAX_STEPS) {
        int op = table[t[ip]] & 15;
        switch (op) {
        case 0: a = (a - 1) & (N2 - 1); break;
        case 1: a = (a + 1) & (N2 - 1); break;
        case 2: b = (b - 1) & (N2 - 1); break;
        case 3: b = (b + 1) & (N2 - 1); break;
        case 4: t[a]--; break;
        case 5: t[a]++; break;
        case 6: t[b] = t[a]; break;
        case 7: t[a] = t[b]; break;
        case 8:
            if (t[a] == 0) {
                int d = 1, p = ip;
                while (d && ++p < N2) {
                    int o = table[t[p]] & 15;
                    d += (o == 8) - (o == 9);
                }
                if (d) return steps + 1;
                ip = p;
            }
            break;
        case 9:
            if (t[a] != 0) {
                int d = 1, p = ip;
                while (d && --p >= 0) {
                    int o = table[t[p]] & 15;
                    d += (o == 9) - (o == 8);
                }
                if (d) return steps + 1;
                ip = p;
            }
            break;
        default: break;
        }
        ip++;
        steps++;
    }
    return steps;
}

/* ------------------------------------------------------------------ */
/* Une epoque : chaque bande rencontre (au plus) une voisine           */
/* ------------------------------------------------------------------ */

static uint16_t order[NTAPES];
static uint8_t  used[NTAPES];

void world_epoch(World *w)
{
    for (int i = 0; i < NTAPES; i++) order[i] = (uint16_t)i;
    for (int i = NTAPES - 1; i > 0; i--) {
        int j = rnd(w, i + 1);
        uint16_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    memset(used, 0, sizeof used);

    const uint8_t *table = w->mem + OFF_TABLE;
    uint8_t buf[N2];
    uint64_t steps = 0;

    for (int i = 0; i < NTAPES; i++) {
        int a = order[i];
        if (used[a]) continue;
        int ax = a % GW, ay = a / GW, b = -1;
        for (int tries = 0; tries < 8; tries++) {
            int dx = (int)rnd(w, 2 * RADIUS + 1) - RADIUS;
            int dy = (int)rnd(w, 2 * RADIUS + 1) - RADIUS;
            if (!dx && !dy) continue;
            int c = ((ay + dy + GH) % GH) * GW + (ax + dx + GW) % GW;
            if (!used[c]) { b = c; break; }
        }
        if (b < 0) continue;
        used[a] = used[b] = 1;
        uint8_t *pa = w->mem + (size_t)a * TAPE, *pb = w->mem + (size_t)b * TAPE;
        memcpy(buf, pa, TAPE);
        memcpy(buf + TAPE, pb, TAPE);
        steps += run(buf, table);
        memcpy(pa, buf, TAPE);
        memcpy(pb, buf + TAPE, TAPE);
    }

    /* Le bruit de fond : quelques octets changent au hasard. */
    double expected = WORLD_BYTES * MUTATION_RATE;
    uint32_t n = (uint32_t)expected;
    if ((double)(world_rand(w) >> 11) / 9007199254740992.0 < expected - n) n++;
    for (uint32_t i = 0; i < n; i++)
        w->mem[rnd(w, WORLD_BYTES)] = (uint8_t)world_rand(w);

    w->steps_last = steps;
    w->steps_total += steps;
    w->epoch++;
}

/* ------------------------------------------------------------------ */
/* Mesures : des nombres, sans interpretation                          */
/* ------------------------------------------------------------------ */

void world_measure(World *w, Metrics *m)
{
    static uint32_t hist[256];
    memset(hist, 0, sizeof hist);
    for (size_t i = 0; i < WORLD_BYTES; i++) hist[w->mem[i]]++;
    double h = 0;
    for (int i = 0; i < 256; i++)
        if (hist[i]) { double p = (double)hist[i] / WORLD_BYTES; h -= p * log2(p); }
    m->entropy = h;

    const uint8_t *table = w->mem + OFF_TABLE;
    uint64_t active = 0;
    for (int i = 0; i < 256; i++)
        if ((table[i] & 15) < 10) active += hist[i];
    m->density = (double)active / WORLD_BYTES;

    /* Diversite : sur 4096 positions tirees au hasard, combien de sequences
     * de 8 octets sont differentes les unes des autres ? */
    enum { S = 4096, H = 8192 };
    static uint64_t set[H];
    static uint8_t  occ[H];
    memset(occ, 0, sizeof occ);
    int distinct = 0;
    uint64_t lr = w->epoch * 0x9E3779B97F4A7C15ull + 12345;  /* hasard local : ne touche pas a celui du monde */
    for (int k = 0; k < S; k++) {
        size_t p = (size_t)((splitmix(&lr) >> 32) * (WORLD_BYTES - 8) >> 32);
        uint64_t v;
        memcpy(&v, w->mem + p, 8);
        uint64_t x = v;
        uint32_t slot = (uint32_t)(splitmix(&x) & (H - 1));
        while (occ[slot] && set[slot] != v) slot = (slot + 1) & (H - 1);
        if (!occ[slot]) { occ[slot] = 1; set[slot] = v; distinct++; }
    }
    m->distinct = (double)distinct / S;

    m->activity = (double)w->steps_last / (NTAPES / 2);

    double e = 0;
    for (int i = 0; i < SOUND_BYTES; i++) e += fabs((double)w->mem[OFF_SOUND + i] - 128.0);
    m->sound = e / SOUND_BYTES / 128.0;
}

/* ------------------------------------------------------------------ */
/* Sauvegarde                                                          */
/* ------------------------------------------------------------------ */

static const char MAGIC[4] = { '-', 'W', '1', 0 };

int world_save(const World *w, const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(MAGIC, 4, 1, f) == 1 && fwrite(w, sizeof *w, 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    remove(path);
    return rename(tmp, path);
}

int world_load(World *w, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char m[4];
    int ok = fread(m, 4, 1, f) == 1 && !memcmp(m, MAGIC, 4) && fread(w, sizeof *w, 1, f) == 1;
    fclose(f);
    return ok ? 0 : -1;
}
