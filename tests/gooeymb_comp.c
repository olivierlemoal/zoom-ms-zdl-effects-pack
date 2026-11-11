/* GooeyMB: the crossover sums back flat, each band only compresses its own range (a loud low note
 * leaves the highs alone), static reduction per band, AUTO makeup per band, Mix per band, linked
 * stereo, crossover moves while playing, no clicks or NaN, labels. */
#define GOOEYMB_HOST_TEST
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#define ZM_WANT_BLOB
static unsigned char zm_t_blob[4096];
#define ZM_PIC zm_t_blob
#define ZM_FW_OK() 0u
#define ZM_POST() ((void)0)
#include "../src/custom/gooeymb/gooeymb.c"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define SR 44096                                       /* whole blocks; tones use it as the rate */
#define MAXN (SR * 10)
static float inL[MAXN], inR[MAXN], outL[MAXN], outR[MAXN], refL[MAXN];
static GmState S;

/* screen values: LoThr, HiThr, XOver, Level, LoRat, HiRat, Attck, Rels, Mix */
typedef struct { float lo, hi, xo, lr, hr, att, rel, mix, lv; } K;
static K kd(void) { K k = { 400, 400, 9, 4, 1, 4, 4, 100, 24 }; return k; }

static void run_from(int n, K k, int from_block, K k2)
{
    float u[9]; GmParams P; int b, i;
    gm_init(&S);
    for (b = 0; b < n / 8; b++) {
        float buf[16];
        K c = (b < from_block) ? k : k2;
        u[0] = c.lo; u[1] = c.hi; u[2] = c.xo; u[3] = c.lv; u[4] = c.lr; u[5] = c.hr; u[6] = c.att; u[7] = c.rel; u[8] = c.mix;
        for (i = 0; i < 8; i++) { buf[i] = inL[b * 8 + i]; buf[i + 8] = inR[b * 8 + i]; }
        gm_prepare(&P, &S, u);
        gm_process(&S, &P, buf);
        for (i = 0; i < 8; i++) { outL[b * 8 + i] = buf[i]; outR[b * 8 + i] = buf[i + 8]; }
    }
}
static void run(int n, K k) { run_from(n, k, 1 << 30, k); }
static void sine(float *x, int n, float f, float a) { int i; for (i = 0; i < n; i++) x[i] = a * sinf(6.2831853f * f * (float)i / (float)SR); }
static void add_sine(float *x, int n, float f, float a) { int i; for (i = 0; i < n; i++) x[i] += a * sinf(6.2831853f * f * (float)i / (float)SR + 0.3f); }
static float peak(const float *x, int a, int b) { float m = 0; int i; for (i = a; i < b; i++) if (fabsf(x[i]) > m) m = fabsf(x[i]); return m; }
static float db(float x) { return 20.0f * log10f(x); }
static float lin(float d) { return powf(10.0f, d / 20.0f); }
/* amplitude of the f component over [a, b) (whole periods of both test tones) */
static float amp_at(const float *x, int a, int b, float f)
{
    double re = 0, im = 0; int i;
    for (i = a; i < b; i++) { double ph = 6.283185307 * f * i / SR; re += x[i] * cos(ph); im += x[i] * sin(ph); }
    return (float)(2.0 * sqrt(re * re + im * im) / (b - a));
}
static unsigned int rng = 777;
static float rnd(void) { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) / 8388608.0f - 1.0f; }

/* steady reduction in dB of each band (0 low, 1 high) for a sine at f, level_db: the gain computer's
 * output, before the AUTO makeup gives it back (manual release, so it is all in rF) */
static void steady_gr(K k, float f, float level_db, float *gr)
{
    sine(inL, SR, f, lin(level_db));
    for (int j = 0; j < 2205; j++) inL[j] *= (float)j / 2205.0f;   /* 50 ms fade-in: no onset click */
    memcpy(inR, inL, sizeof(float) * SR);
    k.att = 2; k.rel = 3;
    run(SR / 2, k);
    gr[0] = (S.rS[0] + S.rF[0]) * 6.0206f; gr[1] = (S.rS[1] + S.rF[1]) * 6.0206f;
}

