/* Entraîne l'Écrivain sur PC (sans écran), pour tester.
 * Usage : pc_train livre.txt [lettres] */
#include "../src/model.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static Model m;

static void write_sample(int n)
{
    Trace t;
    int ctx[CTX];
    for (int c = 0; c < CTX; c++) ctx[c] = 0;   /* lettre la plus fréquente : l'espace */
    for (int i = 0; i < n; i++) {
        model_predict(&m, ctx, &t);
        int v = model_sample(&m, t.p, 0.8f);
        uint32_t cp = m.vocab[v];
        if (cp < 0x80) putchar((int)cp);
        else if (cp < 0x800) { putchar(0xC0 | (cp >> 6)); putchar(0x80 | (cp & 63)); }
        else { putchar(0xE0 | (cp >> 12)); putchar(0x80 | ((cp >> 6) & 63)); putchar(0x80 | (cp & 63)); }
        for (int c = 0; c < CTX - 1; c++) ctx[c] = ctx[c + 1];
        ctx[CTX - 1] = v;
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) return 1;
    buf[sz] = 0;
    fclose(f);

    uint32_t vocab[VMAX];
    int vsize, n;
    int *text = text_encode(buf, &n, vocab, &vsize);
    printf("%d lettres, %d différentes\n", n, vsize);
    model_init(&m, text, n, vocab, vsize, (uint64_t)time(NULL));

    long total = argc > 2 ? atol(argv[2]) : 2000000;
    clock_t t0 = clock();
    uint64_t r = 12345;
    for (long s = 1; s <= total; s++) {
        r = r * 6364136223846793005ull + 1442695040888963407ull;
        int i = CTX + (int)((r >> 33) % (uint64_t)(n - CTX));
        float lr = 0.03f;
        model_train(&m, text, i, lr);
        if (s % 200000 == 0) {
            double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
            printf("\n--- %ld lettres  perte %.2f bits/lettre  réussite %.0f%%  (%.0f lettres/s)\n",
                   s, m.loss_avg, m.acc_avg * 100, s / sec);
            write_sample(240);
        }
    }
    return 0;
}
