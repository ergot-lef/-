/*
 * Écrivain : un tout petit réseau de neurones qui apprend à écrire en lisant un livre.
 *
 * Il regarde les CTX dernières lettres et doit deviner la suivante.
 *   1. Chaque lettre est transformée en EMB nombres (son « plongement »).
 *   2. Ces nombres passent dans HID neurones cachés (activation tanh).
 *   3. Une couche de sortie donne une note pour chaque lettre possible,
 *      convertie en probabilités (softmax).
 * À chaque erreur, tous les poids sont légèrement corrigés (rétropropagation).
 * C'est le même principe que les grands modèles de langage, en minuscule.
 */
#ifndef MODEL_H
#define MODEL_H

#include <stdint.h>

#define CTX   8      /* lettres de contexte          */
#define EMB   12     /* nombres par lettre           */
#define HID   192    /* neurones cachés              */
#define VMAX  96     /* lettres différentes, au plus */
#define IN    (CTX * EMB)

typedef struct {
    /* vocabulaire */
    int      vsize;
    uint32_t vocab[VMAX];          /* code Unicode de chaque lettre */

    /* poids */
    float emb[VMAX][EMB];
    float w1[HID][IN];
    float b1[HID];
    float w2[VMAX][HID];
    float b2[VMAX];

    /* apprentissage */
    uint64_t seen;                 /* lettres lues            */
    double   loss_avg;             /* bits par lettre, lissé  */
    double   acc_avg;              /* taux de bonne réponse   */
    uint64_t rng;
} Model;

/* État d'un passage avant (pour afficher le « cerveau ») */
typedef struct {
    float x[IN];
    float h[HID];
    float p[VMAX];
} Trace;

void model_init(Model *m, const int *text, int n, const uint32_t *vocab, int vsize, uint64_t seed);
/* Une étape d'apprentissage sur la lettre text[i] (avec ses CTX précédentes). */
void model_train(Model *m, const int *text, int i, float lr);
/* Probabilités de la lettre suivante après un contexte de CTX indices. */
void model_predict(const Model *m, const int ctx[CTX], Trace *t);
/* Tire une lettre selon les probabilités, avec une « audace » (température). */
int  model_sample(Model *m, const float *p, float temperature);

int  model_save(const Model *m, const char *path);
int  model_load(Model *m, const char *path);

/* Lit un texte UTF-8, construit le vocabulaire (les VMAX lettres les plus fréquentes)
 * et renvoie le texte converti en indices (les lettres rares sont ignorées). */
int *text_encode(const char *utf8, int *n_out, uint32_t *vocab, int *vsize);
int *text_encode_with(const char *utf8, int *n_out, const uint32_t *vocab, int vsize);

#endif
