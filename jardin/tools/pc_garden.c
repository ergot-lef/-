/* Fait tourner le Jardin sur PC sans écran et affiche son évolution.
 * Usage : pc_garden [ticks] */
#include "../src/garden.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static Garden g;

int main(int argc, char **argv)
{
    long ticks = argc > 1 ? atol(argv[1]) : 120000;
    garden_new(&g, (uint64_t)time(NULL) * 2654435761ull);
    printf("   tick  saison      pop  cerveau  géné  morts/attaque  nourriture  extinctions\n");
    clock_t t0 = clock();
    for (long t = 0; t <= ticks; t++) {
        if (t % 7500 == 0) {
            GardenStats s;
            garden_stats(&g, &s);
            printf("%7ld  %-10s %4.0f  %7.2f  %5.1f  %13.2f  %10.2f  %3u   (%.0fs)\n", t, garden_season_name(&g), s.pop,
                   s.brain, s.gen, s.kills, s.food, g.extinctions, (double)(clock() - t0) / CLOCKS_PER_SEC);
            fflush(stdout);
        }
        garden_step(&g);
    }
    return 0;
}
