/* Aperçu PC : fait tourner le jardin puis enregistre des images de l'écran. */
#include "../src/garden.h"
#include "../src/view.h"
#include "../../common/gfx.h"
#include <stdio.h>
#include <string.h>

static Garden g;
static ViewState v;
static uint32_t buf[SCR_W * SCR_H];

int main(void)
{
    gfx_init_pc(buf);
    garden_new(&g, 42);
    memset(&v, 0, sizeof v);
    v.selected = -1; v.speed = 1;
    for (int t = 0; t < 20000; t++) {
        garden_step(&g);
        if (t % 100 == 0 && v.hist_n < VHIST) {
            GardenStats s; garden_stats(&g, &s);
            v.hist[0][v.hist_n] = s.pop; v.hist[1][v.hist_n] = s.brain; v.hist[2][v.hist_n] = s.kills; v.hist[3][v.hist_n] = s.food;
            v.hist_n++;
        }
    }
    for (int i = 0; i < MAXC; i++) if (g.c[i].alive) { v.selected = i; break; }
    view_draw(&g, &v);
    gfx_save_ppm("/tmp/jardin1.ppm");
    v.selected = -1; v.paused = 1;
    view_draw(&g, &v);
    gfx_save_ppm("/tmp/jardin2.ppm");
    return 0;
}
