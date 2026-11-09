/*
 * gooeymb.c - "GooeyMB": Gooey's compressor split in two bands (low / high), linked stereo
 *
 * Made for bass: the lows are held on their own, so a loud low string does not pull down the
 * attack and the upper harmonics. Each band is Gooey's detector, gain computer and ballistics.
 *
 * KNOBS (screen values)
 *   0 LoThr 0..400  low band threshold, -40 .. 0 dB in 0.1 dB steps (400 = 0 dB = OFF: that band is not compressed)
 *   1 HiThr 0..400  high band threshold, same scale (default OFF)
 *   2 XOver 0..20   crossover 60 .. 400 Hz on a log scale (default 9 = 141 Hz)
 *   3 Zoom  0..15   meter only: x0.5 .. x8.0 rows a dB around the LoThr line
 *   4 LoRat 0..3    low band ratio 1.5 / 2 / 4 / 10 (default 2 = 4:1)
 *   5 HiRat 0..3    high band ratio (default 1 = 2:1)
 *   6 Attck 0..5    0.1 / 0.3 / 1 / 3 / 10 / 30 ms, both bands (default 4 = 10 ms)
 *   7 Rels  0..4    0.1 / 0.3 / 0.6 / 1.2 s / AUTO, both bands (default 4 = AUTO)
 *   8 Mix   0..100  linear dry/wet inside each band (parallel compression), default 100
 *
 * HOW IT WORKS (everything but the sample loops runs once per 8-sample block)
 *   Crossover: Linkwitz-Riley 24 dB/oct from three TPT state-variable filters per side (one
 *   Butterworth split, then a second low-pass on the low and a second high-pass on the high).
 *   The two bands sum back to an all-pass: flat level, a phase turn around the crossover. It
 *   always runs, so the defaults (both bands OFF) are flat in level, not bit-exact.
 *   Per band, as Gooey: linked stereo detector (half peak, half RMS), soft knee 6 dB, slope
 *   1 - 1/ratio, attack / release one-poles in the log domain, AUTO release with a slow stage.
 *   The low band's detector is slower (peak fall 150 ms, RMS 20 ms instead of 50 / 5) so the
 *   gain does not follow the cycles of a low note (that would distort it).
 *   Makeup is a fixed AUTO per band, from its threshold and ratio: the reduction a -6 dBFS level
 *   would get. No time behaviour, so a held note keeps its reduction and quiet notes after a loud
 *   passage are not pushed up; the tone balance stays where it was. The makeup also lifts a
 *   note's attack before the detector catches it, so the output has a -1 dBFS ceiling (instant
 *   attack, 50 ms release) that only acts on those attacks.
 *   Mix is applied per band (dry + wet x gain), so the dry part is the same summed bands and
 *   Mix never combs against the crossover's phase.
 *
 * METER: Gooey's live cover (src/custom/common/zdl_meter.h): the output level, the line at
 * LoThr (tag "LO"), the HiThr value in a second tag at the bottom. The tag font has no O: the
 * zero glyph draws it.
 *
 * Pedal-safe rules (docs/SAFE-DSP-RULES.md): no static arrays in .audio, no divide, no libm,
 * no switch, no double, helpers forced inline, state in the arena with a magic.
 */

#include <stdint.h>

#ifdef __TI_COMPILER_VERSION__
#define GM_DO_PRAGMA(x) _Pragma(#x)
#define GM_EXPAND_PRAGMA(x) GM_DO_PRAGMA(x)
#define GM_ALWAYS_INLINE(fn) GM_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(fn))
#define GM_CODE_SECTION(fn) GM_EXPAND_PRAGMA(CODE_SECTION(fn, ".audio"))
#define GM_BLOCK_LOOP() GM_DO_PRAGMA(MUST_ITERATE(8, 8, 8)) GM_DO_PRAGMA(UNROLL(1))
#else
#define GM_ALWAYS_INLINE(fn)
#define GM_CODE_SECTION(fn)
#define GM_BLOCK_LOOP()
#endif

