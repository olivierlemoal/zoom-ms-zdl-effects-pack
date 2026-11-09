/* GooeyMB meter on the cover (src/custom/common/zdl_meter.h): every frame equals the reference render
 * (line at LoThr with its "LO" tag, HiThr in the bottom tag), stays within ZM_CAP, flips A/B, at most
 * 15 redraws a second, nothing on stock firmware or off screen, the level is the output. */
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
static void eff_block(const float *u, float amp, int b)
{
    float buf[16];
    GmParams P;
    unsigned int on;
    int i;
    for (i = 0; i < 8; i++) {
        float t = (float)(b * 8 + i), a = amp;
        if (amp < 0.0f) { float ph = fmodf(t, 22050.0f); a = -amp * expf(-ph / 3000.0f); }
        buf[i] = a * sinf(0.05f * t); buf[i + 8] = buf[i];
    }
    gm_prepare(&P, &S, u);
    on = zm_begin(&S.zm, (void *)&S);
    gm_process(&S, &P, buf);
    if (on) gm_meter(&S, u);
    last_out = 0.0f;
    for (i = 0; i < 16; i++) last_out = fabsf(buf[i]) > last_out ? fabsf(buf[i]) : last_out;
}

/* custom_covers.meter_geometry("gooeymb") */
static void eff_geom(const float *u, int *by, RefTag *tags, int *nt)
{
    *by = (int)ZM_MID;
    snprintf(tags[0].text, sizeof tags[0].text, "L0 %.1f", (u[0] - 400) / 10.0);
    tags[0].xr = 126; tags[0].yb = *by - 2;
    snprintf(tags[1].text, sizeof tags[1].text, "HI %.1f", (u[1] - 400) / 10.0);
    tags[1].xr = 126; tags[1].yb = 62; *nt = 2;
}
static void eff_randk(float *u) { u[0] = (float)(rand() % 401); u[1] = (float)(rand() % 401); u[2] = (float)(rand() % 16); }
static void eff_line(const float *u, double *line, double *rpd) { *line = (u[0] - 400.0) / 10.0; *rpd = (u[2] + 1.0) / 2.0; }

int main(void)
{
    static const float udef[EFF_NK] = { 400.0f, 400.0f, 1.0f, 9.0f, 2.0f, 1.0f, 4.0f, 4.0f, 100.0f };
    float u[EFF_NK], pk;
    int g, b, r, w;

    zmt_run(udef);

    /* the shown level is the output after the gains; with Mix 0 the summed bands */
    for (g = 0; g < 2; g++) {
        zmt_reset(1u, 1u);
        memcpy(u, udef, sizeof u); u[0] = 200.0f; u[1] = 200.0f;
        if (g) u[8] = 0.0f;
        for (b = 0; b < 5512; b++) eff_block(u, 0.5f, b);
        pk = 0.0f;
        for (b = 5512; b < 5512 + 2000; b++) { eff_block(u, 0.5f, b); pk = last_out > pk ? last_out : pk; }
        r = (int)zm_row(&S.zm, S.zm.lv[(S.zm.head - 1u) & 255u]);
        w = ref_row(u, 20.0 * log10(pk));
        CHECK(r >= w - 1 && r <= w + 1, "%s: row %d, output peak %.3f wants %d", g ? "Mix 0" : "Thr -20", r, pk, w);
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