int main(void)
{
    int i;

    /* ---- 1. both bands OFF: the summed bands are flat in level at every crossover ---- */
    {
        static const float fr[6] = { 30, 60, 141, 400, 1000, 8000 };
        float worst = 0;
        int x, j;
        for (x = 0; x <= 20; x += 5) for (j = 0; j < 6; j++) {
            K k = kd(); float d;
            k.xo = (float)x;
            sine(inL, SR, fr[j], 0.5f); memcpy(inR, inL, sizeof(float) * SR);
            run(SR, k);
            d = fabsf(db(peak(outL, SR / 2, SR) / 0.5f));
            if (d > worst) worst = d;
        }
        printf("both OFF: worst level change %.3f dB (crossover 60..400 Hz, 30 Hz..8 kHz)\n", worst);
        CHECK(worst < 0.05f, "summed bands not flat: %.3f dB", worst);
    }

    /* ---- 2. each band compresses only its range ---- */
    {
        K k = kd(); float a[2], b2[2], c[2], d[2];
        k.lo = 100; k.lr = 4;                                 /* low band -30 dB, 4:1 */
        steady_gr(k, 50.0f, -6.0f, a); steady_gr(k, 2000.0f, -6.0f, b2);
        k = kd(); k.hi = 100; k.hr = 4;                       /* high band -30 dB, 4:1 */
        steady_gr(k, 50.0f, -6.0f, c); steady_gr(k, 2000.0f, -6.0f, d);
        printf("LoThr -30 4:1: 50 Hz GR %.2f dB, 2 kHz %.2f; HiThr -30: 50 Hz %.2f, 2 kHz %.2f (want about 18 where it compresses)\n",
               a[0], b2[0], c[1], d[1]);
        CHECK(fabsf(a[0] - 18.0f) < 1.5f, "low band on 50 Hz: %.2f dB", a[0]);
        CHECK(fabsf(d[1] - 18.0f) < 1.5f, "high band on 2 kHz: %.2f dB", d[1]);
        CHECK(b2[0] < 0.1f, "low band reacts to 2 kHz: %.2f dB", b2[0]);
        CHECK(c[1] < 0.1f, "high band reacts to 50 Hz: %.2f dB", c[1]);
        CHECK(a[1] == 0.0f && d[0] == 0.0f, "a band at 0 dB (OFF) reduced: %.2f, %.2f", a[1], d[0]);
        /* ratio per band: 10:1 on the low band, 1.5:1 on the high (24 dB over: 21.6 and 8) */
        k = kd(); k.lo = 100; k.lr = 7; steady_gr(k, 50.0f, -6.0f, a);
        k = kd(); k.hi = 100; k.hr = 0; steady_gr(k, 2000.0f, -6.0f, d);
        printf("LoRat 10:1 on 50 Hz: %.2f dB (21.6); HiRat 1.5:1 on 2 kHz: %.2f dB (8.0)\n", a[0], d[1]);
        CHECK(fabsf(a[0] - 21.6f) < 1.5f && fabsf(d[1] - 8.0f) < 1.0f, "ratios per band: %.2f, %.2f", a[0], d[1]);
        /* the in-between steps: 3:1 and 6:1 on the low band (24 dB over: 16.0 and 20.0) */
        k = kd(); k.lo = 100; k.lr = 3; steady_gr(k, 50.0f, -6.0f, a);
        k = kd(); k.lo = 100; k.lr = 5; steady_gr(k, 50.0f, -6.0f, b2);
        printf("LoRat 3:1 on 50 Hz: %.2f dB (16.0); 6:1: %.2f dB (20.0)\n", a[0], b2[0]);
        CHECK(fabsf(a[0] - 16.0f) < 1.5f && fabsf(b2[0] - 20.0f) < 1.5f, "3:1 / 6:1 steps: %.2f, %.2f", a[0], b2[0]);
    }

    /* ---- 3. the point of it: a loud low note pressed down, the harmonics over it keep their level ---- */
    {
        K k = kd(); float h_off, h_on, gr;
        sine(inL, SR * 2, 55.0f, 0.7f); add_sine(inL, SR * 2, 1650.0f, 0.05f); memcpy(inR, inL, sizeof(float) * SR * 2);
        run(SR * 2, k);
        h_off = amp_at(outL, SR, SR + 22050, 1650.0f);
        k.lo = 100; k.att = 2; k.rel = 3;
        run(SR * 2, k);
        h_on = amp_at(outL, SR, SR + 22050, 1650.0f); gr = (S.rS[0] + S.rF[0]) * 6.0206f;
        printf("55 Hz + 1650 Hz, LoThr -30: low band GR %.2f dB, harmonic moved %.2f dB\n", gr, db(h_on / h_off));
        CHECK(gr > 10.0f, "the low band did not press the fundamental: %.2f dB", gr);
        CHECK(fabsf(db(h_on / h_off)) < 0.5f, "the harmonic moved %.2f dB", db(h_on / h_off));
    }

    /* ---- 3b. a low E (41 Hz) held under heavy compression: the gain does not follow its cycles ---- */
    {
        const float f = (float)SR / 1072.0f;                 /* 41.13 Hz: whole periods in the window */
        K k = kd(); float f1, f3;
        k.lo = 100; k.lr = 7;                                 /* -30 dB, 10:1, default 10 ms / AUTO */
        sine(inL, SR * 2, f, 0.7f); memcpy(inR, inL, sizeof(float) * SR * 2);
        run(SR * 2, k);
        f1 = amp_at(outL, SR, SR + 1072 * 40, f); f3 = amp_at(outL, SR, SR + 1072 * 40, 3.0f * f);
        printf("41 Hz at -3 dB, LoThr -30 10:1: 3rd harmonic %.1f dB under the note\n", db(f1 / f3));
        CHECK(db(f1 / f3) > 60.0f, "gain ripple on a low note: 3rd harmonic only %.1f dB down", db(f1 / f3));
    }

    /* ---- 4. AUTO makeup is fixed: a -6 dBFS note comes out at its level, a held note does not swell,
     *         and a quiet note after a loud passage is not pushed above its own steady level ---- */
    {
        const float f = (float)SR / 882.0f;                  /* 50 Hz: whole periods in the window */
        const int on = SR, cut = SR * 4, n = SR * 8;
        K k = kd(); float g, early, late, quiet, worst = -100.0f;
        int j;
        k.lo = 100; k.lr = 4;                                 /* -30 dB, 4:1, default 10 ms / AUTO */
        sine(inL, SR * 2, f, 0.5f); memcpy(inR, inL, sizeof(float) * SR * 2);
        run(SR * 2, k);
        g = db(amp_at(outL, SR, SR + 882 * 20, f) / 0.5f);
        printf("AUTO makeup: a -6 dBFS 50 Hz note comes out at %+.2f dB\n", g);
        CHECK(fabsf(g) < 1.0f, "AUTO makeup off by %.2f dB at -6 dBFS", g);
        for (i = 0; i < n; i++) {
            float a = i < on ? 0.0f : i < cut ? 0.7f : 0.007f;
            inL[i] = inR[i] = a * sinf(6.2831853f * f * (float)i / (float)SR);
        }
        run(n, k);
        early = db(amp_at(outL, on + 882 * 25, on + 882 * 27, f) / 0.7f);         /* 0.5 s into the note */
        late = db(amp_at(outL, cut - 882 * 3, cut - 882, f) / 0.7f);              /* 3 s into the note */
        quiet = db(amp_at(outL, n - 882 * 3, n - 882, f) / 0.007f);                /* 4 s after it: steady */
        for (j = 0; j < 12; j++) {                                                 /* 0.25 .. 3 s after it */
            int a = cut + 882 * 12 + j * 882 * 12;
            float d = db(amp_at(outL, a, a + 882 * 2, f) / 0.007f);
            if (d > worst) worst = d;
        }
        printf("held loud note: %+.2f dB at 0.5 s, %+.2f at 3 s; quiet note after it: worst %+.2f dB, steady %+.2f\n",
               early, late, worst, quiet);
        CHECK(fabsf(late - early) < 1.0f, "a held note swells: %+.2f dB at 0.5 s, %+.2f at 3 s", early, late);
        CHECK(worst < quiet + 0.5f, "quiet note pushed up after a loud one: %+.2f dB vs %+.2f steady", worst, quiet);
    }

    /* ---- 4b. a loud note after silence: the makeup lifts its attack, the ceiling holds it at -1 dBFS ---- */
    {
        const int on = SR / 2;
        K k = kd(); float pk, settled;
        k.lo = 100; k.lr = 4;                                 /* -30 dB, 4:1, makeup +18 dB, attack 10 ms */
        for (i = 0; i < SR; i++) inL[i] = inR[i] = i < on ? 0.0f : 0.7f * sinf(6.2831853f * 50.0f * (float)(i - on) / (float)SR);
        run(SR, k);
        pk = db(peak(outL, on, on + SR / 20)); settled = db(peak(outL, SR - 4000, SR));
        printf("-3 dBFS note after silence: attack peak %+.2f dBFS, settled %+.2f dBFS\n", pk, settled);
        CHECK(pk < -0.99f, "attack over the -1 dBFS ceiling: %+.2f dBFS", pk);
        CHECK(settled < -3.0f, "the settled note stays at the ceiling: %+.2f dBFS", settled);
    }

    /* ---- 4c. Level is a plain output level: -12 dB on the output, the same gain reduction ---- */
    {
        K k = kd(); float a, b, gra, grb;
        k.lo = 100; k.lr = 4;                                 /* -30 dB, 4:1 */
        sine(inL, SR * 2, 50.0f, 0.25f); memcpy(inR, inL, sizeof(float) * SR * 2);
        run(SR * 2, k); a = peak(outL, SR, SR * 2); gra = (S.rS[0] + S.rF[0]) * 6.0206f;
        k.lv = 12;                                            /* -12 dB */
        run(SR * 2, k); b = peak(outL, SR, SR * 2); grb = (S.rS[0] + S.rF[0]) * 6.0206f;
        printf("Level -12: output %+.2f dB, gain reduction %.2f -> %.2f dB\n", db(b / a), gra, grb);
        CHECK(fabsf(db(b / a) + 12.0f) < 0.1f, "Level -12 moved the output %+.2f dB", db(b / a));
        CHECK(fabsf(gra - grb) < 0.01f, "Level changed the compression: %.2f vs %.2f dB", gra, grb);
    }

    /* ---- 5. Mix per band: 0 = the bands uncompressed, 50 = halfway in gain ---- */
    {
        K k = kd(); float g, o50, d = 0, ref[SR / 8];
        sine(inL, SR, 50.0f, 0.5f); memcpy(inR, inL, sizeof(float) * SR);
        run(SR, k); memcpy(ref, outL + SR - SR / 8, sizeof ref);
        k.lo = 100; k.att = 2; k.rel = 3; k.mix = 0; run(SR, k);
        for (i = 0; i < SR / 8; i++) d += fabsf(outL[SR - SR / 8 + i] - ref[i]);
        CHECK(d < 1e-3f, "Mix 0 differs from the uncompressed bands (%g)", d);
        k.mix = 100; run(SR / 2, k); g = peak(outL, SR / 2 - 4000, SR / 2) / 0.5f;
        k.mix = 50; run(SR / 2, k); o50 = peak(outL, SR / 2 - 4000, SR / 2) / 0.5f;
        printf("Mix 50 on 50 Hz: %.3f (0.5 + 0.5 g = %.3f)\n", o50, 0.5f + 0.5f * g);
        CHECK(fabsf(o50 - (0.5f + 0.5f * g)) < 0.02f, "Mix 50 %.3f vs %.3f", o50, 0.5f + 0.5f * g);
    }

    /* ---- 6. linked stereo: both sides get one gain, from the louder side (alone, the quiet side
     *         would sit under LoThr and get the whole makeup, about x7.9) ---- */
    {
        K k = kd(); float gl, gr;
        k.lo = 100; k.att = 2; k.rel = 3;
        sine(inL, SR, 50.0f, 0.25f); sine(inR, SR, 50.0f, 0.02f);
        run(SR, k);
        gl = peak(outL, SR - 4000, SR) / 0.25f; gr = peak(outR, SR - 4000, SR) / 0.02f;
        printf("linked: gain L %.4f, R %.4f\n", gl, gr);
        CHECK(fabsf(gl - gr) < 0.01f && gr < 3.0f, "L gain %.4f vs R gain %.4f", gl, gr);
    }

    /* ---- 7. no clicks, no NaN: fastest attack 10:1, and the crossover swept while playing ---- */
    {
        K k = kd(), k2; float mx = 0, gp = -1.0f, gs; int bad = 0;
        for (i = 0; i < SR * 2; i++) {
            float a = ((i / 6000) % 2) ? 0.9f : 0.02f;
            inL[i] = a * sinf(0.09f * (float)i); inR[i] = 0.5f * inL[i];
        }
        run(SR * 2, k); memcpy(refL, outL, sizeof(float) * SR * 2);     /* the bands uncompressed */
        k.lo = 0; k.hi = 0; k.lr = 7; k.hr = 7; k.att = 0; k.rel = 0;
        run(SR * 2, k);
        for (i = 100; i < SR * 2; i++) {             /* 632 Hz is all high band: out / ref is its gain */
            if (!(fabsf(outL[i]) < 4.0f) || !(fabsf(outR[i]) < 4.0f)) bad++;
            if (fabsf(outL[i]) > 0.8922f || fabsf(outR[i]) > 0.8922f) bad++;      /* the -1 dBFS ceiling */
            /* while the ceiling holds the attack of a loud note (here +30 dB of makeup against 0.1 ms
             * attack), it steps the gain at each new peak: skip the first 512 samples (12 ms) */
            if (fabsf(refL[i]) > 0.2f && i % 6000 >= 512) { gs = outL[i] / refL[i]; if (gp > 0.0f && fabsf(gs - gp) > mx) mx = fabsf(gs - gp); gp = gs; }
            else gp = -1.0f;
        }
        printf("fastest attack, 10:1: largest gain step per sample %.3f, bad samples %d\n", mx, bad);
        CHECK(bad == 0 && mx < 0.16f, "clicks, NaN or over the ceiling: step %.3f, %d bad", mx, bad);
        /* crossover 60 -> 400 Hz in the middle of a loud bass note with noise on top */
        for (i = 0; i < SR * 2; i++) { inL[i] = 0.8f * sinf(6.2831853f * 70.0f * i / SR) + 0.1f * rnd(); inR[i] = inL[i]; }
        k = kd(); k.lo = 200; k.hi = 250; k.xo = 0; k2 = k; k2.xo = 20;
        run_from(SR * 2, k, SR / 8, k2);
        bad = 0; mx = 0;
        for (i = 1; i < SR * 2; i++) {
            if (!(fabsf(outL[i]) < 2.0f)) bad++;
            if (fabsf(outL[i] - outL[i - 1]) > mx) mx = fabsf(outL[i] - outL[i - 1]);
        }
        printf("crossover swept while playing: largest sample step %.3f, bad samples %d\n", mx, bad);
        CHECK(bad == 0 && mx < 0.5f, "crossover jump: step %.3f, %d bad", mx, bad);
    }

    /* ---- labels ---- */
    {
        char t[16];
        ZDL_GetLabel_2(0, t);  CHECK(!strcmp(t, "60Hz"), "XOver 0 label %s", t);
        ZDL_GetLabel_2(9, t);  CHECK(!strcmp(t, "141Hz"), "XOver 9 label %s", t);
        ZDL_GetLabel_2(20, t); CHECK(!strcmp(t, "400Hz"), "XOver 20 label %s", t);
        ZDL_GetLabel_3(0, t);  CHECK(!strcmp(t, "-24dB"), "Level 0 label %s", t);
        ZDL_GetLabel_3(24, t); CHECK(!strcmp(t, "0dB"), "Level 24 label %s", t);
        ZDL_GetLabel_3(30, t); CHECK(!strcmp(t, "+6dB"), "Level 30 label %s", t);
        ZDL_GetLabel_0(400, t); CHECK(!strcmp(t, "OFF"), "LoThr 400 label %s", t);
        ZDL_GetLabel_0(399, t); CHECK(!strcmp(t, "-0.1"), "LoThr 399 label %s", t);
        ZDL_GetLabel_1(225, t); CHECK(!strcmp(t, "-17.5"), "HiThr 225 label %s", t);
        ZDL_GetLabel_4(0, t);  CHECK(!strcmp(t, "1.5:1"), "LoRat 0 label %s", t);
        ZDL_GetLabel_4(2, t);  CHECK(!strcmp(t, "2.5:1"), "LoRat 2 label %s", t);
        ZDL_GetLabel_4(3, t);  CHECK(!strcmp(t, "3:1"), "LoRat 3 label %s", t);
        ZDL_GetLabel_4(5, t);  CHECK(!strcmp(t, "6:1"), "LoRat 5 label %s", t);
        ZDL_GetLabel_5(7, t);  CHECK(!strcmp(t, "10:1"), "HiRat 7 label %s", t);
        ZDL_GetLabel_6(5, t);  CHECK(!strcmp(t, "30ms"), "Attck 5 label %s", t);
        ZDL_GetLabel_7(4, t);  CHECK(!strcmp(t, "AUTO"), "Rels 4 label %s", t);
    }

    printf(fails ? "\n%d FAILED\n" : "\nall ok\n", fails);
    return fails != 0;
}
