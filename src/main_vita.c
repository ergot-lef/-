/*
 * - : version PS Vita.
 *
 * Ce fichier ne contient aucune intention : il branche la memoire du monde
 * sur l'ecran, le haut-parleur et les capteurs, puis laisse le monde tourner.
 *
 *   START (hors du monde)        : pause / reprise
 *   en pause, maintenir TRIANGLE : semer un nouveau monde (l'ancien est perdu)
 */
#include <psp2/audioin.h>
#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/motion.h>
#include <psp2/power.h>
#include <psp2/touch.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "world.h"

#define DIR          "ux0:data/-"
#define SAVE_PATH    DIR "/monde.bin"
#define JOURNAL_PATH DIR "/journal.csv"

#define SCR_W 960
#define SCR_H 544
#define MEASURE_EVERY_S 10
#define SAVE_EVERY_S    120
#define HISTORY         900   /* points gardes pour les courbes de la pause */

static World w;
static volatile int paused, reseed_request, quit;
static volatile int mic_level;

static Metrics history[HISTORY];
static volatile int history_len;

/* ------------------------------------------------------------------ */
/* Hasard reel                                                         */
/* ------------------------------------------------------------------ */

static void true_seed(uint8_t out[32])
{
    memset(out, 0, 32);
    sceKernelGetRandomNumber(out, 32);
    /* On melange aussi l'instant exact et le bruit des capteurs. */
    SceUInt64 t = sceKernelGetProcessTimeWide();
    SceMotionSensorState ms;
    memset(&ms, 0, sizeof ms);
    sceMotionGetSensorState(&ms, 1);
    uint8_t extra[32];
    memcpy(extra, &t, 8);
    memcpy(extra + 8, &ms.accelerometer, 12);
    memcpy(extra + 20, &ms.gyro, 12);
    for (int i = 0; i < 32; i++) out[i] ^= extra[i];
}

/* ------------------------------------------------------------------ */
/* Journal muet : des nombres, horodates                               */
/* ------------------------------------------------------------------ */

static void journal_new(void)
{
    FILE *f = fopen(JOURNAL_PATH, "w");
    if (!f) return;
    fprintf(f, "# graine ");
    for (int i = 0; i < 32; i++) fprintf(f, "%02X", w.seed[i]);
    fprintf(f, "\nunix,epoque,entropie,diversite,densite,activite,son\n");
    fclose(f);
}

static void journal_add(const Metrics *m)
{
    FILE *f = fopen(JOURNAL_PATH, "a");
    if (f) {
        fprintf(f, "%lld,%llu,%.4f,%.4f,%.4f,%.1f,%.4f\n", (long long)time(NULL),
                (unsigned long long)w.epoch, m->entropy, m->distinct, m->density,
                m->activity, m->sound);
        fclose(f);
    }
    int n = history_len;
    if (n == HISTORY) {
        memmove(history, history + 1, sizeof(Metrics) * (HISTORY - 1));
        n--;
    }
    history[n] = *m;
    history_len = n + 1;
}

/* ------------------------------------------------------------------ */
/* Le temps du monde (thread a part)                                   */
/* ------------------------------------------------------------------ */

static void seed_new_world(void)
{
    uint8_t seed[32];
    true_seed(seed);
    world_new(&w, seed);
    history_len = 0;
    journal_new();
    world_save(&w, SAVE_PATH);
}

static int sim_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    time_t last_measure = time(NULL), last_save = last_measure;
    while (!quit) {
        if (reseed_request) {
            seed_new_world();
            reseed_request = 0;
        }
        if (paused) {
            sceKernelDelayThread(50 * 1000);
            continue;
        }
        world_epoch(&w);
        time_t now = time(NULL);
        if (now - last_measure >= MEASURE_EVERY_S) {
            Metrics m;
            world_measure(&w, &m);
            journal_add(&m);
            last_measure = now;
        }
        if (now - last_save >= SAVE_EVERY_S) {
            world_save(&w, SAVE_PATH);
            last_save = now;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Le son : la derniere ligne du monde, jouee en boucle a 8000 Hz       */
/* ------------------------------------------------------------------ */

static int audio_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    enum { GRAIN = 480 };
    int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, GRAIN, 48000, SCE_AUDIO_OUT_MODE_MONO);
    if (port < 0) return 0;
    int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
    static int16_t buf[2][GRAIN];
    int cur = 0;
    uint32_t pos = 0;  /* position en 1/6 d'echantillon : 48000 / 8000 = 6 */
    while (!quit) {
        for (int i = 0; i < GRAIN; i++) {
            uint8_t v = w.mem[OFF_SOUND + (pos / 6) % SOUND_BYTES];
            buf[cur][i] = paused ? 0 : (int16_t)(((int)v - 128) << 7);
            pos = (pos + 1) % (SOUND_BYTES * 6);
        }
        sceAudioOutOutput(port, buf[cur]);
        cur ^= 1;
    }
    sceAudioOutReleasePort(port);
    return 0;
}

