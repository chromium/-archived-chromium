
#include "SkFontHost.h"
#include <math.h>

static void build_power_table(uint8_t table[], float ee)
{
//    printf("------ build_power_table %g\n", ee);
    for (int i = 0; i < 256; i++)
    {
        float x = i / 255.f;
     //   printf(" %d %g", i, x);
        x = powf(x, ee);
     //   printf(" %g", x);
        int xx = SkScalarRound(SkFloatToScalar(x * 255));
     //   printf(" %d\n", xx);
        table[i] = SkToU8(xx);
    }
}

static bool gGammaIsBuilt;
static uint8_t gBlackGamma[256], gWhiteGamma[256];

#define ANDROID_BLACK_GAMMA     (1.4f)
#define ANDROID_WHITE_GAMMA     (1/1.4f)

void SkFontHost::GetGammaTables(const uint8_t* tables[2])
{
    // would be cleaner if these tables were precomputed and just linked in
    if (!gGammaIsBuilt)
    {
        build_power_table(gBlackGamma, ANDROID_BLACK_GAMMA);
        build_power_table(gWhiteGamma, ANDROID_WHITE_GAMMA);
        gGammaIsBuilt = true;
    }
    tables[0] = gBlackGamma;
    tables[1] = gWhiteGamma;
}

#define BLACK_GAMMA_THRESHOLD   0x40
#define WHITE_GAMMA_THRESHOLD   0xC0

int SkFontHost::ComputeGammaFlag(const SkPaint& paint)
{
    if (paint.getShader() == NULL)
    {
        SkColor c = paint.getColor();
        int r = SkColorGetR(c);
        int g = SkColorGetG(c);
        int b = SkColorGetB(c);
        int luminance = (r * 2 + g * 5 + b) >> 3;
        
        if (luminance <= BLACK_GAMMA_THRESHOLD)
        {
        //    printf("------ black gamma for [%d %d %d]\n", r, g, b);
            return SkScalerContext::kGammaForBlack_Flag;
        }
        if (luminance >= WHITE_GAMMA_THRESHOLD)
        {
        //    printf("------ white gamma for [%d %d %d]\n", r, g, b);
            return SkScalerContext::kGammaForWhite_Flag;
        }
    }
    return 0;
}