#define GM_MAGIC     0x474D4232u            /* "GMB2": change when GmState changes */
#define GM_KEY_NONE  0xFFFFFFFFu
#define GM_DPK       0.99638f               /* high band peak follower fall per block: 50 ms */
#define GM_CMS       0.03563f               /* high band mean-square follower per block: 5 ms */
#define GM_DPK_LO    0.998791f              /* low band: 150 ms, so a low note does not ripple the gain */
#define GM_CMS_LO    0.009029f              /* low band: 20 ms                              */
#define GM_MK_REF    -0.99657840f           /* AUTO makeup reference level: -6 dBFS in log2 units */
#define GM_CEIL      0.89125094f            /* output ceiling: -1 dBFS                      */
#define GM_LREL      4.5341e-4f             /* ceiling release per sample: 50 ms            */
#define GM_CSLOW     0.00015117f            /* AUTO slow stage per block: 1.2 s            */
#define GM_KNEE_H    0.49829f               /* half the 6 dB knee, in log2 units (1 = 6.02 dB) */
#define GM_KNEE_K    0.50172f               /* 1 / (2 x knee width)                        */
#define GM_DB2LOG    0.16609640f            /* dB -> log2 units                            */
#define GM_SQRT2     1.41421356f            /* Butterworth damping                         */

/* ---- meter: the cover (zdl_meter.h) --------------------------------------------------- */
#ifndef GOOEYMB_HOST_TEST
#ifndef GOOEYMB_AUDIO_FUNC
#define GOOEYMB_AUDIO_FUNC Fx_DLY_GooeyMB
#endif
#define GM_PIC_X(f) _##f##_Pic
#define GM_PIC_(f) GM_PIC_X(f)
extern unsigned char GM_PIC_(GOOEYMB_AUDIO_FUNC)[];     /* the cover data; the linker resolves it */
#define ZM_PIC GM_PIC_(GOOEYMB_AUDIO_FUNC)
#endif
#include "gooeymb_layout.h"                /* ZM_TOP / ZM_CAP, from build.py */
#include "../common/zdl_meter.h"
#include "../common/zarena.h"

typedef struct {
    unsigned int magic;
    unsigned int was_off;
    unsigned int k_att, k_rel, k_xo;        /* knob values the cached numbers are for */
    float cA, cR, a1, a2, a3;               /* ballistics; crossover filter coefficients */
    float pkenv[2], ms[2], rS[2], rF[2], gm[2];   /* per band: 0 low, 1 high */
    float lg;                               /* output ceiling gain */
    float xf[12];                           /* crossover states: per side split, low, high (2 each) */
    ZmState zm;                             /* the cover meter */
    unsigned int seal[ZA_NSEAL(240)];       /* every word's bits 30, 29 between blocks (zarena.h) */
} GmState;
ZA_CHECK(GmState);
/* sealed words: all before zm.lv[]; the meter history is under 0x2000 (ZM_NOLV), so already safe */
#define GM_HDR ((int)(offsetof(GmState, zm.lv) >> 2))

typedef struct {
    float t2[2], slope[2], mk[2], dryG, wetG, cA, cR, a1, a2, a3;
    unsigned int off[2], autor;
} GmParams;

/* knobs arrive as (screen value) / 100; convert back like the rest of the pack */
GM_ALWAYS_INLINE(gm_ui)
static inline float gm_ui(float raw, float def_ui, float max_ui)
{
    float ui;
    if (!(raw >= 0.0f && raw <= 300.0f)) ui = def_ui;
    else if (raw <= 3.05f) ui = raw * 100.0f;
    else ui = raw;
    ui = (float)(int)(ui + 0.5f);
    if (ui > max_ui) ui = max_ui;
    if (ui < 0.0f) ui = 0.0f;
    return ui;
}

/* thresholds (0..400): the pedal passes screen / 100 (0..4.00); always scale by 100 */
GM_ALWAYS_INLINE(gm_ui_thr)
static inline float gm_ui_thr(float raw, float def_ui)
{
    float ui;
    if (!(raw >= 0.0f && raw <= 4.005f)) ui = def_ui;
    else ui = (float)(int)(raw * 100.0f + 0.5f);
    if (ui > 400.0f) ui = 400.0f;
    return ui;
}

