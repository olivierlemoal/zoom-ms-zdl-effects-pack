/* GooeyMB meter on the cover (src/custom/common/zdl_meter.h): every frame equals the reference render
 * (line at LoThr with its "LO" tag, HiThr in the bottom tag), stays within ZM_CAP, flips A/B, at most
 * 15 redraws a second, nothing on stock firmware or off screen, the level is the low band before its gain. */
#define GOOEYMB_HOST_TEST
#define EFF_NK 9
#define ZM_EFFECT_C "../src/custom/gooeymb/gooeymb.c"
#include "zm_test.h"

static const char *EFF_NAME = "GooeyMB";
static const char *EFF_PREVIEW_DIR = "tests/_gen";

static GmState S;
static void eff_init(void) { memset(&S, 0x5A, sizeof S); gm_init(&S); }
static ZmState *eff_zm(void) { return &S.zm; }

static float last_out;                               /* loudest output sample of the last block */
static float tone = 0.007f;                          /* radians a sample: 0.007 = 49 Hz (low band), 0.05 = 351 Hz (high) */
static void eff_block(const float *u, float amp, int b)
{
    float buf[16];
    GmParams P;
    unsigned int on;
    int i;
    for (i = 0; i < 8; i++) {
        float t = (float)(b * 8 + i), a = amp;
        if (amp < 0.0f) { float ph = fmodf(t, 22050.0f); a = -amp * expf(-ph / 3000.0f); }
        buf[i] = a * sinf(tone * t); buf[i + 8] = buf[i];
    }
    gm_prepare(&P, &S, u);
    on = zm_begin(&S.zm, (void *)&S);
    gm_process(&S, &P, buf);
    if (on) gm_meter(&S, u);
    last_out = 0.0f;
    for (i = 0; i < 16; i++) last_out = fabsf(buf[i]) > last_out ? fabsf(buf[i]) : last_out;
}

/* custom_covers.meter_geometry("gooeymb"); the bottom tag is the gain reduction readout while HiThr is OFF */
static void eff_geom(const float *u, int *by, RefTag *tags, int *nt)
{
    *by = (int)ZM_MID;
    snprintf(tags[0].text, sizeof tags[0].text, "L0 %.1f", (u[0] - 400) / 10.0);
    tags[0].xr = 126; tags[0].yb = *by - 2;
    if (u[1] >= 400.0f) snprintf(tags[1].text, sizeof tags[1].text, "GR %.1f", -(double)((S.zm.key >> 18) & 511u) / 10.0 + 0.0);
    else snprintf(tags[1].text, sizeof tags[1].text, "HI %.1f", (u[1] - 400) / 10.0);
    tags[1].xr = 126; tags[1].yb = 62; *nt = 2;
}
static void eff_randk(float *u) { u[0] = (float)(rand() % 401); u[1] = (float)(rand() % 401); u[3] = (float)(rand() % 31); }
static void eff_line(const float *u, double *line, double *rpd) { *line = (u[0] - 400.0) / 10.0; *rpd = 1.0; }

int main(void)
{
    static const float udef[EFF_NK] = { 400.0f, 400.0f, 9.0f, 24.0f, 4.0f, 1.0f, 4.0f, 4.0f, 100.0f };
    float u[EFF_NK], pk, shown;
    int g, b;

    zmt_run(udef);

    /* the shown level is the low band before its gain: the same at any LoThr and makeup, and at Mix 0 */
    for (g = 0; g < 3; g++) {
        zmt_reset(1u, 1u);
        memcpy(u, udef, sizeof u); u[0] = g == 1 ? 0.0f : 200.0f; u[1] = 200.0f;
        if (g == 2) u[8] = 0.0f;
        tone = 0.007f;
        for (b = 0; b < 5512 + 2000; b++) eff_block(u, 0.1f, b);          /* -20 dBFS: the output is ~10 dB louder */
        shown = ZM_DB0 - (float)S.zm.lv[(S.zm.head - 1u) & 255u] / 16.0f;
        CHECK(fabsf(shown + 20.0f) < 0.5f, "%s: 49 Hz at -20 dBFS shown at %.2f dB",
              g == 0 ? "LoThr -20" : g == 1 ? "LoThr -40" : "Mix 0", shown);
    }

    /* the bottom tag reads the low band's gain reduction while HiThr is OFF: 49 Hz at -20 dBFS,
     * LoThr -40 4:1 holds it about 15 dB down; the readout follows the gain computer */
    {
        float gr;
        zmt_reset(1u, 1u);
        memcpy(u, udef, sizeof u); u[0] = 0.0f; u[1] = 400.0f;
        tone = 0.007f;
        for (b = 0; b < 5512 * 2; b++) eff_block(u, 0.1f, b);
        gr = (S.rS[0] + S.rF[0]) * 6.0206f;
        printf("GR readout %u tenths, gain computer %.2f dB\n", S.grshow, gr);
        CHECK(S.grshow > 120u && fabsf((float)S.grshow * 0.1f - gr) < 0.5f, "GR readout %u tenths vs %.2f dB", S.grshow, gr);
        /* the tag takes a new readout at the next frame boundary: it may lag one readout */
        CHECK(abs((int)((S.zm.key >> 18) & 511u) - (int)S.grshow) <= 3, "GR tag not updated: key %u, readout %u", (S.zm.key >> 18) & 511u, S.grshow);
    }

    /* a high note shows far under its output: the high band is not on this meter */
    {
        zmt_reset(1u, 1u);
        memcpy(u, udef, sizeof u); u[0] = 200.0f;
        tone = 0.05f;
        pk = 0.0f;
        for (b = 0; b < 5512 + 2000; b++) { eff_block(u, 0.5f, b); if (b >= 5512) pk = last_out > pk ? last_out : pk; }
        shown = ZM_DB0 - (float)S.zm.lv[(S.zm.head - 1u) & 255u] / 16.0f;
        printf("351 Hz: output %.1f dB, meter %.1f dB\n", 20.0 * log10(pk), shown);
        CHECK(shown < 20.0f * log10f(pk) - 20.0f, "351 Hz: meter %.1f dB, output %.1f dB", shown, 20.0 * log10(pk));
        tone = 0.007f;
    }

    {
        zmt_reset(1u, 1u);
        memcpy(u, udef, sizeof u); u[0] = 200.0f; u[1] = 280.0f;
        run_blocks(u, 5512 * 9, -0.9f);
        CHECK(frame_matches(u, 0), "preview frame differs");
        preview("gooeymb-meter.png");
    }
    printf(fails ? "\n%d FAILED\n" : "\nall ok\n", fails);
    return fails != 0;
}
