/*
 * Évasion — version PS Vita.
 *   L / R    : ralentir / accélérer le temps
 *   □        : afficher le cerveau du meneur
 *   SELECT   : changer la règle (libre / stricte) — l'évolution continue avec la nouvelle règle
 *   START    : pause et courbe des progrès
 *   △ maintenu 3 s : tout recommencer de zéro
 */
#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>

#include <string.h>
#include <time.h>

#include "../../common/gfx.h"
#include "sim.h"
#include "view.h"

#define DIR       "ux0:data/evasion"
#define SAVE_PATH DIR "/evasion.bin"

static Sim s;
static ViewState v;
static volatile int reset_request, toggle_rule;
static volatile int target_sps = 60;
static const int speeds[] = { 1, 2, 4, 8, 32, 1000 };

static void new_sim(void)
{
    uint64_t seed = 0;
    sceKernelGetRandomNumber(&seed, sizeof seed);
    int strict = s.strict;
    sim_new(&s, seed ^ sceKernelGetProcessTimeWide());
    s.strict = strict;
    v.cam_x = -40;
}

static int sim_thread(SceSize a, void *b)
{
    (void)a; (void)b;
    SceUInt64 last = sceKernelGetProcessTimeWide();
    double budget = 0;
    time_t last_save = time(NULL);
    for (;;) {
        if (reset_request) { new_sim(); reset_request = 0; }
        if (toggle_rule) { s.strict = !s.strict; toggle_rule = 0; }
        SceUInt64 now = sceKernelGetProcessTimeWide();
        budget += (double)(now - last) * 1e-6 * target_sps;
        last = now;
        if (v.paused || budget < 1) { if (v.paused) budget = 0; sceKernelDelayThread(2000); continue; }
        if (budget > 5000) budget = 5000;
        while (budget >= 1 && !v.paused) {
            sim_step(&s);
            budget -= 1;
            if (sceKernelGetProcessTimeWide() - now > 30000) break;
        }
        if (time(NULL) - last_save >= 120) { sim_save(&s, SAVE_PATH); last_save = time(NULL); }
    }
    return 0;
}

int main(void)
{
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL);
    sceIoMkdir(DIR, 0777);
    if (gfx_init() < 0) { sceKernelExitProcess(0); return 0; }

    memset(&v, 0, sizeof v);
    v.speed = 1;
    v.cam_x = -40;
    if (sim_load(&s, SAVE_PATH) < 0) new_sim();

    SceUID th = sceKernelCreateThread("evasion", sim_thread, 160, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
    if (th >= 0) sceKernelStartThread(th, 0, NULL);

    int sp = 0;
    uint32_t prev = 0;
    for (;;) {
        SceCtrlData pad;
        memset(&pad, 0, sizeof pad);
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pr = pad.buttons & ~prev;
        prev = pad.buttons;
        if (pr & SCE_CTRL_START) { v.paused = !v.paused; if (v.paused) sim_save(&s, SAVE_PATH); }
        if (pr & SCE_CTRL_RTRIGGER && sp < 5) sp++;
        if (pr & SCE_CTRL_LTRIGGER && sp > 0) sp--;
        if (pr & SCE_CTRL_SQUARE) v.show_brain = !v.show_brain;
        if (pr & SCE_CTRL_SELECT) toggle_rule = 1;
        v.speed = speeds[sp];
        target_sps = 60 * speeds[sp];
        if (pad.buttons & SCE_CTRL_TRIANGLE) {
            if (++v.hold >= 90) { reset_request = 1; v.hold = 0; v.paused = 0; }
        } else v.hold = 0;

        view_follow(&s, &v);
        view_draw(&s, &v);
        gfx_flip();
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
    return 0;
}
