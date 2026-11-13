/* GooeyMB through the real pedal entry (arena, knob conversion, defaults): the defaults are flat in
 * level, switched off it leaves the buffer alone, and with LoThr down both sides get one gain. */
#include <stdio.h>
#include <math.h>
#include "stereo_entry.h"
unsigned char _Fx_DLY_GooeyMB_Pic[4096];   /* the cover the linker would resolve */
#define ZM_FW_OK() 0u
#define ZM_POST() ((void)0)
#include "../src/custom/gooeymb/gooeymb.c"
#define D(n) params[GOOEYMB_##n##_SLOT] = GOOEYMB_##n##_UI_DEFAULT / 100.0f

/* loudest output on each side over the last 100 of n blocks: a 50 Hz sine, L at a, R at 0.4 a */
static void play(int n, float a, float *pl, float *pr)
{
    int b, i;
    *pl = 0; *pr = 0;
    for (b = 0; b < n; b++) {
        for (i = 0; i < 8; i++) {
            float x = sinf(6.2831853f * 50.0f * (float)(b * 8 + i) / 44100.0f);
            fx[i] = a * x; fx[i + 8] = 0.4f * a * x;
        }
        Fx_DLY_GooeyMB(ctx);
        if (b >= n - 100) for (i = 0; i < 8; i++) {
            if (fabsf(fx[i]) > *pl) *pl = fabsf(fx[i]);
            if (fabsf(fx[i + 8]) > *pr) *pr = fabsf(fx[i + 8]);
        }
    }
}

int main(void)
{
    int fails = 0, i;
    float pl, pr, off = 0;
    se_init(65536);
    D(LOTHR); D(HITHR); D(LEVEL); D(XOVER); D(LORAT); D(HIRAT); D(ATTCK); D(RELS); D(MIX);

    play(3000, 0.5f, &pl, &pr);
    printf("defaults: L %.4f R %.4f (0.5, 0.2)\n", pl, pr);
    if (fabsf(pl - 0.5f) > 0.005f || fabsf(pr - 0.2f) > 0.002f) { puts("FAIL: defaults not flat"); fails++; }

    params[0] = 0.0f;                                                  /* effect off: untouched */
    for (i = 0; i < 16; i++) fx[i] = 0.5f * (float)(i + 1);
    Fx_DLY_GooeyMB(ctx);
    for (i = 0; i < 16; i++) off += fabsf(fx[i] - 0.5f * (float)(i + 1));
    if (off != 0.0f) { puts("FAIL: switched off but the buffer changed"); fails++; }
    params[0] = 1.0f;

    params[GOOEYMB_LOTHR_SLOT] = 1.00f;                                /* -30 dB */
    params[GOOEYMB_RELS_SLOT] = 0.03f;                                 /* 1.2 s */
    play(1500, 0.25f, &pl, &pr);                                       /* alone, R (-20 dBFS) would get x3.35 */
    printf("LoThr -30: gain L %.4f R %.4f (linked)\n", pl / 0.25f, pr / 0.1f);
    if (!(pr / 0.1f < 2.5f && fabsf(pl / 0.25f - pr / 0.1f) < 0.01f)) { puts("FAIL: stereo link through the entry"); fails++; }
    return fails;
}