/* 2^x for x in -20..20 (error about 2e-6) */
GM_ALWAYS_INLINE(gm_exp2)
static inline float gm_exp2(float x)
{
    union { float f; unsigned int u; } c;
    int n;
    float f, e, r;
    if (x > 20.0f) x = 20.0f;
    if (x < -20.0f) x = -20.0f;
    n = (int)(x + 32.5f) - 32;
    f = x - (float)n;
    e = f * 0.6931472f;
    r = 1.0f + e * (1.0f + e * (0.5f + e * (0.16666667f + e * (0.041666668f + e * 0.008333334f))));
    c.u = (unsigned int)(n + 127) << 23;
    return r * c.f;
}

/* log2 x for x > 0 normal (error < 2e-4) */
GM_ALWAYS_INLINE(gm_log2)
static inline float gm_log2(float x)
{
    union { float f; unsigned int u; } c;
    float m;
    int e;
    c.f = x;
    e = (int)(c.u >> 23) - 127;
    c.u = (c.u & 0x007FFFFFu) | 0x3F800000u;
    m = c.f;
    return (float)e + 1.442695f * (-1.7417939f + (2.8212026f + (-1.4699568f + (0.44717955f - 0.056570851f * m) * m) * m) * m);
}

/* sqrt x for x >= 0: inverse square root by the exponent trick and three Newton steps */
GM_ALWAYS_INLINE(gm_sqrt)
static inline float gm_sqrt(float x)
{
    union { float f; unsigned int u; } c;
    float y;
    if (x < 1e-20f) return 0.0f;
    c.f = x;
    c.u = 0x5f3759dfu - (c.u >> 1);
    y = c.f;
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y;
}

/* crossover in Hz of knob 0..20: 60 .. 400 Hz, log */
GM_ALWAYS_INLINE(gm_xo_hz)
static inline float gm_xo_hz(unsigned int k)
{
    return 60.0f * gm_exp2((float)(int)k * 0.13684828f);
}

GM_ALWAYS_INLINE(gm_reset)
static inline void gm_reset(GmState *s)
{
    int i;
    for (i = 0; i < 2; i++) { s->pkenv[i] = 0.0f; s->ms[i] = 0.0f; s->rS[i] = 0.0f; s->rF[i] = 0.0f; s->gm[i] = 1.0f; }
    s->lg = 1.0f;
    for (i = 0; i < 12; i++) s->xf[i] = 0.0f;
}

GM_ALWAYS_INLINE(gm_init)
static inline void gm_init(GmState *s)
{
    gm_reset(s);
    s->k_att = GM_KEY_NONE; s->k_rel = GM_KEY_NONE; s->k_xo = GM_KEY_NONE;
    s->cA = 0.0f; s->cR = 0.0f; s->a1 = 1.0f; s->a2 = 0.0f; s->a3 = 0.0f; s->was_off = 0u;
    zm_init(&s->zm);
    ZA_ZERO_TAIL(s, GmState);
    s->magic = GM_MAGIC;
}

/* 1 / ms of the attack and release choices (no tables in .audio) */
GM_ALWAYS_INLINE(gm_att_inv)
static inline float gm_att_inv(unsigned int i)
{
    return (i == 0u) ? 10.0f : (i == 1u) ? 3.3333333f : (i == 2u) ? 1.0f : (i == 3u) ? 0.33333333f : (i == 4u) ? 0.1f : 0.033333333f;
}
GM_ALWAYS_INLINE(gm_rel_inv)
static inline float gm_rel_inv(unsigned int i)
{
    return (i == 0u) ? 0.01f : (i == 1u) ? 0.0033333333f : (i == 2u) ? 0.0016666667f : (i == 3u) ? 0.00083333333f : 0.01f;
}

GM_ALWAYS_INLINE(gm_slope)
static inline float gm_slope(unsigned int r)
{
    return (r == 0u) ? 0.33333333f : (r == 1u) ? 0.5f : (r == 2u) ? 0.75f : 0.9f;
}

/* u[] = screen values in manifest order: LoThr, HiThr, XOver, Zoom, LoRat, HiRat, Attck, Rels, Mix.
 * The cached numbers are only recomputed when their knob moved. */
