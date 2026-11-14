/* Aperçu PC : fait évoluer N générations, puis enregistre des images du meneur. */
#include "../src/sim.h"
#include "../src/view.h"
#include "../../common/gfx.h"
#include <stdio.h>
#include <stdlib.h>
static Sim s; static ViewState v; static uint32_t buf[SCR_W * SCR_H];
int main(int argc, char **argv)
{
    int gens = argc > 1 ? atoi(argv[1]) : 30;
    gfx_init_pc(buf);
    sim_new(&s, 1);
    while ((int)s.generation < gens) sim_step(&s);
    v.speed = 1; v.cam_x = -40; v.show_brain = argc > 2;
    char p[64]; int shot = 0;
    for (int t = 0; t < 3600 && sim_alive(&s); t++) {
        sim_step(&s); view_follow(&s, &v);
        if (t % 60 == 0 && shot < 12) { view_draw(&s, &v); snprintf(p, sizeof p, "/tmp/ev_%02d.ppm", shot++); gfx_save_ppm(p); }
    }
    printf("%d images\n", shot);
    return 0;
}
