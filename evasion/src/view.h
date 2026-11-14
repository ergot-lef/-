#ifndef VIEW_H
#define VIEW_H
#include "sim.h"
typedef struct {
    float cam_x;
    int   paused, speed, hold, show_brain;
} ViewState;
void view_draw(const Sim *s, const ViewState *v);
void view_follow(const Sim *s, ViewState *v);
#endif