GM_ALWAYS_INLINE(gm_prepare)
static inline void gm_prepare(GmParams *P, GmState *s, const float *u)
{
    unsigned int kx = (unsigned int)(int)u[2], ka = (unsigned int)(int)u[6], kr = (unsigned int)(int)u[7];
    float m = u[8] * 0.01f;
    int b;
    if (ka != s->k_att) { s->k_att = ka; s->cA = 1.0f - gm_exp2(-0.26171f * gm_att_inv(ka)); }
    if (kr != s->k_rel) { s->k_rel = kr; s->cR = 1.0f - gm_exp2(-0.26171f * gm_rel_inv(kr)); }
    if (kx != s->k_xo) {                        /* g = tan(pi f / 44100), a1 = 1 / (1 + g (g + k)) */
        float w = gm_xo_hz(kx) * 7.1237928e-5f, g = w + w * w * w * 0.33333333f;
        float d = 1.0f + g * (g + GM_SQRT2), y = 1.0f;
        y = y * (2.0f - d * y); y = y * (2.0f - d * y); y = y * (2.0f - d * y);
        s->k_xo = kx; s->a1 = y; s->a2 = g * y; s->a3 = g * g * y;
    }
    for (b = 0; b < 2; b++) {
        unsigned int thr = (unsigned int)(int)u[b];
        float over;
        P->off[b] = (thr >= 400u);
        P->t2[b] = ((float)(int)thr - 400.0f) * (0.1f * GM_DB2LOG);
        P->slope[b] = gm_slope((unsigned int)(int)u[4 + b]);
        over = GM_MK_REF - P->t2[b];
        P->mk[b] = (P->off[b] || over <= 0.0f) ? 0.0f : P->slope[b] * over;
    }
    P->dryG = 1.0f - m;
    P->wetG = m;
    P->cA = s->cA; P->cR = s->cR;
    P->a1 = s->a1; P->a2 = s->a2; P->a3 = s->a3;
    P->autor = (kr >= 4u);
}

/* ---- meter ------------------------------------------------------------------------- */
GM_ALWAYS_INLINE(gm_meter_level)
static inline void gm_meter_level(GmState *s, const float *buf)
{
    int i;
    float m = s->zm.lvl;
    GM_BLOCK_LOOP()
    for (i = 0; i < 8; i++) {
        float p = buf[i], q = buf[i + 8];
        p = (p < 0.0f) ? -p : p;
        q = (q < 0.0f) ? -q : q;
        p = (q > p) ? q : p;
        m = (p > m) ? p : m;
    }
    s->zm.lvl = m;
}

/* "XX -12.3" (or "XX 0.0" when OFF) into t; returns the length */
GM_ALWAYS_INLINE(gm_tag_text)
static inline unsigned int gm_tag_text(char *t, char a, char b, unsigned int thr)
{
    t[0] = a; t[1] = b; t[2] = ' '; t[3] = '-';
    return zm_tenths(400u - thr, t, thr < 400u ? 4u : 3u);
}

/* once per on-screen block: history column; line, zoom and tags for LoThr/HiThr/Zoom; frame */
GM_ALWAYS_INLINE(gm_meter)
static inline void gm_meter(GmState *s, const float *u)
{
    ZmState *z = &s->zm;
    unsigned int lo = (unsigned int)(int)u[0], hi = (unsigned int)(int)u[1], zo = (unsigned int)(int)u[3];
    unsigned int key = lo | (hi << 9) | (zo << 18);
    if (zm_column(z)) zm_push(z);
    if (key != z->key && z->pos >= ZM_RAW) {
        char t[12];
        zm_scale(z, ((float)(int)lo - 400.0f) * 0.1f, (float)(int)(zo + 1u) * 0.5f);
        z->key = key; z->dirty = 1u;
        zm_tag_set(&z->tag[0], t, gm_tag_text(t, 'L', '0', lo), 126u, ZM_MID - 2u);
        zm_tag_set(&z->tag[1], t, gm_tag_text(t, 'H', 'I', hi), 126u, 62u);
    }
    zm_frame(z);
}

