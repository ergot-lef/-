#ifndef VIEW_H
#define VIEW_H

#include "garden.h"

#define VHIST 1500

typedef struct {
    int   selected;         /* créature suivie, ou -1 */
    uint32_t selected_id;   /* son identité (les places se libèrent et se réutilisent) */
    int   paused, speed, hold;
    float hist[4][VHIST];   /* créatures, neurones, chasse, nourriture */
    int   hist_n;
} ViewState;

void view_draw(const Garden *g, const ViewState *v);
int  view_pick(const Garden *g, int x, int y);

#endif
