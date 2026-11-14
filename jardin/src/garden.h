/*
 * Jardin : des créatures avec un vrai petit cerveau, dans un monde qui leur résiste.
 *
 * - De la nourriture pousse sur le sol, plus ou moins selon l'endroit (terres fertiles)
 *   et la saison (abondante au printemps et en été, rare en hiver).
 * - Chaque créature voit devant elle (nourriture et voisins à gauche, en face, à droite),
 *   sent son énergie, et décide : tourner, avancer, attaquer.
 * - Bouger coûte de l'énergie. Penser aussi : chaque neurone consomme un peu.
 * - Avec assez d'énergie, elle a un petit. Le cerveau du petit est une copie
 *   légèrement modifiée (mutations), et peut gagner ou perdre des neurones.
 * - À zéro d'énergie, elle meurt et nourrit le sol.
 * Personne ne leur dit quoi faire : ce qui marche se transmet, le reste disparaît.
 */
#ifndef GARDEN_H
#define GARDEN_H

#include <stdint.h>

#define WORLD_W 960.0f
#define WORLD_H 512.0f
#define CELL    16
#define FW      60          /* 960 / 16 */
#define FH      32          /* 512 / 16 */

#define NI      9           /* entrées du cerveau  */
#define NH_MAX  16          /* neurones cachés max */
#define NO      3           /* sorties             */
#define MAXC    700         /* créatures max       */

#define SEASON_TICKS 6000   /* durée d'une année   */

typedef struct {
    int   nh;                       /* neurones cachés utilisés */
    float win[NH_MAX][NI];
    float wh[NH_MAX][NH_MAX];       /* mémoire : chaque neurone écoute les autres au tour précédent */
    float bh[NH_MAX];
    float wout[NO][NH_MAX];
    float bo[NO];
} Brain;

typedef struct {
    int      alive;
    float    x, y, a;               /* position, direction */
    float    energy;
    uint32_t age, gen, kids;
    uint32_t id, parent;
    uint8_t  r, g, b;               /* couleur héritée (montre les lignées) */
    Brain    brain;
    float    in[NI], h[NH_MAX], out[NO];   /* dernière pensée (pour l'affichage) */
    float    speed;
    int      attacking;
} Creature;

typedef struct {
    float    food[FH][FW];
    float    fert[FH][FW];          /* fertilité du sol */
    Creature c[MAXC];
    int      count;
    uint64_t tick;
    uint64_t rng;
    uint32_t next_id;
    uint32_t extinctions;
    uint32_t births, deaths, kills;          /* depuis le début */
    float    kill_rate;                      /* attaques mortelles récentes (moyenne glissante) */
} Garden;

typedef struct {
    float pop, brain, kills, gen, food;
} GardenStats;

void garden_new(Garden *g, uint64_t seed);
void garden_step(Garden *g);
void garden_stats(const Garden *g, GardenStats *s);
float garden_season(const Garden *g);       /* 0..1 : abondance de la saison */
const char *garden_season_name(const Garden *g);
int  garden_save(const Garden *g, const char *path);
int  garden_load(Garden *g, const char *path);

extern const char *const garden_input_names[NI];
extern const char *const garden_output_names[NO];

#endif