/* ---- the compressor: one 8-sample block, L = buf[0..7], R = buf[8..15], in place ------- */
GM_ALWAYS_INLINE(gm_process)
static inline void gm_process(GmState *s, const GmParams *P, float *restrict buf)
{
    float lo[16], hi[16], gt[2], g0, g1, s0, s1, lg;
    int i, c, b;

    /* 1. split each side: LR4 low = LP(LP(x)), high = HP(HP(x)), three TPT SVFs (Simper) */
    for (c = 0; c < 2; c++) {
        const float a1 = P->a1, a2 = P->a2, a3 = P->a3;
        float *f = s->xf + 6 * c, *x = buf + 8 * c, *l = lo + 8 * c, *h = hi + 8 * c;
        float p1 = f[0], p2 = f[1], q1 = f[2], q2 = f[3], r1 = f[4], r2 = f[5];
        GM_BLOCK_LOOP()
        for (i = 0; i < 8; i++) {
            float v0 = x[i], v1, v2, v3, lp, hp;
            v3 = v0 - p2; v1 = a1 * p1 + a2 * v3; v2 = p2 + a2 * p1 + a3 * v3;
            p1 = 2.0f * v1 - p1; p2 = 2.0f * v2 - p2;
            lp = v2; hp = v0 - GM_SQRT2 * v1 - v2;
            v3 = lp - q2; v1 = a1 * q1 + a2 * v3; v2 = q2 + a2 * q1 + a3 * v3;
            q1 = 2.0f * v1 - q1; q2 = 2.0f * v2 - q2;
            l[i] = v2;
            v3 = hp - r2; v1 = a1 * r1 + a2 * v3; v2 = r2 + a2 * r1 + a3 * v3;
            r1 = 2.0f * v1 - r1; r2 = 2.0f * v2 - r2;
            h[i] = hp - GM_SQRT2 * v1 - v2;
        }
        f[0] = p1; f[1] = p2; f[2] = q1; f[3] = q2; f[4] = r1; f[5] = r2;
    }

    /* 2. per band: detector, gain computer, ballistics, makeup -> the band's gain for the block */
    for (b = 0; b < 2; b++) {
        const float *v = b ? hi : lo;
        float pk = 0.0f, sq = 0.0f, dpk, lvl, o, gr, rS, rF, need, tot;
        GM_BLOCK_LOOP()
        for (i = 0; i < 8; i++) {
            float p = v[i], q = v[i + 8];
            p = (p < 0.0f) ? -p : p;
            q = (q < 0.0f) ? -q : q;
            p = (q > p) ? q : p;
            pk = (p > pk) ? p : pk;
            sq += p * p;
        }
        dpk = b ? GM_DPK : GM_DPK_LO;
        s->pkenv[b] = (pk > s->pkenv[b] * dpk) ? pk : s->pkenv[b] * dpk;
        s->ms[b] += (b ? GM_CMS : GM_CMS_LO) * (sq * 0.125f - s->ms[b]);
        lvl = 0.5f * (s->pkenv[b] + gm_sqrt(2.0f * s->ms[b])) + 1e-6f;
        o = gm_log2(lvl) - P->t2[b];
        if (P->off[b] || o <= -GM_KNEE_H) gr = 0.0f;
        else if (o >= GM_KNEE_H) gr = P->slope[b] * o;
        else { float t = o + GM_KNEE_H; gr = P->slope[b] * t * t * GM_KNEE_K; }
        rS = s->rS[b]; rF = s->rF[b];
        if (P->autor) { rS += GM_CSLOW * (gr - rS); need = gr - rS; need = (need > 0.0f) ? need : 0.0f; }
        else { rS = 0.0f; need = gr; }
        rF += ((need > rF) ? P->cA : P->cR) * (need - rF);
        if (rF < 1e-7f) rF = 0.0f;
        if (rS < 1e-7f) rS = 0.0f;
        s->rS[b] = rS; s->rF[b] = rF;
        tot = rS + rF;
        gt[b] = P->dryG + P->wetG * gm_exp2(P->mk[b] - tot);
    }

    /* 3. sum the bands, each gain moved in a line over the block (no zipper); the makeup lifts a note's
     *    attack before the detector catches it, so a ceiling with instant attack holds it at -1 dBFS */
    g0 = s->gm[0]; s0 = (gt[0] - g0) * 0.125f; s->gm[0] = gt[0];
    g1 = s->gm[1]; s1 = (gt[1] - g1) * 0.125f; s->gm[1] = gt[1];
    lg = s->lg;
    GM_BLOCK_LOOP()
    for (i = 0; i < 8; i++) {
        float yl, yr, p, q;
        g0 += s0; g1 += s1;
        yl = g0 * lo[i] + g1 * hi[i];
        yr = g0 * lo[i + 8] + g1 * hi[i + 8];
        p = (yl < 0.0f) ? -yl : yl;
        q = (yr < 0.0f) ? -yr : yr;
        p = (q > p) ? q : p;
        lg += GM_LREL * (1.0f - lg);
        if (p * lg > GM_CEIL) lg = GM_CEIL * gm_exp2(-gm_log2(p));
        buf[i] = yl * lg;
        buf[i + 8] = yr * lg;
    }
    s->lg = lg;

    if (s->zm.on) gm_meter_level(s, buf);
}

