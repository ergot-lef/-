/*
 * Jardin — version PS Vita.
 *   toucher une créature : voir son cerveau penser en direct (toucher ailleurs : fermer)
 *   L / R : ralentir / accélérer le temps
 *   START : pause et courbes de l'histoire du jardin
 *   △ maintenu 3 s : semer un nouveau jardin
 */
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>

#include <string.h>
#include <time.h>

#include "../../common/gfx.h"
#include "garden.h"
#include "view.h"

#define DIR       "ux0:data/jardin"
#define SAVE_PATH DIR "/jardin.bin"

static Garden g;
static ViewState v;
static volatile int reset_request;
static volatile int target_tps = 30;      /* pas de temps par seconde demandés */
static const int speeds[] = { 1, 3, 10, 30, 100, 1000 };

static void new_garden(void)
{
    uint64_t seed = 0;
    sceKernelGetRandomNumber(&seed, sizeof seed);
    garden_new(&g, seed ^ sceKernelGetProcessTimeWide());
    v.hist_n = 0;
    v.selected = -1;
}

static void record(void)
{
    GardenStats s;
    garden_stats(&g, &s);
    if (v.hist_n == VHIST) {
        for (int k = 0; k < 4; k++) memmove(v.hist[k], v.hist[k] + 1, sizeof(float) * (VHIST - 1));
        v.hist_n--;
    }
    v.hist[0][v.hist_n] = s.pop;
    v.hist[1][v.hist_n] = s.brain;
    v.hist[2][v.hist_n] = s.kills;
    v.hist[3][v.hist_n] = s.food;
    v.hist_n++;
}

static int sim_thread(SceSize a, void *b)
{
    (void)a; (void)b;
    SceUInt64 last = sceKernelGetProcessTimeWide();
    double budget = 0;
    time_t last_save = time(NULL);
    for (;;) {
        if (reset_request) { new_garden(); reset_request = 0; }
        SceUInt64 now = sceKernelGetProcessTimeWide();
        budget += (double)(now - last) * 1e-6 * target_tps;
        last = now;
        if (v.paused || budget < 1) { if (v.paused) budget = 0; sceKernelDelayThread(2000); continue; }
        if (budget > 2000) budget = 2000;   /* on fait ce qu'on peut, sans s'emballer */
        while (budget >= 1 && !v.paused) {
            garden_step(&g);
            if (g.tick % 200 == 0) record();
            budget -= 1;
            if (sceKernelGetProcessTimeWide() - now > 30000) break;
        }
        if (time(NULL) - last_save >= 120) { garden_save(&g, SAVE_PATH); last_save = time(NULL); }
    }
    return 0;
}

int main(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceIoMkdir(DIR, 0777);
    if (gfx_init() < 0) { sceKernelExitProcess(0); return 0; }

    memset(&v, 0, sizeof v);
    v.selected = -1;
    v.speed = 1;
    if (garden_load(&g, SAVE_PATH) < 0) new_garden();

    SceUID th = sceKernelCreateThread("jardin", sim_thread, 160, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);

    int sp = 0, touching = 0;
    uint32_t prev = 0;
    for (;;) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof pad);
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pr = pad.buttons & ~prev;
        prev = pad.buttons;
        if (pr & SCE_CTRL_START) { v.paused = !v.paused; if (v.paused) garden_save(&g, SAVE_PATH); }
        if (pr & SCE_CTRL_RTRIGGER && sp < 5) sp++;
        if (pr & SCE_CTRL_LTRIGGER && sp > 0) sp--;
        v.speed = speeds[sp];
        target_tps = 30 * speeds[sp];
        if (pad.buttons & SCE_CTRL_TRIANGLE) {
            if (++v.hold >= 90) { reset_request = 1; v.hold = 0; v.paused = 0; }
        } else v.hold = 0;

        SceTouchData t;
        memset(&t, 0, sizeof t);
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1);
        if (t.reportNum > 0 && !touching && !v.paused) {
            int x = t.report[0].x / 2, y = t.report[0].y / 2;   /* le tactile est en 1920 x 1088 */
            int p = view_pick(&g, x, y);
            v.selected = (p == v.selected) ? -1 : p;
            v.selected_id = v.selected >= 0 ? g.c[v.selected].id : 0;
        }
        touching = t.reportNum > 0;
        if (v.selected >= 0 && (!g.c[v.selected].alive || g.c[v.selected].id != v.selected_id)) v.selected = -1;

        view_draw(&g, &v);
        gfx_flip();
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    return 0;
}
