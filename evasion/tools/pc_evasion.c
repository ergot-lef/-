/* Évasion sur PC, sans écran : affiche les progrès génération après génération.
 * Usage : pc_evasion [générations] */
#include "../src/sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static Sim s;

int main(int argc, char **argv)
{
    int gens = argc > 1 ? atoi(argv[1]) : 300;
    sim_new(&s, argc > 2 ? strtoull(argv[2], 0, 10) : (uint64_t)time(NULL));
    clock_t t0 = clock();
    uint32_t g = s.generation;
    while ((int)s.generation <= gens) {
        sim_step(&s);
        if (s.generation != g) {
            g = s.generation;
            if (g % 10 == 0 || g < 6)
                printf("gén %4u  meilleur %6.0f  record %6.0f  obstacles %2d  sortie gén %u  (%.0fs)\n", g - 1,
                       s.gen_best[s.ngen_best - 1], s.record, s.obstacles_record, s.first_exit_gen,
                       (double)(clock() - t0) / CLOCKS_PER_SEC);
            fflush(stdout);
        }
    }
    return 0;
}