/* ---- on-screen text ------------------------------------------------------ */
/* tenths as "12.3": whole part, '.', one decimal; the caller puts a sign in front */
GM_ALWAYS_INLINE(gm_tenths_text)
static inline int gm_tenths_text(unsigned int v, char *out, int len)
{
    int t = 0, n = (int)v;
    while (n >= 100) { n -= 100; t++; }
    if (t > 0) { out[len] = (char)('0' + t); len++; }
    t = 0;
    while (n >= 10) { n -= 10; t++; }
    out[len] = (char)('0' + t); len++;
    out[len] = '.'; len++;
    out[len] = (char)('0' + n); len++;
    out[len] = 0;
    return len;
}

/* whole number 0..999 as text */
GM_ALWAYS_INLINE(gm_int_text)
static inline int gm_int_text(int n, char *out, int len)
{
    int h = 0, t = 0;
    while (n >= 100) { n -= 100; h++; }
    while (n >= 10) { n -= 10; t++; }
    if (h > 0) { out[len] = (char)('0' + h); len++; }
    if (h > 0 || t > 0) { out[len] = (char)('0' + t); len++; }
    out[len] = (char)('0' + n); len++;
    out[len] = 0;
    return len;
}

/* thresholds: 0..400 -> "-40.0".."0" (dB, 0.1 a step) */
GM_ALWAYS_INLINE(gm_thr_label)
static inline int gm_thr_label(unsigned int value, char *out)
{
    if (value > 400u) value = 400u;
    if (value == 400u) { out[0] = '0'; out[1] = 0; return 1; }
    out[0] = '-';
    return gm_tenths_text(400u - value, out, 1);
}

/* ratios: 1.5:1 / 2:1 / 4:1 / 10:1 */
GM_ALWAYS_INLINE(gm_ratio_label)
static inline int gm_ratio_label(unsigned int value, char *out)
{
    int n = 0;
    if (value == 0u) { out[n++] = '1'; out[n++] = '.'; out[n++] = '5'; }
    else { n = gm_int_text((value == 1u) ? 2 : (value == 2u) ? 4 : 10, out, 0); }
    out[n++] = ':'; out[n++] = '1'; out[n] = 0;
    return n;
}

int ZDL_GetLabel_0(unsigned int value, char *out) { return gm_thr_label(value, out); }
int ZDL_GetLabel_1(unsigned int value, char *out) { return gm_thr_label(value, out); }

/* knob 2 XOver: 60 .. 400 Hz */
int ZDL_GetLabel_2(unsigned int value, char *out)
{
    int n;
    if (value > 20u) value = 20u;
    n = gm_int_text((int)(gm_xo_hz(value) + 0.5f), out, 0);
    out[n++] = 'H'; out[n++] = 'z'; out[n] = 0;
    return n;
}

/* knob 3 Zoom: x0.5 .. x8.0 (meter rows a dB) */
int ZDL_GetLabel_3(unsigned int value, char *out)
{
    if (value > 15u) value = 15u;
    out[0] = 'x';
    return gm_tenths_text((value + 1u) * 5u, out, 1);
}

int ZDL_GetLabel_4(unsigned int value, char *out) { return gm_ratio_label(value, out); }
int ZDL_GetLabel_5(unsigned int value, char *out) { return gm_ratio_label(value, out); }