/* Le micro : seulement un niveau, injecte dans la bande d'entree. */
static int mic_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE, 256, 16000, SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
    if (port < 0) return 0;
    static int16_t buf[256];
    while (!quit) {
        if (sceAudioInInput(port, buf) < 0) { sceKernelDelayThread(100000); continue; }
        int peak = 0;
        for (int i = 0; i < 256; i++) { int a = buf[i] < 0 ? -buf[i] : buf[i]; if (a > peak) peak = a; }
        mic_level = peak >> 7;
    }
    sceAudioInReleasePort(port);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Les capteurs : 64 octets bruts, sans dire a qui ils appartiennent    */
/* ------------------------------------------------------------------ */

static uint8_t clamp8(float v) { return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v; }

static void read_sensors(uint8_t in[TAPE], const SceCtrlData *pad)
{
    memset(in, 0, TAPE);
    SceTouchData tf, tb;
    memset(&tf, 0, sizeof tf);
    memset(&tb, 0, sizeof tb);
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &tf, 1);
    sceTouchPeek(SCE_TOUCH_PORT_BACK, &tb, 1);
    for (unsigned i = 0; i < 2 && i < tf.reportNum; i++) {
        in[0 + i * 3] = (uint8_t)(tf.report[i].x >> 3);
        in[1 + i * 3] = (uint8_t)(tf.report[i].y >> 3);
        in[2 + i * 3] = tf.report[i].force ? tf.report[i].force : 1;
    }
    for (unsigned i = 0; i < 2 && i < tb.reportNum; i++) {
        in[6 + i * 3] = (uint8_t)(tb.report[i].x >> 3);
        in[7 + i * 3] = (uint8_t)(tb.report[i].y >> 3);
        in[8 + i * 3] = tb.report[i].force ? tb.report[i].force : 1;
    }
    uint32_t b = pad->buttons & ~SCE_CTRL_START;
    memcpy(in + 12, &b, 4);
    in[16] = pad->lx; in[17] = pad->ly; in[18] = pad->rx; in[19] = pad->ry;

    SceMotionSensorState ms;
    memset(&ms, 0, sizeof ms);
    if (sceMotionGetSensorState(&ms, 1) >= 0) {
        in[20] = clamp8(128 + ms.accelerometer.x * 64);
        in[21] = clamp8(128 + ms.accelerometer.y * 64);
        in[22] = clamp8(128 + ms.accelerometer.z * 64);
        in[23] = clamp8(128 + ms.gyro.x * 16);
        in[24] = clamp8(128 + ms.gyro.y * 16);
        in[25] = clamp8(128 + ms.gyro.z * 16);
    }
    in[26] = (uint8_t)(mic_level > 255 ? 255 : mic_level);
    uint32_t now = (uint32_t)time(NULL);
    memcpy(in + 27, &now, 4);
    in[31] = (uint8_t)scePowerGetBatteryLifePercent();
}

/* ------------------------------------------------------------------ */
/* L'ecran : chaque octet est un pixel, colore par la palette du monde  */
/* ------------------------------------------------------------------ */

static uint32_t *fb[2];

static void draw_world(uint32_t *dst)
{
    const uint8_t *p = w.mem + OFF_PALETTE;
    uint32_t pal[256];
    for (int i = 0; i < 256; i++)
        pal[i] = 0xFF000000u | ((uint32_t)p[i * 3 + 2] << 16) | ((uint32_t)p[i * 3 + 1] << 8) | p[i * 3];
    for (int ty = 0; ty < GH; ty++)
        for (int tx = 0; tx < GW; tx++) {
            const uint8_t *t = w.mem + (size_t)(ty * GW + tx) * TAPE;
            uint32_t *o = dst + ty * 8 * SCR_W + tx * 8;
            for (int r = 0; r < 8; r++, o += SCR_W, t += 8)
                for (int c = 0; c < 8; c++) o[c] = pal[t[c]];
        }
}

