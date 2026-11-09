/* gooeymb leaves its slot's arena safe for a stock delay or reverb (MIDISTOMP #60) */
#include "stereo_entry.h"
unsigned char _Fx_DLY_GooeyMB_Pic[4096];
#define ZM_FW_OK() 0u
#define ZM_POST() ((void)0)
#include "../src/custom/gooeymb/gooeymb.c"
#include "arena_safe.h"
int main(void)
{
    se_init(sizeof(GmState) + 64);
    return as_run(Fx_DLY_GooeyMB, sizeof(GmState), 13, 120000, "gooeymb");
}
