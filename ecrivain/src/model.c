#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */

static uint64_t next_u64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) { return (float)((next_u64(s) >> 40) * (1.0 / 16777216.0)); }

static float gauss(uint64_t *s)
{
    float u = frand(s) + 1e-7f, v = frand(s);
    return sqrtf(-2.0f * logf(u)) * cosf(6.2831853f * v);
}

/* ------------------------------------------------------------------ */
/* Texte -> indices                                                    */
/* ------------------------------------------------------------------ */

static uint32_t utf8_decode(const unsigned char **s)
{
    const unsigned char *p = *s;
    uint32_t c = *p++;
    if (c >= 0xF0 && (p[0] & 0xC0) == 0x80) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *s = p;
    return c;
}

int *text_encode_with(const char *utf8, int *n_out, const uint32_t *vocab, int vsize)
{
    size_t len = strlen(utf8);
    int *out = malloc(sizeof(int) * (len + 1));
    int n = 0;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) {
        uint32_t c = utf8_decode(&s);
        if (c == '\r') continue;
        for (int k = 0; k < vsize; k++)
            if (vocab[k] == c) { out[n++] = k; break; }
    }
    *n_out = n;
    return out;
}

int *text_encode(const char *utf8, int *n_out, uint32_t *vocab, int *vsize)
{
    /* compte les lettres */
    enum { MAXC = 4096 };
    static uint32_t cp[MAXC];
    static long cnt[MAXC];
    int nc = 0;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) {
        uint32_t c = utf8_decode(&s);
        if (c == '\r') continue;
        int k = 0;
        while (k < nc && cp[k] != c) k++;
        if (k == nc && nc < MAXC) { cp[nc] = c; cnt[nc] = 0; nc++; }
        if (k < MAXC) cnt[k]++;
    }
    /* garde les VMAX plus fréquentes */
    int v = 0;
    while (v < VMAX && v < nc) {
        int best = -1;
        for (int k = 0; k < nc; k++)
            if (cnt[k] > 0 && (best < 0 || cnt[k] > cnt[best])) best = k;
        if (best < 0) break;
        vocab[v++] = cp[best];
        cnt[best] = -1;
    }
    *vsize = v;
    return text_encode_with(utf8, n_out, vocab, v);
}

/* ------------------------------------------------------------------ */
/* Réseau                                                              */
/* ------------------------------------------------------------------ */

void model_init(Model *m, const int *text, int n, const uint32_t *vocab, int vsize, uint64_t seed)
{
    (void)text; (void)n;
    memset(m, 0, sizeof *m);
    m->vsize = vsize;
    memcpy(m->vocab, vocab, sizeof(uint32_t) * vsize);
    m->rng = seed ? seed : 1;
    for (int v = 0; v < VMAX; v++)
        for (int e = 0; e < EMB; e++) m->emb[v][e] = gauss(&m->rng) * 0.5f;
    float s1 = 1.0f / sqrtf((float)IN), s2 = 1.0f / sqrtf((float)HID);
    for (int h = 0; h < HID; h++)
        for (int i = 0; i < IN; i++) m->w1[h][i] = gauss(&m->rng) * s1;
    for (int v = 0; v < VMAX; v++)
        for (int h = 0; h < HID; h++) m->w2[v][h] = gauss(&m->rng) * s2;
    m->loss_avg = log2((double)vsize);
    m->acc_avg = 1.0 / vsize;
}

static void forward(const Model *m, const int *ctx, Trace *t)
{
    for (int c = 0; c < CTX; c++)
        memcpy(t->x + c * EMB, m->emb[ctx[c]], sizeof(float) * EMB);
    for (int h = 0; h < HID; h++) {
        float a = m->b1[h];
        const float *w = m->w1[h];
        for (int i = 0; i < IN; i++) a += w[i] * t->x[i];
        t->h[h] = tanhf(a);
    }
    float mx = -1e30f;
    for (int v = 0; v < m->vsize; v++) {
        float a = m->b2[v];
        const float *w = m->w2[v];
        for (int h = 0; h < HID; h++) a += w[h] * t->h[h];
        t->p[v] = a;
        if (a > mx) mx = a;
    }
    float sum = 0;
    for (int v = 0; v < m->vsize; v++) { t->p[v] = expf(t->p[v] - mx); sum += t->p[v]; }
    for (int v = 0; v < m->vsize; v++) t->p[v] /= sum;
    for (int v = m->vsize; v < VMAX; v++) t->p[v] = 0;
}

void model_predict(const Model *m, const int ctx[CTX], Trace *t) { forward(m, ctx, t); }

void model_train(Model *m, const int *text, int i, float lr)
{
    static Trace t;
    static float dh[HID], dx[IN];
    const int *ctx = text + i - CTX;
    int y = text[i];
    forward(m, ctx, &t);

    /* statistiques */
    float py = t.p[y] > 1e-12f ? t.p[y] : 1e-12f;
    int best = 0;
    for (int v = 1; v < m->vsize; v++) if (t.p[v] > t.p[best]) best = v;
    m->loss_avg = m->loss_avg * 0.9995 + 0.0005 * (-log2(py));
    m->acc_avg = m->acc_avg * 0.9995 + 0.0005 * (best == y);
    m->seen++;

    /* sortie : erreur = probabilités - bonne réponse */
    memset(dh, 0, sizeof dh);
    for (int v = 0; v < m->vsize; v++) {
        float g = t.p[v] - (v == y);
        float *w = m->w2[v];
        for (int h = 0; h < HID; h++) { dh[h] += g * w[h]; w[h] -= lr * g * t.h[h]; }
        m->b2[v] -= lr * g;
    }
    /* couche cachée */
    memset(dx, 0, sizeof dx);
    for (int h = 0; h < HID; h++) {
        float g = dh[h] * (1.0f - t.h[h] * t.h[h]);
        float *w = m->w1[h];
        for (int k = 0; k < IN; k++) { dx[k] += g * w[k]; w[k] -= lr * g * t.x[k]; }
        m->b1[h] -= lr * g;
    }
    /* plongements des lettres */
    for (int c = 0; c < CTX; c++)
        for (int e = 0; e < EMB; e++) m->emb[ctx[c]][e] -= lr * dx[c * EMB + e];
}

int model_sample(Model *m, const float *p, float temperature)
{
    float q[VMAX], sum = 0;
    for (int v = 0; v < m->vsize; v++) {
        q[v] = p[v] > 0 ? powf(p[v], 1.0f / temperature) : 0;
        sum += q[v];
    }
    float r = frand(&m->rng) * sum;
    for (int v = 0; v < m->vsize; v++) { r -= q[v]; if (r <= 0) return v; }
    return m->vsize - 1;
}

/* ------------------------------------------------------------------ */

static const char MAGIC[4] = { 'E', 'C', 'R', '1' };

int model_save(const Model *m, const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    int ok = fwrite(MAGIC, 4, 1, f) == 1 && fwrite(m, sizeof *m, 1, f) == 1;
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return -1; }
    remove(path);
    return rename(tmp, path);
}

int model_load(Model *m, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char mg[4];
    int ok = fread(mg, 4, 1, f) == 1 && !memcmp(mg, MAGIC, 4) && fread(m, sizeof *m, 1, f) == 1;
    fclose(f);
    return ok ? 0 : -1;
}