/* knob 6 Attck: 0.1 / 0.3 / 1 / 3 / 10 / 30 ms */
int ZDL_GetLabel_6(unsigned int value, char *out)
{
    int n;
    if (value == 0u || value == 1u) { out[0] = '0'; out[1] = '.'; out[2] = (char)('1' + 2 * (int)value); n = 3; }
    else n = gm_int_text((value == 2u) ? 1 : (value == 3u) ? 3 : (value == 4u) ? 10 : 30, out, 0);
    out[n++] = 'm'; out[n++] = 's'; out[n] = 0;
    return n;
}

/* knob 7 Rels: 0.1s / 0.3s / 0.6s / 1.2s / AUTO */
int ZDL_GetLabel_7(unsigned int value, char *out)
{
    if (value >= 4u) { out[0] = 'A'; out[1] = 'U'; out[2] = 'T'; out[3] = 'O'; out[4] = 0; return 4; }
    out[0] = (value == 3u) ? '1' : '0'; out[1] = '.';
    out[2] = (value == 0u) ? '1' : (value == 1u) ? '3' : (value == 2u) ? '6' : '2';
    out[3] = 's'; out[4] = 0;
    return 4;
}

/* ---- pedal entry point ---------------------------------------------------- */
#ifndef GOOEYMB_HOST_TEST

#include "gooeymb_params.h"

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

GM_CODE_SECTION(GOOEYMB_AUDIO_FUNC)
void GOOEYMB_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *,
                                     *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    volatile unsigned int *desc;
    uintptr_t base, end, stateBase;
    unsigned int span;
    GmState *s;
    GmParams P;
    float u[9];
    unsigned int on;

    *magicDst = *magicSrc;                       /* preserve the magic shuttle */

    desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;

    base = (uintptr_t)desc[0];
    end  = (uintptr_t)desc[1];
    span = desc[2];
    stateBase = (base + 3u) & ~(uintptr_t)3u;

    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u || (end & 3u) != 0u || (span & 3u) != 0u) return;
    if ((end - base) < sizeof(GmState) || span < (end - base)) return;
    if (stateBase + sizeof(GmState) > end) return;

    s = (GmState *)stateBase;

    za_unseal(s, GM_HDR, s->seal);
    if (params[0] < 0.5f) {                      /* effect switched off: input untouched */
        if (s->magic == GM_MAGIC) s->was_off = 1u;
        goto done;
    }
    if (s->magic != GM_MAGIC) gm_init(s);
    if (s->was_off) { gm_reset(s); s->was_off = 0u; }

    u[0] = gm_ui_thr(params[GOOEYMB_LOTHR_SLOT], (float)GOOEYMB_LOTHR_UI_DEFAULT);
    u[1] = gm_ui_thr(params[GOOEYMB_HITHR_SLOT], (float)GOOEYMB_HITHR_UI_DEFAULT);
    u[2] = gm_ui(params[GOOEYMB_XOVER_SLOT], (float)GOOEYMB_XOVER_UI_DEFAULT, 20.0f);
    u[3] = gm_ui(params[GOOEYMB_ZOOM_SLOT],  (float)GOOEYMB_ZOOM_UI_DEFAULT,  15.0f);
    u[4] = gm_ui(params[GOOEYMB_LORAT_SLOT], (float)GOOEYMB_LORAT_UI_DEFAULT, 3.0f);
    u[5] = gm_ui(params[GOOEYMB_HIRAT_SLOT], (float)GOOEYMB_HIRAT_UI_DEFAULT, 3.0f);
    u[6] = gm_ui(params[GOOEYMB_ATTCK_SLOT], (float)GOOEYMB_ATTCK_UI_DEFAULT, 5.0f);
    u[7] = gm_ui(params[GOOEYMB_RELS_SLOT],  (float)GOOEYMB_RELS_UI_DEFAULT,  4.0f);
    u[8] = gm_ui(params[GOOEYMB_MIX_SLOT],   (float)GOOEYMB_MIX_UI_DEFAULT,   100.0f);

    gm_prepare(&P, s, u);
    on = zm_begin(&s->zm, params);
    gm_process(s, &P, fxBuf);
    if (on) gm_meter(s, u);
done:
    za_seal(s, GM_HDR, s->seal);        /* leave the arena safe (MIDISTOMP #60) */
}

#endif /* GOOEYMB_HOST_TEST */
