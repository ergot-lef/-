/*
 * Écrivain — version PS Vita.
 * Un petit réseau de neurones lit un livre en boucle et apprend à écrire.
 * À l'écran : ce qu'il écrit, ses neurones qui s'allument, ses hésitations,
 * et sa courbe d'erreur qui (normalement) descend au fil des heures.
 */
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../common/gfx.h"
#include "model.h"

#define DIR        "ux0:data/ecrivain"
#define BRAIN_PATH DIR "/cerveau.bin"
#define NOTE_PATH  DIR "/carnet.txt"
#define BOOK_PATH  "app0:livre.txt"

#define HIST 2000

static Model m;
static int *text, ntext;
static volatile int paused, reset_request;
static volatile float speed;            /* lettres lues par seconde */
static float hist_loss[HIST];
static volatile int hist_n;

static void push_hist(float v)
{
    int n = hist_n;
    if (n == HIST) { memmove(hist_loss, hist_loss + 1, sizeof(float) * (HIST - 1)); n--; }
    hist_loss[n] = v;
    hist_n = n + 1;
}

static void put_utf8(FILE *f, uint32_t cp)
{
    if (cp < 0x80) fputc((int)cp, f);
    else if (cp < 0x800) { fputc(0xC0 | (cp >> 6), f); fputc(0x80 | (cp & 63), f); }
    else { fputc(0xE0 | (cp >> 12), f); fputc(0x80 | ((cp >> 6) & 63), f); fputc(0x80 | (cp & 63), f); }
}

/* Le carnet : un extrait de ce qu'il sait écrire, toutes les 10 minutes. */
static void note_sample(void)
{
    FILE *f = fopen(NOTE_PATH, "a");
    if (!f) return;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    fprintf(f, "\n=== %04d-%02d-%02d %02d:%02d — %llu lettres lues — erreur %.2f bits/lettre — réussite %.0f %%\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min,
            (unsigned long long)m.seen, m.loss_avg, m.acc_avg * 100);
    uint64_t rng = m.seen * 2654435761ull + 7;
    int ctx[CTX];
    for (int c = 0; c < CTX; c++) ctx[c] = 0;
    Trace t;
    for (int i = 0; i < 400; i++) {
        model_predict(&m, ctx, &t);
        int v = model_sample(&m, t.p, 0.8f, &rng);
        put_utf8(f, m.vocab[v]);
        memmove(ctx, ctx + 1, sizeof(int) * (CTX - 1));
        ctx[CTX - 1] = v;
    }
    fputc('\n', f);
    fclose(f);
}

static void new_brain(void)
{
    uint64_t seed = 0;
    sceKernelGetRandomNumber(&seed, sizeof seed);
    uint32_t vocab[VMAX];
    int vsize = 0;
    for (int i = 0; i < m.vsize; i++) vocab[i] = m.vocab[i];
    vsize = m.vsize;
    model_init(&m, text, ntext, vocab, vsize, seed ^ sceKernelGetProcessTimeWide());
    hist_n = 0;
    remove(NOTE_PATH);
}

/* ------------------------------------------------------------------ */
/* Apprentissage (thread à part, sur son propre cœur)                  */
/* ------------------------------------------------------------------ */

