#ifndef SkFloatBits_DEFINED
#define SkFloatBits_DEFINED

#include "SkTypes.h"

/** Convert a sign-bit int (i.e. float interpreted as int) into a 2s compliement
    int. This also converts -0 (0x80000000) to 0. Doing this to a float allows
    it to be compared using normal C operators (<, <=, etc.)
*/
static inline int32_t SkSignBitTo2sCompliment(int32_t x) {
    if (x < 0) {
        x &= 0x7FFFFFFF;
        x = -x;
    }
    return x;
}

#ifdef SK_CAN_USE_FLOAT

union SkFloatIntUnion {
    float   fFloat;
    int32_t fSignBitInt;
};

/** Return the float as a 2s compliment int. Just just be used to compare floats
    to each other or against positive float-bit-constants (like 0)
*/
static int32_t SkFloatAsInt(float x) {
    SkFloatIntUnion data;
    data.fFloat = x;
    return SkSignBitTo2sCompliment(data.fSignBitInt);
}

#endif

#ifdef SK_SCALAR_IS_FLOAT
    #define SkScalarAsInt(x)    SkFloatAsInt(x)
#else
    #define SkScalarAsInt(x)    (x)
#endif

#endif

