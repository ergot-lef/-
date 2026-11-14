/*
 * Version PC sans ecran du monde, pour tester la physique.
 * Usage : pc_sim [epoques] [dossier_images]
 * Affiche les mesures regulierement et, si un dossier est donne,
 * ecrit l'ecran (1 octet = 1 pixel, via la palette du monde) en PPM.
 */
#include "../src/world.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static World w;

static void dump(const char *dir, uint64_t epoch)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%08llu.ppm", dir, (unsigned long long)epoch);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n960 544\n255\n");
    const uint8_t *pal = w.mem + OFF_PALETTE;
    for (int y = 0; y < 544; y++)
        for (int x = 0; x < 960; x++) {
            int t = (y / 8) * GW + x / 8;
            uint8_t v = w.mem[t * TAPE + (y % 8) * 8 + (x % 8)];
            fwrite(pal + v * 3, 3, 1, f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    long epochs = argc > 1 ? atol(argv[1]) : 5000;
    const char *dir = argc > 2 ? argv[2] : NULL;

    uint8_t seed[32];
    FILE *r = fopen("/dev/urandom", "rb");
    if (!r || fread(seed, 32, 1, r) != 1) return 1;
    fclose(r);
    world_new(&w, seed);

    printf("graine ");
    for (int i = 0; i < 8; i++) printf("%02X", seed[i]);
    printf("\nepoque  entropie  diversite  densite  activite  son   (s)\n");

    clock_t t0 = clock();
    for (long e = 0; e <= epochs; e++) {
        if (e % 250 == 0) {
            Metrics m;
            world_measure(&w, &m);
            printf("%6ld  %8.3f  %9.3f  %7.3f  %8.1f  %4.2f  %.0f\n", e, m.entropy, m.distinct,
                   m.density, m.activity, m.sound, (double)(clock() - t0) / CLOCKS_PER_SEC);
            fflush(stdout);
            if (dir && e % 1000 == 0) dump(dir, e);
        }
        world_epoch(&w);
    }
    return 0;
}
