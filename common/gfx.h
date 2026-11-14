/* Dessin logiciel minimal sur une image 960 x 544 (ecran de la Vita).
 * Les fonctions de dessin marchent aussi sur PC (pour les apercus) ;
 * seules gfx_init / gfx_flip parlent a la console. */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define SCR_W 960
#define SCR_H 544

/* Couleur : 0xAABBGGRR (format de l'ecran de la Vita) */
#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

extern uint32_t *gfx_px;   /* image en cours de dessin */

void gfx_clear(uint32_t c);
void gfx_rect(int x, int y, int w, int h, uint32_t c);
void gfx_frame(int x, int y, int w, int h, uint32_t c);
void gfx_line(int x0, int y0, int x1, int y1, uint32_t c);
void gfx_disc(int cx, int cy, int r, uint32_t c);
void gfx_dim(int x, int y, int w, int h, int shift);   /* assombrit une zone */
/* Texte UTF-8, retourne la largeur dessinee. Retour a la ligne sur '\n'. */
int  gfx_text(int x, int y, uint32_t c, const char *utf8);
int  gfx_textf(int x, int y, uint32_t c, const char *fmt, ...);
/* Trace une courbe de n valeurs dans un cadre, mise a l'echelle automatique. */
void gfx_plot(int x, int y, int w, int h, const float *v, int n, uint32_t c);
/* Decode un caractere UTF-8, avance *s. */
uint32_t utf8_next(const char **s);

#ifdef __vita__
int  gfx_init(void);
void gfx_flip(void);        /* affiche l'image et passe a la suivante (30 images/s) */
#else
void gfx_init_pc(uint32_t *buffer);
int  gfx_save_ppm(const char *path);
#endif

#endif