/* Pause : l'image s'assombrit, les mesures du journal s'affichent en courbes. */
static void draw_pause(uint32_t *dst, int hold)
{
    for (int i = 0; i < SCR_W * SCR_H; i++) dst[i] = 0xFF000000u | ((dst[i] >> 2) & 0x003F3F3Fu);

    int n = history_len;
    const int x0 = 40, x1 = SCR_W - 40, rows = 5, h = 80, gap = 16, y0 = 40;
    for (int k = 0; k < rows; k++) {
        int top = y0 + k * (h + gap);
        for (int x = x0; x < x1; x++) dst[(top + h) * SCR_W + x] = 0xFF404040u;
        if (n < 2) continue;
        double lo = 1e30, hi = -1e30, v[HISTORY];
        for (int i = 0; i < n; i++) {
            const Metrics *m = &history[i];
            v[i] = k == 0 ? m->entropy : k == 1 ? m->distinct : k == 2 ? m->density
                 : k == 3 ? m->activity : m->sound;
            if (v[i] < lo) lo = v[i];
            if (v[i] > hi) hi = v[i];
        }
        if (hi - lo < 1e-9) hi = lo + 1e-9;
        for (int x = x0; x < x1; x++) {
            int i = (int)((double)(x - x0) / (x1 - x0) * (n - 1));
            int y = top + h - (int)((v[i] - lo) / (hi - lo) * h);
            dst[y * SCR_W + x] = 0xFFFFFFFFu;
        }
    }
    /* Maintenir TRIANGLE : une barre se remplit, puis un nouveau monde est seme. */
    if (hold > 0) {
        int len = (x1 - x0) * hold / 90;
        for (int y = SCR_H - 24; y < SCR_H - 16; y++)
            for (int x = x0; x < x0 + len; x++) dst[y * SCR_W + x] = 0xFFFFFFFFu;
    }
}

static int alloc_fb(void)
{
    const SceSize size = 2 * 1024 * 1024;  /* 960*544*4 arrondi a 256 Ko */
    for (int i = 0; i < 2; i++) {
        SceUID id = sceKernelAllocMemBlock("fb", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, size, NULL);
        if (id < 0 || sceKernelGetMemBlockBase(id, (void **)&fb[i]) < 0) return -1;
    }
    return 0;
}

static void show(int i)
{
    SceDisplayFrameBuf f;
    memset(&f, 0, sizeof f);
    f.size = sizeof f;
    f.base = fb[i];
    f.pitch = SCR_W;
    f.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    f.width = SCR_W;
    f.height = SCR_H;
    sceDisplaySetFrameBuf(&f, SCE_DISPLAY_SETBUF_NEXTFRAME);
}

/* ------------------------------------------------------------------ */

int main(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
    sceMotionStartSampling();

    sceIoMkdir(DIR, 0777);
    if (world_load(&w, SAVE_PATH) < 0) seed_new_world();

    if (alloc_fb() < 0) { sceKernelExitProcess(0); return 0; }

    SceUID th;
    /* Un coeur pour le monde, un pour le son et le micro, l'ecran garde le sien. */
    th = sceKernelCreateThread("monde", sim_thread, 160, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    th = sceKernelCreateThread("son", audio_thread, 64, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);
    th = sceKernelCreateThread("micro", mic_thread, 100, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);

    int cur = 0, hold = 0;
    uint32_t prev = 0;
    uint8_t in[TAPE];
    for (;;) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof pad);
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~prev;
        prev = pad.buttons;

        if (pressed & SCE_CTRL_START) {
            paused = !paused;
            if (paused) world_save(&w, SAVE_PATH);
            hold = 0;
        }
        if (paused && (pad.buttons & SCE_CTRL_TRIANGLE)) {
            if (++hold >= 90) { reseed_request = 1; paused = 0; hold = 0; }
        } else {
            hold = 0;
        }

        if (!paused) {
            read_sensors(in, &pad);
            world_set_input(&w, in);
        }

        draw_world(fb[cur]);
        if (paused) draw_pause(fb[cur], hold);
        show(cur);
        sceDisplayWaitVblankStartMulti(2);   /* 30 images par seconde */
        cur ^= 1;

        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    return 0;
}
