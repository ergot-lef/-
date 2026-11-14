/*
 * -  : un monde sans intention.
 *
 * Le monde est un seul bloc de memoire, decoupe en bandes de 64 octets
 * posees sur une grille torique de 120 x 68 bandes. Chaque octet est un pixel
 * (une bande = un carre de 8 x 8 pixels, la grille entiere = 960 x 544 = l'ecran).
 *
 * Quelques bandes sont simplement "branchees" sur l'exterieur :
 *   - la table d'instructions (ce que signifie chaque octet quand il est execute),
 *   - la palette (couleur de chaque valeur d'octet),
 *   - l'entree (capteurs de la console, reecrits a chaque image),
 *   - la derniere ligne : le son (jouee en boucle).
 * Ces bandes vivent, mutent et peuvent etre reecrites comme toutes les autres.
 */
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include <stddef.h>

#define TAPE        64
#define GW          120
#define GH          68
#define NTAPES      (GW * GH)
#define WORLD_BYTES (NTAPES * TAPE)

/* Branchements (index de bandes) */
#define T_TABLE     0                   /* 4 bandes  = 256 octets  */
#define T_PALETTE   4                   /* 12 bandes = 768 octets  */
#define T_INPUT     16                  /* 1 bande   = 64 octets   */
#define T_SOUND     ((GH - 1) * GW)     /* derniere ligne = 7680 octets */
#define SOUND_BYTES (GW * TAPE)

#define OFF_TABLE   (T_TABLE * TAPE)
#define OFF_PALETTE (T_PALETTE * TAPE)
#define OFF_INPUT   (T_INPUT * TAPE)
#define OFF_SOUND   (T_SOUND * TAPE)

/* Physique de depart */
#define MAX_STEPS     8192      /* duree max d'une rencontre                 */
#define RADIUS        2         /* distance max entre deux bandes qui se rencontrent */
#define MUTATION_RATE 0.00024   /* probabilite de mutation par octet et par epoque */

typedef struct {
    uint8_t  mem[WORLD_BYTES];
    uint8_t  seed[32];      /* la graine : tirée du hasard réel, jamais choisie */
    uint64_t rng[4];
    uint64_t epoch;
    uint64_t steps_total;
    uint64_t steps_last;    /* instructions executees pendant la derniere epoque */
} World;

typedef struct {
    double entropy;     /* entropie des octets (0..8 bits)                          */
    double distinct;    /* part de sequences de 8 octets distinctes (0..1)          */
    double density;     /* part des octets qui sont des instructions actives (0..1) */
    double activity;    /* instructions executees par rencontre, en moyenne         */
    double sound;       /* energie de la zone son (0..1)                            */
} Metrics;

void     world_new(World *w, const uint8_t seed[32]);
void     world_epoch(World *w);
void     world_measure(World *w, Metrics *m);
void     world_set_input(World *w, const uint8_t in[TAPE]);
int      world_save(const World *w, const char *path);
int      world_load(World *w, const char *path);
uint64_t world_rand(World *w);

#endif