static int train_thread(SceSize a, void *b)
{
    (void)a; (void)b;
    uint64_t r = 0;
    sceKernelGetRandomNumber(&r, sizeof r);
    time_t last_hist = time(NULL), last_save = last_hist, last_note = last_hist;
    uint64_t seen0 = m.seen;
    SceUInt64 t0 = sceKernelGetProcessTimeWide();
    for (;;) {
        if (reset_request) { new_brain(); reset_request = 0; seen0 = m.seen; t0 = sceKernelGetProcessTimeWide(); }
        if (paused) { sceKernelDelayThread(50000); seen0 = m.seen; t0 = sceKernelGetProcessTimeWide(); continue; }
        for (int k = 0; k < 256; k++) {
            r = r * 6364136223846793005ull + 1442695040888963407ull;
            int i = CTX + (int)((r >> 33) % (uint64_t)(ntext - CTX));
            float lr = 0.01f / (1.0f + (float)m.seen / 8e6f);
            if (lr < 0.002f) lr = 0.002f;
            model_train(&m, text, i, lr);
        }
        SceUInt64 t1 = sceKernelGetProcessTimeWide();
        if (t1 - t0 > 2000000) {
            speed = (float)((double)(m.seen - seen0) * 1e6 / (double)(t1 - t0));
            seen0 = m.seen; t0 = t1;
        }
        time_t now = time(NULL);
        if (now - last_hist >= 30) { push_hist((float)m.loss_avg); last_hist = now; }
        if (now - last_save >= 120) { model_save(&m, BRAIN_PATH); last_save = now; }
        if (now - last_note >= 600) { note_sample(); last_note = now; }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* L'écriture à l'écran                                                */
/* ------------------------------------------------------------------ */

#define COLS  76
#define LINES 13

static uint32_t page[LINES][COLS];
static int row, col;
static int ctx[CTX];
static Trace trace;
static int last_letter = -1;
static uint64_t pen_rng;

static void page_clear(void)
{
    memset(page, 0, sizeof page);
    row = col = 0;
    for (int c = 0; c < CTX; c++) ctx[c] = 0;
    last_letter = -1;
}

static void page_put(uint32_t cp)
{
    if (cp == '\n' || col >= COLS) {
        row++; col = 0;
        if (cp == '\n') return;
    }
    if (col == 0 && cp == ' ') return;   /* pas d'espace en début de ligne */
    if (row >= LINES) {
        memmove(page[0], page[1], sizeof(page[0]) * (LINES - 1));
        memset(page[LINES - 1], 0, sizeof(page[0]));
        row = LINES - 1;
    }
    page[row][col++] = cp;
}

static void write_one_letter(float temperature)
{
    model_predict(&m, ctx, &trace);
    int v = model_sample(&m, trace.p, temperature, &pen_rng);
    page_put(m.vocab[v]);
    memmove(ctx, ctx + 1, sizeof(int) * (CTX - 1));
    ctx[CTX - 1] = v;
    last_letter = v;
}

static void cp_label(char *out, uint32_t cp)
{
    if (cp == ' ') { strcpy(out, "espace"); return; }
    if (cp == '\n') { strcpy(out, "retour"); return; }
    char *o = out;
    if (cp < 0x80) *o++ = (char)cp;
    else if (cp < 0x800) { *o++ = (char)(0xC0 | (cp >> 6)); *o++ = (char)(0x80 | (cp & 63)); }
    else { *o++ = (char)(0xE0 | (cp >> 12)); *o++ = (char)(0x80 | ((cp >> 6) & 63)); *o++ = (char)(0x80 | (cp & 63)); }
    *o = 0;
}

static void draw(float temperature, int hold)
{
    const uint32_t BG = RGB(12, 14, 20), PANEL = RGB(22, 26, 36), TXT = RGB(225, 228, 235),
                   DIM = RGB(130, 136, 150), ACC = RGB(255, 196, 90);
    gfx_clear(BG);

    /* En-tête */
    gfx_text(12, 6, ACC, "ÉCRIVAIN");
    gfx_text(100, 6, DIM, "lit « Le tour du monde en quatre-vingts jours » de Jules Verne, et apprend à écrire");

    /* Ce qu'il écrit */
    gfx_rect(8, 30, 628, 250, PANEL);
    gfx_text(16, 36, DIM, "Ce qu'il écrit :");
    char buf[8];
    for (int r = 0; r < LINES; r++)
        for (int c = 0; c < COLS; c++)
            if (page[r][c]) { cp_label(buf, page[r][c] == ' ' ? 0 : page[r][c]); if (page[r][c] != ' ') gfx_text(16 + c * 8, 58 + r * 16, TXT, buf); }
    gfx_rect(16 + col * 8, 58 + row * 16 + 13, 8, 2, ACC);   /* curseur */

    /* Son cerveau */
    gfx_rect(644, 30, 308, 250, PANEL);
    gfx_text(652, 36, DIM, "Ce qu'il regarde :");
    for (int c = 0; c < CTX; c++) {
        gfx_frame(652 + c * 36, 56, 32, 22, RGB(70, 76, 92));
        uint32_t cp = m.vocab[ctx[c]];
        if (cp != ' ') { cp_label(buf, cp); gfx_text(664 + c * 36, 59, TXT, buf); }
    }
    gfx_text(652, 86, DIM, "Ses 192 neurones :");
    for (int h = 0; h < HID; h++) {
        float a = trace.h[h];
        int k = (int)(fabsf(a) * 230);
        uint32_t c = a >= 0 ? RGB(20, 30 + k * 0.9, 20) : RGB(30 + k * 0.9, 20, 20);
        gfx_rect(652 + (h % 24) * 12, 106 + (h / 24) * 12, 11, 11, c);
    }
    /* hésitations : les 4 lettres les plus probables */
    gfx_text(652, 204, DIM, "Il hésite entre :");
    int top[4] = { -1, -1, -1, -1 };
    for (int k = 0; k < 4; k++)
        for (int v = 0; v < m.vsize; v++) {
            int used = 0;
            for (int j = 0; j < k; j++) used |= top[j] == v;
            if (!used && (top[k] < 0 || trace.p[v] > trace.p[top[k]])) top[k] = v;
        }
    for (int k = 0; k < 4; k++) {
        if (top[k] < 0) continue;
        int y = 222 + k * 14;
        char lbl[16];
        cp_label(lbl, m.vocab[top[k]]);
        gfx_text(652, y - 2, top[k] == last_letter ? ACC : TXT, lbl);
        gfx_rect(712, y + 2, (int)(trace.p[top[k]] * 190), 8, top[k] == last_letter ? ACC : RGB(90, 140, 220));
        gfx_textf(908, y - 2, DIM, "%2.0f%%", trace.p[top[k]] * 100);
    }

    /* Progrès */
    gfx_rect(8, 288, 944, 224, PANEL);
    gfx_text(16, 294, DIM, "Son erreur au fil du temps (bits par lettre : plus c'est bas, mieux il devine)");
    float tmp[HIST];
    int n = hist_n;
    memcpy(tmp, hist_loss, sizeof(float) * n);
    gfx_plot(16, 314, 620, 190, tmp, n, ACC);
    double tours = (double)m.seen / (ntext > 0 ? ntext : 1);
    gfx_textf(652, 318, TXT, "Lettres lues : %llu", (unsigned long long)m.seen);
    gfx_textf(652, 338, TXT, "Tours du livre : %.2f", tours);
    gfx_textf(652, 358, TXT, "Erreur : %.2f bits/lettre", m.loss_avg);
    gfx_textf(652, 378, TXT, "Bonne lettre devinée : %.0f %%", m.acc_avg * 100);
    gfx_textf(652, 398, TXT, "Vitesse : %.0f lettres/s", speed);
    gfx_textf(652, 418, TXT, "Audace : %.1f", temperature);
    gfx_text(652, 446, DIM, "(au hasard pur : 6,6 bits/lettre)");
    if (paused) gfx_text(652, 476, ACC, "EN PAUSE");

    /* Aide */
    gfx_text(12, 522, DIM, "X : nouvelle page   ↑↓ : audace   START : pause   △ maintenu : tout oublier");
    if (hold > 0) gfx_rect(8, 540, 944 * hold / 90, 3, RGB(220, 70, 70));
}

int main(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    sceIoMkdir(DIR, 0777);
    if (gfx_init() < 0) { sceKernelExitProcess(0); return 0; }

    /* le livre */
    FILE *f = fopen(BOOK_PATH, "rb");
    if (!f) { gfx_clear(0xFF000000u); gfx_text(20, 20, 0xFFFFFFFFu, "Livre introuvable."); gfx_flip(); sceKernelDelayThread(5000000); sceKernelExitProcess(0); return 0; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *book = malloc(sz + 1);
    sz = (long)fread(book, 1, sz, f);
    book[sz] = 0;
    fclose(f);

    uint32_t vocab[VMAX];
    int vsize;
    text = text_encode(book, &ntext, vocab, &vsize);
    free(book);

    /* le cerveau : on reprend là où il en était, sinon il naît */
    if (model_load(&m, BRAIN_PATH) < 0 || m.vsize != vsize || memcmp(m.vocab, vocab, sizeof(uint32_t) * vsize)) {
        m.vsize = vsize;
        memcpy(m.vocab, vocab, sizeof(uint32_t) * vsize);
        new_brain();
    }
    sceKernelGetRandomNumber(&pen_rng, sizeof pen_rng);
    page_clear();

    SceUID th = sceKernelCreateThread("apprentissage", train_thread, 160, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);

    float temperature = 0.8f;
    uint32_t prev = 0;
    int frame = 0, hold = 0, written = 0;
    for (;;) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof pad);
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pr = pad.buttons & ~prev;
        prev = pad.buttons;
        if (pr & SCE_CTRL_START) { paused = !paused; if (paused) model_save(&m, BRAIN_PATH); }
        if (pr & SCE_CTRL_CROSS) { page_clear(); written = 0; }
        if (pr & SCE_CTRL_UP && temperature < 1.5f) temperature += 0.1f;
        if (pr & SCE_CTRL_DOWN && temperature > 0.25f) temperature -= 0.1f;
        if (pad.buttons & SCE_CTRL_TRIANGLE) {
            if (++hold >= 90) { reset_request = 1; hold = 0; page_clear(); written = 0; }
        } else hold = 0;

        /* une lettre toutes les 2 images ; une nouvelle page après ~900 lettres */
        if (++frame % 2 == 0) {
            if (written++ > 900) { page_clear(); written = 0; }
            write_one_letter(temperature);
        }
        draw(temperature, hold);
        gfx_flip();
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    return 0;
}
