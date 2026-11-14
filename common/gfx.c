#include "gfx.h"
#include "font.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t *gfx_px;

static inline void put(int x, int y, uint32_t c)
{
    if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) gfx_px[y * SCR_W + x] = c;
}

void gfx_clear(uint32_t c)
{
    for (int i = 0; i < SCR_W * SCR_H; i++) gfx_px[i] = c;
}

void gfx_rect(int x, int y, int w, int h, uint32_t c)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > SCR_W ? SCR_W : x + w, y1 = y + h > SCR_H ? SCR_H : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) gfx_px[j * SCR_W + i] = c;
}

void gfx_frame(int x, int y, int w, int h, uint32_t c)
{
    gfx_rect(x, y, w, 1, c);
    gfx_rect(x, y + h - 1, w, 1, c);
    gfx_rect(x, y, 1, h, c);
    gfx_rect(x + w - 1, y, 1, h, c);
}

void gfx_line(int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 4000; guard++) {
        put(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_disc(int cx, int cy, int r, uint32_t c)
{
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r + r) put(cx + i, cy + j, c);
}

void gfx_dim(int x, int y, int w, int h, int shift)
{
    uint32_t mask = (0xFFu >> shift) * 0x010101u;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if ((unsigned)i < SCR_W && (unsigned)j < SCR_H) {
                uint32_t *p = &gfx_px[j * SCR_W + i];
                *p = 0xFF000000u | ((*p >> shift) & mask);
            }
}

uint32_t utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = *p++;
    if (c >= 0xF0 && (p[0] & 0xC0) == 0x80) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0 && (p[0] & 0xC0) == 0x80) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0 && (p[0] & 0xC0) == 0x80) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *s = (const char *)p;
    return c;
}

static int glyph_index(uint32_t cp)
{
    if (cp >= 32 && cp < 127) return (int)cp - 32;
    if (cp >= 160 && cp < 256) return (int)cp - 160 + 95;
    for (int i = 191; i < FONT_COUNT; i++)
        if (font_cp[i] == cp) return i;
    return '?' - 32;
}

static void draw_glyph(int x, int y, uint32_t c, uint32_t cp)
{
    const uint8_t *g = font_px[glyph_index(cp)];
    uint32_t cr = c & 0xFF, cg = (c >> 8) & 0xFF, cb = (c >> 16) & 0xFF;
    for (int j = 0; j < FONT_H_PX; j++)
        for (int i = 0; i < FONT_W; i++) {
            uint32_t a = g[j * FONT_W + i];
            if (!a) continue;
            int px = x + i, py = y + j;
            if ((unsigned)px >= SCR_W || (unsigned)py >= SCR_H) continue;
            uint32_t d = gfx_px[py * SCR_W + px];
            uint32_t r = ((d & 0xFF) * (255 - a) + cr * a) / 255;
            uint32_t gg = (((d >> 8) & 0xFF) * (255 - a) + cg * a) / 255;
            uint32_t b = (((d >> 16) & 0xFF) * (255 - a) + cb * a) / 255;
            gfx_px[py * SCR_W + px] = 0xFF000000u | (b << 16) | (gg << 8) | r;
        }
}

int gfx_text(int x, int y, uint32_t c, const char *s)
{
    int cx = x, maxw = 0;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        if (cp == '\n') { cx = x; y += FONT_H_PX; continue; }
        draw_glyph(cx, y, c, cp);
        cx += FONT_W;
        if (cx - x > maxw) maxw = cx - x;
    }
    return maxw;
}

int gfx_textf(int x, int y, uint32_t c, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return gfx_text(x, y, c, buf);
}

void gfx_plot(int x, int y, int w, int h, const float *v, int n, uint32_t c)
{
    gfx_frame(x, y, w, h, RGB(60, 60, 60));
    if (n < 2) return;
    float lo = v[0], hi = v[0];
    for (int i = 1; i < n; i++) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
    if (hi - lo < 1e-9f) hi = lo + 1e-9f;
    int px = -1, py = -1;
    for (int xi = 0; xi < w - 2; xi++) {
        int i = (int)((long long)xi * (n - 1) / (w - 3 > 0 ? w - 3 : 1));
        int yy = y + h - 2 - (int)((v[i] - lo) / (hi - lo) * (h - 4));
        if (px >= 0) gfx_line(px, py, x + 1 + xi, yy, c);
        px = x + 1 + xi; py = yy;
    }
}

/* ------------------------------------------------------------------ */
#ifdef __vita__
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>

static uint32_t *fb[2];
static int cur;

int gfx_init(void)
{
    for (int i = 0; i < 2; i++) {
        SceUID id = sceKernelAllocMemBlock("fb", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 2 * 1024 * 1024, NULL);
        if (id < 0 || sceKernelGetMemBlockBase(id, (void **)&fb[i]) < 0) return -1;
    }
    cur = 0;
    gfx_px = fb[0];
    return 0;
}

void gfx_flip(void)
{
    SceDisplayFrameBuf f;
    memset(&f, 0, sizeof f);
    f.size = sizeof f;
    f.base = fb[cur];
    f.pitch = SCR_W;
    f.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    f.width = SCR_W;
    f.height = SCR_H;
    sceDisplaySetFrameBuf(&f, SCE_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStartMulti(2);
    cur ^= 1;
    gfx_px = fb[cur];
}
#else
void gfx_init_pc(uint32_t *buffer) { gfx_px = buffer; }

int gfx_save_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        uint32_t p = gfx_px[i];
        unsigned char rgb[3] = { (unsigned char)(p & 0xFF), (unsigned char)((p >> 8) & 0xFF), (unsigned char)((p >> 16) & 0xFF) };
        fwrite(rgb, 3, 1, f);
    }
    fclose(f);
    return 0;
}
#endif
