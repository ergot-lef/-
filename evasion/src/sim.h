/*
 * Évasion : un bonhomme articulé apprend seul à marcher, à sortir de sa cellule,
 * puis à franchir des obstacles.
 *
 * Le corps : 11 points (tête, cou, bassin, genoux, chevilles, talons, orteils) reliés par des os
 * de longueur fixe, soumis à la gravité et au frottement du sol.
 * Les muscles : 4 articulations (deux hanches, deux genoux). Le cerveau choisit
 * à chaque instant l'angle visé pour chacune ; le muscle tire vers cet angle.
 * Le cerveau : un petit réseau de neurones (15 entrées, 12 neurones, 4 sorties).
 *
 * L'apprentissage : 40 bonshommes essaient en même temps. Un essai s'arrête quand il
 * tombe dans un trou, quand il n'avance plus, ou (règle stricte) quand sa tête touche le sol.
 * Les plus avancés deviennent les parents de la génération suivante (avec mutations).
 * Personne ne leur montre comment marcher.
 */
#ifndef SIM_H
#define SIM_H

#include <stdint.h>

#define NPTS   11
#define NB_IN  15
#define NB_HID 12
#define NB_OUT 4
#define POP    40

#define FLOOR_Y   420.0f
#define CELL_X0   0.0f      /* mur gauche de la cellule */
#define CELL_X1   240.0f    /* mur droit, avec la porte */
#define CELL_TOP  250.0f    /* plafond                  */
#define DOOR_TOP  300.0f    /* haut de l'ouverture      */
#define COURSE_LEN 6000.0f
#define MAX_OBS   64

enum { P_HEAD, P_NECK, P_HIP, P_KNEE_L, P_FOOT_L, P_KNEE_R, P_FOOT_R, P_TOE_L, P_TOE_R, P_HEEL_L, P_HEEL_R };

typedef struct {
    float w1[NB_HID][NB_IN + 1];
    float w2[NB_OUT][NB_HID + 1];
} Brain;

typedef struct {
    float x[NPTS], y[NPTS], px[NPTS], py[NPTS];
    int   alive;
    float best_x;           /* plus loin atteint (bassin) */
    int   stall;            /* pas sans progrès           */
    int   steps;
    float in[NB_IN], hid[NB_HID], out[NB_OUT];
    int   contact[2];
} Body;

typedef struct {
    float x0, x1;           /* étendue horizontale */
    float h;                /* > 0 : bloc de hauteur h ; < 0 : trou */
} Obstacle;

typedef struct {
    Brain    brains[POP];
    Body     bodies[POP];
    float    fitness[POP];
    Obstacle obs[MAX_OBS];
    int      nobs;
    uint32_t generation;
    uint64_t rng;
    int      step;              /* pas dans la génération en cours */
    float    record;            /* meilleure distance de tous les temps */
    float    gen_best[2000];    /* meilleure distance de chaque génération */
    int      ngen_best;
    uint32_t first_exit_gen, first_obstacle_gen;   /* moments marquants (0 = pas encore) */
    int      obstacles_record;                     /* obstacles franchis au mieux */
    int      strict;            /* 1 : une chute (tête au sol) élimine ; 0 : il peut ramper */
} Sim;

void  sim_new(Sim *s, uint64_t seed);
void  sim_step(Sim *s);                  /* fait avancer tous les bonshommes d'un pas */
int   sim_alive(const Sim *s);
float ground_at(const Sim *s, float x);  /* hauteur du sol (y) à l'abscisse x */
int   sim_leader(const Sim *s);          /* bonhomme vivant le plus avancé */
int   obstacles_passed(const Sim *s, float x);
int   sim_save(const Sim *s, const char *path);
int   sim_load(Sim *s, const char *path);

extern const char *const sim_input_names[NB_IN];
extern const char *const sim_output_names[NB_OUT];

#endif
